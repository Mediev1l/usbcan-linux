#pragma once

#include "CyclicSender.h"
#include "FilterModel.h"
#include "LibraryModel.h"
#include "Settings.h"
#include "TraceFilterProxy.h"
#include "TraceModel.h"
#include "TransportFactory.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QVariantList>

namespace usbcan {

// Everything the QML UI binds to: connection, trace, filters, saved messages.
// The UI holds no logic of its own; every action is a Q_INVOKABLE here.
class AppController : public QObject
{
    Q_OBJECT

    // Connection
    Q_PROPERTY(int backend READ backend WRITE setBackend NOTIFY connectionSettingsChanged)
    Q_PROPERTY(QVariantList ports READ ports NOTIFY devicesChanged)
    Q_PROPERTY(QString port READ port WRITE setPort NOTIFY connectionSettingsChanged)
    Q_PROPERTY(QStringList interfaces READ interfaces NOTIFY devicesChanged)
    Q_PROPERTY(QString interfaceName READ interfaceName WRITE setInterfaceName NOTIFY connectionSettingsChanged)
    Q_PROPERTY(QVariantList bitrates READ bitrates CONSTANT)
    Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY connectionSettingsChanged)
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY connectionSettingsChanged)
    Q_PROPERTY(bool extendedFrames READ extendedFrames WRITE setExtendedFrames NOTIFY connectionSettingsChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString connectionLabel READ connectionLabel NOTIFY connectedChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString busStatus READ busStatus NOTIFY busStatusChanged)

    // Statistics
    Q_PROPERTY(double rxCount READ rxCount NOTIFY countersChanged)
    Q_PROPERTY(double txCount READ txCount NOTIFY countersChanged)
    Q_PROPERTY(double rxRate READ rxRate NOTIFY countersChanged)

    // Trace and filters
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(int traceMode READ traceMode WRITE setTraceMode NOTIFY traceModeChanged)
    Q_PROPERTY(usbcan::TraceFilterProxy *trace READ trace CONSTANT)
    Q_PROPERTY(usbcan::FilterModel *filterModel READ filterModel CONSTANT)
    Q_PROPERTY(QSortFilterProxyModel *filters READ filters CONSTANT)
    Q_PROPERTY(QString filterSearch READ filterSearch WRITE setFilterSearch NOTIFY filterSearchChanged)
    Q_PROPERTY(bool newIdsVisible READ newIdsVisible WRITE setNewIdsVisible NOTIFY newIdsVisibleChanged)

    // Saved messages
    Q_PROPERTY(usbcan::LibraryModel *library READ library CONSTANT)
    Q_PROPERTY(QString libraryPath READ libraryPath NOTIFY libraryPathChanged)
    // Saved messages currently being sent periodically.
    Q_PROPERTY(int runningCount READ runningCount NOTIFY runningCountChanged)

public:
    struct Dependencies
    {
        QSettings *settings = nullptr; // required, not owned
        TransportFactory transportFactory = &createTransport;
        QString serialByIdDir = QStringLiteral("/dev/serial/by-id");
        QString devDir = QStringLiteral("/dev");
        QString sysNetDir = QStringLiteral("/sys/class/net");
        int flushIntervalMs = 50;
        qsizetype traceCapacity = 200000;
    };

    explicit AppController(Dependencies deps, QObject *parent = nullptr);
    ~AppController() override;

    // --- connection -------------------------------------------------------
    int backend() const { return int(m_conn.backend); }
    void setBackend(int backend);
    QVariantList ports() const { return m_ports; }
    QString port() const { return m_conn.port; }
    void setPort(const QString &port);
    QStringList interfaces() const { return m_interfaces; }
    QString interfaceName() const { return m_conn.interface; }
    void setInterfaceName(const QString &name);
    QVariantList bitrates() const;
    int bitrate() const { return m_conn.waveshare.bitrate; }
    void setBitrate(int bitrate);
    int mode() const { return int(m_conn.waveshare.mode); }
    void setMode(int mode);
    bool extendedFrames() const;
    void setExtendedFrames(bool extended);
    bool connected() const { return m_transport != nullptr; }
    QString connectionLabel() const;
    QString errorText() const { return m_errorText; }
    QString busStatus() const { return m_busStatus; }

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE bool connectDevice();
    Q_INVOKABLE void disconnectDevice();
    Q_INVOKABLE void toggleConnection();
    Q_INVOKABLE void dismissError();

    // --- statistics ---------------------------------------------------------
    double rxCount() const { return double(m_rxCount); }
    double txCount() const { return double(m_txCount); }
    double rxRate() const { return m_rxRate; }
    // Recomputes rxRate; called once per second by a timer.
    void updateRates();

    // --- trace and filters ------------------------------------------------
    bool paused() const { return m_paused; }
    void setPaused(bool paused);
    int traceMode() const { return m_traceModel.mode(); }
    void setTraceMode(int mode);
    TraceFilterProxy *trace() { return &m_traceProxy; }
    TraceModel *traceModel() { return &m_traceModel; }
    FilterModel *filterModel() { return &m_filterModel; }
    QSortFilterProxyModel *filters() { return &m_filterProxy; }
    QString filterSearch() const { return m_filterSearch; }
    void setFilterSearch(const QString &text);
    bool newIdsVisible() const { return m_filterModel.newIdsVisible(); }
    void setNewIdsVisible(bool visible);

    // Moves buffered frames into the models; called every flushIntervalMs.
    void flush();

    Q_INVOKABLE void clearTrace();
    // Empties the ID list and unhides everything; IDs still on the bus reappear.
    Q_INVOKABLE void forgetIds();
    // Most recent frame of an identifier (filter list key):
    // {id, extended, payload, name}, or an empty map if never seen.
    Q_INVOKABLE QVariantMap lastFrame(quint64 key) const;
    // Trace row actions (rows of the `trace` proxy).
    Q_INVOKABLE int saveTraceRow(int row, const QString &name, const QString &comment, int periodMs);
    Q_INVOKABLE bool resendTraceRow(int row);
    Q_INVOKABLE void hideTraceRow(int row);
    Q_INVOKABLE QString cansendText(int row) const;
    Q_INVOKABLE QString traceRowName(int row) const;
    // ID ("18FF9341") and payload ("32 FF 0F") of a trace row, shown separately.
    Q_INVOKABLE QString traceRowId(int row) const;
    Q_INVOKABLE QString traceRowPayload(int row) const;

    // File dialogs hand over URLs; plain paths work too. Format from the extension.
    Q_INVOKABLE bool exportTrace(const QString &fileUrl);
    Q_INVOKABLE bool importTrace(const QString &fileUrl);

    // --- sending and saved messages ----------------------------------------
    LibraryModel *library() { return &m_library; }
    QString libraryPath() const { return m_libraryPath; }
    int runningCount() const { return m_cyclic.runningCount(); }

    // Empty string when valid, otherwise a message for the UI.
    Q_INVOKABLE QString validateId(const QString &text, bool extended) const;
    Q_INVOKABLE QString validatePayload(const QString &text) const;
    Q_INVOKABLE bool looksExtended(const QString &text) const;
    // Canonical spelling of typed input ("18ff9341" -> "18FF9341", "32ff" -> "32 FF");
    // invalid input is returned trimmed, unchanged.
    Q_INVOKABLE QString displayId(const QString &text, bool extended) const;
    Q_INVOKABLE QString displayPayload(const QString &text) const;

    Q_INVOKABLE bool sendFrame(const QString &idText, bool extended, const QString &payloadText);
    Q_INVOKABLE int saveMessage(const QString &name, const QString &idText, bool extended,
                                const QString &payloadText, int periodMs, const QString &comment);
    Q_INVOKABLE bool sendLibraryRow(int row);
    // Starts periodic sending (or sends once when the period is 0); stops if running.
    Q_INVOKABLE void toggleCyclic(int row);
    Q_INVOKABLE void stopAllCyclic();
    Q_INVOKABLE bool exportLibrary(const QString &fileUrl);
    Q_INVOKABLE bool importLibrary(const QString &fileUrl);

signals:
    void connectionSettingsChanged();
    void devicesChanged();
    void connectedChanged();
    void errorTextChanged();
    void busStatusChanged();
    void countersChanged();
    void pausedChanged();
    void traceModeChanged();
    void filterSearchChanged();
    void newIdsVisibleChanged();
    void libraryPathChanged();
    void runningCountChanged();

private:
    static QString toLocalPath(const QString &fileUrl);
    void setError(const QString &message);
    void onFramesReceived(const QList<usbcan::CanFrame> &frames);
    void onTransportError(const QString &message);
    bool transmit(CanFrame frame);
    void teardownTransport();
    void saveLibrary();
    void onLibraryChanged();
    QString nameFor(quint32 id, bool extended) const;
    CanFrame traceFrame(int proxyRow) const;

    Dependencies m_deps;
    Settings m_settings;
    ConnectionSettings m_conn;

    QPointer<ICanTransport> m_transport;
    QVariantList m_ports;
    QStringList m_interfaces;
    QString m_errorText;
    QString m_busStatus;

    TraceModel m_traceModel;
    TraceFilterProxy m_traceProxy;
    FilterModel m_filterModel;
    QSortFilterProxyModel m_filterProxy;
    QString m_filterSearch;
    LibraryModel m_library;
    CyclicSender m_cyclic;
    QString m_libraryPath;

    QList<CanFrame> m_pending;
    bool m_paused = false;
    quint64 m_rxCount = 0;
    quint64 m_txCount = 0;
    quint64 m_rxAtLastRate = 0;
    double m_rxRate = 0;
    QElapsedTimer m_rateClock;
    QTimer m_flushTimer;
    QTimer m_rateTimer;
};

} // namespace usbcan
