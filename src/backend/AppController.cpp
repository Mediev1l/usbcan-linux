#include "AppController.h"

#include "DeviceDiscovery.h"
#include "HexInput.h"
#include "TraceIo.h"

#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

namespace usbcan {

AppController::AppController(Dependencies deps, QObject *parent)
    : QObject(parent)
    , m_deps(std::move(deps))
    , m_settings(m_deps.settings)
    , m_conn(m_settings.connection())
    , m_traceModel(m_deps.traceCapacity)
    , m_libraryPath(m_settings.libraryPath())
{
    const auto lookup = [this](quint32 id, bool extended) { return nameFor(id, extended); };
    m_traceModel.setNameLookup(lookup);
    m_filterModel.setNameLookup(lookup);

    m_traceProxy.setSourceModel(&m_traceModel);
    m_traceProxy.setFilterModel(&m_filterModel);
    m_filterProxy.setSourceModel(&m_filterModel);
    m_filterProxy.setFilterRole(FilterModel::SearchRole);
    m_filterProxy.setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_filterModel.setNewIdsVisible(m_settings.newIdsVisible());
    m_filterModel.setHiddenKeys(m_settings.hiddenKeys());
    connect(&m_filterModel, &FilterModel::filterChanged, this,
            [this] { m_settings.setHiddenKeys(m_filterModel.hiddenKeys()); });
    connect(&m_traceModel, &TraceModel::modeChanged, this, &AppController::traceModeChanged);

    // Saved messages: load, autosave, keep running jobs in sync.
    if (QFileInfo::exists(m_libraryPath)) {
        MessageLibrary lib;
        QString error;
        if (lib.load(m_libraryPath, &error))
            m_library.setLibrary(lib);
        else
            setError(QStringLiteral("Cannot load saved messages: %1").arg(error));
    }
    connect(&m_library, &LibraryModel::libraryChanged, this, &AppController::onLibraryChanged);
    connect(&m_library, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex &, int first, int last) {
                for (int row = first; row <= last; ++row)
                    m_cyclic.stop(m_library.handle(row));
            });
    connect(&m_library, &QAbstractItemModel::modelAboutToBeReset, &m_cyclic, &CyclicSender::stopAll);
    connect(&m_cyclic, &CyclicSender::runningChanged, &m_library, &LibraryModel::setRunning);
    connect(&m_cyclic, &CyclicSender::runningChanged, this, &AppController::runningCountChanged);
    connect(&m_cyclic, &CyclicSender::sendRequested, this,
            [this](const CanFrame &frame) { transmit(frame); });

    m_flushTimer.setInterval(m_deps.flushIntervalMs);
    connect(&m_flushTimer, &QTimer::timeout, this, &AppController::flush);
    m_flushTimer.start();
    m_rateTimer.setInterval(1000);
    connect(&m_rateTimer, &QTimer::timeout, this, &AppController::updateRates);
    m_rateTimer.start();
    m_rateClock.start();

    refreshDevices();
}

AppController::~AppController()
{
    m_cyclic.stopAll();
    teardownTransport();
}

// --- connection -------------------------------------------------------------

void AppController::setBackend(int backend)
{
    const Backend b = backend == int(Backend::SocketCan) ? Backend::SocketCan : Backend::Waveshare;
    if (b == m_conn.backend)
        return;
    m_conn.backend = b;
    m_settings.setConnection(m_conn);
    emit connectionSettingsChanged();
}

void AppController::setPort(const QString &port)
{
    if (port == m_conn.port)
        return;
    m_conn.port = port;
    m_settings.setConnection(m_conn);
    emit connectionSettingsChanged();
}

void AppController::setInterfaceName(const QString &name)
{
    if (name == m_conn.interface)
        return;
    m_conn.interface = name;
    m_settings.setConnection(m_conn);
    emit connectionSettingsChanged();
}

QVariantList AppController::bitrates() const
{
    QVariantList list;
    for (int b : waveshare::supportedBitrates())
        list.append(b);
    return list;
}

void AppController::setBitrate(int bitrate)
{
    if (bitrate == m_conn.waveshare.bitrate || !waveshare::bitrateCode(bitrate))
        return;
    m_conn.waveshare.bitrate = bitrate;
    m_settings.setConnection(m_conn);
    emit connectionSettingsChanged();
}

void AppController::setMode(int mode)
{
    if (mode == int(m_conn.waveshare.mode) || mode < 0 || mode > 3)
        return;
    m_conn.waveshare.mode = waveshare::Mode(mode);
    m_settings.setConnection(m_conn);
    emit connectionSettingsChanged();
}

bool AppController::extendedFrames() const
{
    return m_conn.waveshare.frameType == waveshare::FrameType::Extended;
}

void AppController::setExtendedFrames(bool extended)
{
    if (extended == extendedFrames())
        return;
    m_conn.waveshare.frameType = extended ? waveshare::FrameType::Extended
                                          : waveshare::FrameType::Standard;
    m_settings.setConnection(m_conn);
    emit connectionSettingsChanged();
}

QString AppController::connectionLabel() const
{
    if (!m_transport)
        return QStringLiteral("Disconnected");
    if (m_conn.backend == Backend::SocketCan)
        return QStringLiteral("SocketCAN %1").arg(m_transport->description());
    return QStringLiteral("Waveshare %1 @ %2 kbit/s")
        .arg(m_transport->description())
        .arg(m_conn.waveshare.bitrate / 1000);
}

void AppController::refreshDevices()
{
    m_ports.clear();
    for (const auto &p : discovery::serialPorts(m_deps.serialByIdDir, m_deps.devDir)) {
        m_ports.append(QVariantMap{{QStringLiteral("path"), p.path},
                                   {QStringLiteral("label"), p.label},
                                   {QStringLiteral("likelyWaveshare"), p.likelyWaveshare}});
    }
    m_interfaces = discovery::canInterfaces(m_deps.sysNetDir);
    emit devicesChanged();

    if (m_conn.interface.isEmpty() && !m_interfaces.isEmpty())
        setInterfaceName(m_interfaces.contains(QStringLiteral("wscan0")) ? QStringLiteral("wscan0")
                                                                          : m_interfaces.first());
}

bool AppController::connectDevice()
{
    disconnectDevice();
    dismissError();

    std::unique_ptr<ICanTransport> transport = m_deps.transportFactory(m_conn);
    connect(transport.get(), &ICanTransport::errorOccurred, this, &AppController::onTransportError);
    if (!transport->open())
        return false;

    m_transport = transport.release();
    m_transport->setParent(this);
    connect(m_transport, &ICanTransport::framesReceived, this, &AppController::onFramesReceived);
    connect(m_transport, &ICanTransport::busStatus, this, [this](int rx, int tx) {
        m_busStatus = QStringLiteral("RX errors %1, TX errors %2").arg(rx).arg(tx);
        emit busStatusChanged();
    });
    m_settings.setConnection(m_conn);
    emit connectedChanged();
    return true;
}

void AppController::disconnectDevice()
{
    m_cyclic.stopAll();
    const bool wasConnected = connected();
    teardownTransport();
    if (wasConnected)
        emit connectedChanged();
}

void AppController::toggleConnection()
{
    if (connected())
        disconnectDevice();
    else
        connectDevice();
}

void AppController::dismissError()
{
    setError({});
}

void AppController::setError(const QString &message)
{
    if (message == m_errorText)
        return;
    m_errorText = message;
    emit errorTextChanged();
}

void AppController::onTransportError(const QString &message)
{
    setError(message);
    if (m_transport && !m_transport->isOpen()) {
        m_cyclic.stopAll();
        teardownTransport();
        emit connectedChanged();
    }
}

void AppController::teardownTransport()
{
    if (!m_transport)
        return;
    ICanTransport *t = m_transport;
    m_transport = nullptr;
    t->disconnect(this);
    t->close();
    t->deleteLater();
    m_busStatus.clear();
    emit busStatusChanged();
}

void AppController::onFramesReceived(const QList<CanFrame> &frames)
{
    m_rxCount += quint64(frames.size());
    if (!m_paused)
        m_pending.append(frames);
}

bool AppController::transmit(CanFrame frame)
{
    if (!m_transport) {
        setError(QStringLiteral("Not connected"));
        return false;
    }
    frame.dir = Direction::Tx;
    frame.timestampUs = nowUs();
    if (!m_transport->send(frame))
        return false;
    ++m_txCount;
    if (!m_paused)
        m_pending.append(frame);
    emit countersChanged();
    return true;
}

// --- statistics ---------------------------------------------------------------

void AppController::updateRates()
{
    const qint64 elapsed = std::max<qint64>(1, m_rateClock.restart());
    m_rxRate = double(m_rxCount - m_rxAtLastRate) * 1000.0 / double(elapsed);
    m_rxAtLastRate = m_rxCount;
    emit countersChanged();
}

// --- trace and filters ----------------------------------------------------------

void AppController::setPaused(bool paused)
{
    if (paused == m_paused)
        return;
    m_paused = paused;
    emit pausedChanged();
}

void AppController::setTraceMode(int mode)
{
    m_traceModel.setMode(mode);
}

void AppController::setFilterSearch(const QString &text)
{
    if (text == m_filterSearch)
        return;
    m_filterSearch = text;
    m_filterProxy.setFilterFixedString(text.trimmed());
    emit filterSearchChanged();
}

void AppController::setNewIdsVisible(bool visible)
{
    if (visible == m_filterModel.newIdsVisible())
        return;
    m_filterModel.setNewIdsVisible(visible);
    m_settings.setNewIdsVisible(visible);
    emit newIdsVisibleChanged();
}

void AppController::flush()
{
    if (m_pending.isEmpty())
        return;
    const QList<CanFrame> frames = std::exchange(m_pending, {});
    m_traceModel.appendFrames(frames);
    m_filterModel.sync(m_traceModel.store(), frames);
    emit countersChanged();
}

void AppController::clearTrace()
{
    m_pending.clear();
    m_traceModel.clear();
    m_rxCount = 0;
    m_txCount = 0;
    m_rxAtLastRate = 0;
    emit countersChanged();
}

void AppController::forgetIds()
{
    // Rows come back only for identifiers that are still being received.
    m_filterModel.clear();
    m_filterModel.setHiddenKeys({});
}

QVariantMap AppController::lastFrame(quint64 key) const
{
    const TraceStore &store = m_traceModel.store();
    if (!store.contains(key))
        return {};
    const CanFrame &f = store.stats(key).last.frame;
    return {{QStringLiteral("id"), formatId(f.id, f.extended)},
            {QStringLiteral("extended"), f.extended},
            {QStringLiteral("payload"), formatPayload(f)},
            {QStringLiteral("name"), nameFor(f.id, f.extended)}};
}

CanFrame AppController::traceFrame(int proxyRow) const
{
    return m_traceModel.frameAt(m_traceProxy.sourceRow(proxyRow));
}

int AppController::saveTraceRow(int row, const QString &name, const QString &comment, int periodMs)
{
    if (m_traceProxy.sourceRow(row) < 0)
        return -1;
    const CanFrame f = traceFrame(row);
    SavedMessage m;
    m.name = name.trimmed().isEmpty() ? formatId(f.id, f.extended) : name.trimmed();
    m.id = f.id;
    m.extended = f.extended;
    m.payload = f.payload();
    m.periodMs = std::max(0, periodMs);
    m.comment = comment;
    return m_library.add(m);
}

bool AppController::resendTraceRow(int row)
{
    if (m_traceProxy.sourceRow(row) < 0)
        return false;
    return transmit(traceFrame(row));
}

void AppController::hideTraceRow(int row)
{
    if (m_traceProxy.sourceRow(row) < 0)
        return;
    const CanFrame f = traceFrame(row);
    m_filterModel.setVisible(frameKey(f.id, f.extended), false);
}

QString AppController::cansendText(int row) const
{
    if (m_traceProxy.sourceRow(row) < 0)
        return {};
    return toCansend(traceFrame(row));
}

QString AppController::traceRowName(int row) const
{
    if (m_traceProxy.sourceRow(row) < 0)
        return {};
    const CanFrame f = traceFrame(row);
    return nameFor(f.id, f.extended);
}

QString AppController::traceRowId(int row) const
{
    if (m_traceProxy.sourceRow(row) < 0)
        return {};
    const CanFrame f = traceFrame(row);
    return formatId(f.id, f.extended);
}

QString AppController::traceRowPayload(int row) const
{
    if (m_traceProxy.sourceRow(row) < 0)
        return {};
    return formatPayload(traceFrame(row));
}

QString AppController::toLocalPath(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    return url.isLocalFile() ? url.toLocalFile() : fileUrl;
}

bool AppController::exportTrace(const QString &fileUrl)
{
    const QString path = toLocalPath(fileUrl);
    const auto format = traceio::formatForPath(path);
    if (!format) {
        setError(QStringLiteral("Unknown trace format: use .csv, .log or .asc"));
        return false;
    }
    const auto nameLookup = [this](const CanFrame &f) { return nameFor(f.id, f.extended); };
    const QString iface = m_conn.backend == Backend::SocketCan ? m_conn.interface : QStringLiteral("can0");
    QString error;
    if (!traceio::writeFile(path, traceio::exportText(m_traceModel.frames(), *format, nameLookup, iface),
                            &error)) {
        setError(QStringLiteral("Cannot write %1: %2").arg(path, error));
        return false;
    }
    return true;
}

bool AppController::importTrace(const QString &fileUrl)
{
    const QString path = toLocalPath(fileUrl);
    const auto format = traceio::formatForPath(path);
    if (!format) {
        setError(QStringLiteral("Unknown trace format: use .csv or .log"));
        return false;
    }
    QString error;
    const auto text = traceio::readFile(path, &error);
    if (!text) {
        setError(QStringLiteral("Cannot read %1: %2").arg(path, error));
        return false;
    }
    const auto result = traceio::importText(*text, *format);
    if (!result.ok) {
        setError(result.error);
        return false;
    }
    m_pending.append(result.frames);
    flush();
    if (result.skippedLines > 0)
        setError(QStringLiteral("Imported %1 frames, skipped %2 invalid lines")
                     .arg(result.frames.size())
                     .arg(result.skippedLines));
    return true;
}

// --- sending and saved messages ---------------------------------------------------

QString AppController::validateId(const QString &text, bool extended) const
{
    return hexinput::parseId(text, extended).error;
}

QString AppController::validatePayload(const QString &text) const
{
    return hexinput::parsePayload(text).error;
}

bool AppController::looksExtended(const QString &text) const
{
    return hexinput::looksExtended(text);
}

QString AppController::displayId(const QString &text, bool extended) const
{
    const auto id = hexinput::parseId(text, extended);
    return id.ok ? formatId(id.id, extended) : text.trimmed();
}

QString AppController::displayPayload(const QString &text) const
{
    const auto payload = hexinput::parsePayload(text);
    return payload.ok ? formatPayload(payload.bytes) : text.trimmed();
}

bool AppController::sendFrame(const QString &idText, bool extended, const QString &payloadText)
{
    const auto id = hexinput::parseId(idText, extended);
    const auto payload = hexinput::parsePayload(payloadText);
    if (!id.ok || !payload.ok) {
        setError(id.ok ? payload.error : id.error);
        return false;
    }
    return transmit(makeFrame(id.id, extended, payload.bytes));
}

int AppController::saveMessage(const QString &name, const QString &idText, bool extended,
                               const QString &payloadText, int periodMs, const QString &comment)
{
    const auto id = hexinput::parseId(idText, extended);
    const auto payload = hexinput::parsePayload(payloadText);
    if (!id.ok || !payload.ok) {
        setError(id.ok ? payload.error : id.error);
        return -1;
    }
    SavedMessage m;
    m.name = name.trimmed().isEmpty() ? formatId(id.id, extended) : name.trimmed();
    m.id = id.id;
    m.extended = extended;
    m.payload = payload.bytes;
    m.periodMs = std::max(0, periodMs);
    m.comment = comment;
    return m_library.add(m);
}

bool AppController::sendLibraryRow(int row)
{
    if (row < 0 || row >= m_library.rowCount())
        return false;
    return transmit(m_library.message(row).toFrame());
}

void AppController::toggleCyclic(int row)
{
    if (row < 0 || row >= m_library.rowCount())
        return;
    const quint64 handle = m_library.handle(row);
    if (m_cyclic.isRunning(handle)) {
        m_cyclic.stop(handle);
        return;
    }
    if (!m_transport) {
        setError(QStringLiteral("Not connected"));
        return;
    }
    const SavedMessage m = m_library.message(row);
    m_cyclic.start(handle, m.toFrame(), m.periodMs);
}

void AppController::stopAllCyclic()
{
    m_cyclic.stopAll();
}

bool AppController::exportLibrary(const QString &fileUrl)
{
    const QString path = toLocalPath(fileUrl);
    QString error;
    if (!m_library.library().save(path, &error)) {
        setError(QStringLiteral("Cannot write %1: %2").arg(path, error));
        return false;
    }
    return true;
}

bool AppController::importLibrary(const QString &fileUrl)
{
    const QString path = toLocalPath(fileUrl);
    MessageLibrary lib;
    QString error;
    if (!lib.load(path, &error)) {
        setError(QStringLiteral("Cannot load %1: %2").arg(path, error));
        return false;
    }
    m_library.setLibrary(lib);
    return true;
}

void AppController::onLibraryChanged()
{
    saveLibrary();
    m_traceModel.refreshNames();
    m_filterModel.refreshNames();
    // Running jobs follow edits of their message.
    for (int row = 0; row < m_library.rowCount(); ++row) {
        const SavedMessage m = m_library.message(row);
        m_cyclic.update(m_library.handle(row), m.toFrame(), m.periodMs);
    }
}

void AppController::saveLibrary()
{
    QString error;
    if (!m_library.library().save(m_libraryPath, &error))
        setError(QStringLiteral("Cannot save messages to %1: %2").arg(m_libraryPath, error));
}

QString AppController::nameFor(quint32 id, bool extended) const
{
    return m_library.nameFor(id, extended);
}

} // namespace usbcan
