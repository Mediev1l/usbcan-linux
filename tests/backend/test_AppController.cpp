#include "AppController.h"
#include "FakeTransport.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QUrl>

#include <gtest/gtest.h>

using namespace usbcan;

namespace {

CanFrame rx(quint32 id, const char *hex = "", qint64 ms = 0)
{
    return makeFrame(id, true, QByteArray::fromHex(hex), Direction::Rx, ms * 1000);
}

void writeText(const QString &path, const QByteArray &text)
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(text);
}

struct Fixture
{
    QTemporaryDir dir;
    QSettings qs{dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat};
    FakeTransport::Script script;
    QList<ConnectionSettings> requested;
    QPointer<FakeTransport> transport;
    std::unique_ptr<AppController> app;

    explicit Fixture(bool withWscan = true, const QByteArray &libraryJson = {})
    {
        QDir d(dir.path());
        d.mkpath(QStringLiteral("dev"));
        d.mkpath(QStringLiteral("by-id"));
        writeText(dir.filePath(QStringLiteral("dev/ttyUSB0")), {});
        QFile::link(dir.filePath(QStringLiteral("dev/ttyUSB0")),
                    dir.filePath(QStringLiteral("by-id/usb-1a86_USB_Serial-if00-port0")));
        for (const char *name : {"vcan0", "wscan0"}) {
            if (!withWscan && QByteArray(name) == "wscan0")
                continue;
            d.mkpath(QStringLiteral("net/%1").arg(QLatin1String(name)));
            writeText(dir.filePath(QStringLiteral("net/%1/type").arg(QLatin1String(name))), "280");
        }
        qs.setValue(QStringLiteral("library/path"), libraryPath());
        if (!libraryJson.isEmpty())
            writeText(libraryPath(), libraryJson);

        AppController::Dependencies deps;
        deps.settings = &qs;
        deps.transportFactory = [this](const ConnectionSettings &c) {
            requested.append(c);
            auto t = std::make_unique<FakeTransport>(&script);
            transport = t.get();
            return t;
        };
        deps.serialByIdDir = dir.filePath(QStringLiteral("by-id"));
        deps.devDir = dir.filePath(QStringLiteral("dev"));
        deps.sysNetDir = dir.filePath(QStringLiteral("net"));
        deps.flushIntervalMs = 3600 * 1000; // tests flush explicitly
        deps.traceCapacity = 1000;
        app = std::make_unique<AppController>(deps);
    }

    QString libraryPath() const { return dir.filePath(QStringLiteral("lib.canlib.json")); }

    void connectAndReceive(const QList<CanFrame> &frames)
    {
        ASSERT_TRUE(app->connectDevice());
        transport->receive(frames);
        app->flush();
    }
};

} // namespace

TEST(AppController, DiscoversDevicesAndPrefersWscan)
{
    Fixture fx;
    ASSERT_EQ(fx.app->ports().size(), 1);
    const QVariantMap port = fx.app->ports()[0].toMap();
    EXPECT_TRUE(port.value(QStringLiteral("path")).toString().endsWith(QStringLiteral("usb-1a86_USB_Serial-if00-port0")));
    EXPECT_EQ(port.value(QStringLiteral("label")), QStringLiteral("1a86 USB Serial (ttyUSB0)"));
    EXPECT_EQ(port.value(QStringLiteral("likelyWaveshare")), true);
    EXPECT_EQ(fx.app->interfaces(), (QStringList{QStringLiteral("vcan0"), QStringLiteral("wscan0")}));
    EXPECT_EQ(fx.app->interfaceName(), QStringLiteral("wscan0"));

    QSignalSpy devices(fx.app.get(), &AppController::devicesChanged);
    fx.app->setInterfaceName(QStringLiteral("vcan0"));
    fx.app->refreshDevices(); // a chosen interface is kept
    EXPECT_EQ(devices.count(), 1);
    EXPECT_EQ(fx.app->interfaceName(), QStringLiteral("vcan0"));
}

TEST(AppController, FallsBackToFirstInterface)
{
    Fixture fx(false);
    EXPECT_EQ(fx.app->interfaceName(), QStringLiteral("vcan0"));
}

TEST(AppController, ConnectionSettingsArePersisted)
{
    Fixture fx;
    QSignalSpy changed(fx.app.get(), &AppController::connectionSettingsChanged);
    EXPECT_EQ(fx.app->bitrates().first().toInt(), 1000000);
    EXPECT_EQ(fx.app->bitrate(), 250000);
    EXPECT_TRUE(fx.app->extendedFrames());

    fx.app->setBackend(1);
    fx.app->setBackend(1);
    fx.app->setPort(QStringLiteral("/dev/ttyUSB5"));
    fx.app->setPort(QStringLiteral("/dev/ttyUSB5"));
    fx.app->setInterfaceName(QStringLiteral("wscan0")); // unchanged
    fx.app->setBitrate(500000);
    fx.app->setBitrate(500000);
    fx.app->setBitrate(120000); // unsupported
    fx.app->setMode(2);
    fx.app->setMode(2);
    fx.app->setMode(7);
    fx.app->setMode(-1);
    fx.app->setExtendedFrames(false);
    fx.app->setExtendedFrames(false);
    EXPECT_EQ(changed.count(), 5);

    EXPECT_EQ(fx.app->backend(), 1);
    EXPECT_EQ(fx.app->port(), QStringLiteral("/dev/ttyUSB5"));
    EXPECT_EQ(fx.app->bitrate(), 500000);
    EXPECT_EQ(fx.app->mode(), 2);
    EXPECT_FALSE(fx.app->extendedFrames());
    EXPECT_EQ(Settings(&fx.qs).connection().waveshare.bitrate, 500000);

    fx.app->setBackend(0);
    fx.app->setExtendedFrames(true);
    EXPECT_EQ(fx.app->backend(), 0);
    EXPECT_TRUE(fx.app->extendedFrames());
}

TEST(AppController, ConnectAndDisconnect)
{
    Fixture fx;
    QSignalSpy connected(fx.app.get(), &AppController::connectedChanged);
    EXPECT_EQ(fx.app->connectionLabel(), QStringLiteral("Disconnected"));

    ASSERT_TRUE(fx.app->connectDevice());
    EXPECT_TRUE(fx.app->connected());
    EXPECT_EQ(connected.count(), 1);
    ASSERT_EQ(fx.requested.size(), 1);
    EXPECT_EQ(fx.requested[0].waveshare.bitrate, 250000);
    EXPECT_EQ(fx.app->connectionLabel(), QStringLiteral("Waveshare fake0 @ 250 kbit/s"));

    fx.app->toggleConnection();
    EXPECT_FALSE(fx.app->connected());
    EXPECT_EQ(fx.script.closes, 1);
    EXPECT_EQ(connected.count(), 2);
    fx.app->disconnectDevice(); // already disconnected: quiet
    EXPECT_EQ(connected.count(), 2);

    fx.app->setBackend(1);
    fx.app->toggleConnection();
    EXPECT_TRUE(fx.app->connected());
    EXPECT_EQ(fx.app->connectionLabel(), QStringLiteral("SocketCAN fake0"));
}

TEST(AppController, ConnectFailure)
{
    Fixture fx;
    ASSERT_TRUE(fx.app->connectDevice());
    fx.script.openResult = false;
    QSignalSpy connected(fx.app.get(), &AppController::connectedChanged);
    QSignalSpy error(fx.app.get(), &AppController::errorTextChanged);

    EXPECT_FALSE(fx.app->connectDevice()); // reconnect drops the old connection
    EXPECT_FALSE(fx.app->connected());
    EXPECT_EQ(connected.count(), 1);
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("cannot open"));
    EXPECT_EQ(error.count(), 1);

    fx.app->dismissError();
    EXPECT_TRUE(fx.app->errorText().isEmpty());
}

TEST(AppController, ReceivingFillsTraceAndFilters)
{
    Fixture fx;
    QSignalSpy counters(fx.app.get(), &AppController::countersChanged);
    fx.connectAndReceive({rx(0x18FF9341, "01", 0), rx(0x18FF9241, "02", 1)});

    EXPECT_EQ(fx.app->rxCount(), 2);
    EXPECT_EQ(fx.app->trace()->count(), 2);
    EXPECT_EQ(fx.app->traceModel()->rowCount(), 2);
    EXPECT_EQ(fx.app->filterModel()->count(), 2);
    EXPECT_EQ(fx.app->filters()->rowCount(), 2);
    EXPECT_GE(counters.count(), 1);

    counters.clear();
    fx.app->flush(); // nothing pending
    EXPECT_EQ(counters.count(), 0);

    fx.app->updateRates();
    EXPECT_GE(fx.app->rxRate(), 0.0);
    fx.transport->receive({rx(1)});
    fx.app->updateRates();
    EXPECT_GT(fx.app->rxRate(), 0.0);
}

TEST(AppController, PauseKeepsCountingButNotDisplaying)
{
    Fixture fx;
    QSignalSpy paused(fx.app.get(), &AppController::pausedChanged);
    fx.app->setPaused(true);
    fx.app->setPaused(true);
    EXPECT_TRUE(fx.app->paused());
    EXPECT_EQ(paused.count(), 1);

    fx.connectAndReceive({rx(1), rx(2)});
    EXPECT_EQ(fx.app->rxCount(), 2);
    EXPECT_EQ(fx.app->trace()->count(), 0);
    EXPECT_TRUE(fx.app->sendFrame(QStringLiteral("123"), false, QString()));
    fx.app->flush();
    EXPECT_EQ(fx.app->trace()->count(), 0);
    EXPECT_EQ(fx.app->txCount(), 1);
}

TEST(AppController, BusStatusAndTransportErrors)
{
    Fixture fx;
    ASSERT_TRUE(fx.app->connectDevice());
    QSignalSpy bus(fx.app.get(), &AppController::busStatusChanged);
    fx.transport->reportStatus(1, 2);
    EXPECT_EQ(fx.app->busStatus(), QStringLiteral("RX errors 1, TX errors 2"));

    fx.transport->warn(QStringLiteral("glitch")); // connection still open
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("glitch"));
    EXPECT_TRUE(fx.app->connected());

    fx.app->saveMessage(QStringLiteral("A"), QStringLiteral("1"), true, QString(), 1000, QString());
    fx.app->toggleCyclic(0);
    EXPECT_TRUE(fx.app->library()->data(fx.app->library()->index(0), LibraryModel::RunningRole).toBool());

    QSignalSpy connected(fx.app.get(), &AppController::connectedChanged);
    fx.transport->dropConnection(QStringLiteral("Adapter gone"));
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("Adapter gone"));
    EXPECT_FALSE(fx.app->connected());
    EXPECT_EQ(connected.count(), 1);
    EXPECT_TRUE(fx.app->busStatus().isEmpty());
    EXPECT_GE(bus.count(), 2);
    EXPECT_FALSE(fx.app->library()->data(fx.app->library()->index(0), LibraryModel::RunningRole).toBool());
}

TEST(AppController, SendFrame)
{
    Fixture fx;
    EXPECT_FALSE(fx.app->sendFrame(QStringLiteral("18FF9341"), true, QStringLiteral("01")));
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("Not connected"));

    ASSERT_TRUE(fx.app->connectDevice());
    EXPECT_TRUE(fx.app->sendFrame(QStringLiteral("18FF9341"), true, QStringLiteral("32 FF")));
    ASSERT_EQ(fx.script.sent.size(), 1);
    EXPECT_EQ(fx.script.sent[0].id, 0x18FF9341u);
    EXPECT_EQ(fx.script.sent[0].dir, Direction::Tx);
    EXPECT_GT(fx.script.sent[0].timestampUs, 0);
    EXPECT_EQ(fx.app->txCount(), 1);
    fx.app->flush();
    EXPECT_EQ(fx.app->trace()->index(0, 0).data(TraceModel::DirectionRole), QStringLiteral("TX"));

    EXPECT_FALSE(fx.app->sendFrame(QStringLiteral("xyz"), true, QString()));
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("ID must be hexadecimal"));
    EXPECT_FALSE(fx.app->sendFrame(QStringLiteral("1"), true, QStringLiteral("1")));
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("Payload has an odd number of hex digits"));

    fx.script.sendResult = false;
    EXPECT_FALSE(fx.app->sendFrame(QStringLiteral("1"), true, QString()));
    EXPECT_FALSE(fx.app->connected());
    EXPECT_EQ(fx.app->txCount(), 1);
}

TEST(AppController, Validation)
{
    Fixture fx;
    EXPECT_TRUE(fx.app->validateId(QStringLiteral("7FF"), false).isEmpty());
    EXPECT_EQ(fx.app->validateId(QStringLiteral("800"), false), QStringLiteral("Standard ID must be at most 7FF"));
    EXPECT_TRUE(fx.app->validatePayload(QStringLiteral("01 02")).isEmpty());
    EXPECT_FALSE(fx.app->validatePayload(QStringLiteral("0")).isEmpty());
    EXPECT_TRUE(fx.app->looksExtended(QStringLiteral("18FF9341")));
    EXPECT_FALSE(fx.app->looksExtended(QStringLiteral("123")));
    EXPECT_EQ(fx.app->displayId(QStringLiteral(" 0x18ff9341 "), true), QStringLiteral("18FF9341"));
    EXPECT_EQ(fx.app->displayId(QStringLiteral("3"), false), QStringLiteral("003"));
    EXPECT_EQ(fx.app->displayId(QStringLiteral(" xyz "), true), QStringLiteral("xyz"));
    EXPECT_EQ(fx.app->displayPayload(QStringLiteral("32ff,0f")), QStringLiteral("32 FF 0F"));
    EXPECT_EQ(fx.app->displayPayload(QStringLiteral(" zz ")), QStringLiteral("zz"));
}

TEST(AppController, SavedMessagesAreAutosavedAndNamed)
{
    Fixture fx;
    EXPECT_EQ(fx.app->libraryPath(), fx.libraryPath());
    EXPECT_EQ(fx.app->saveMessage(QStringLiteral(" Engine "), QStringLiteral("18FF9341"), true,
                                  QStringLiteral("32 FF"), -3, QStringLiteral("c")),
              0);
    EXPECT_EQ(fx.app->saveMessage(QString(), QStringLiteral("123"), false, QString(), 0, QString()), 1);
    EXPECT_EQ(fx.app->library()->message(1).name, QStringLiteral("123"));
    EXPECT_EQ(fx.app->library()->message(0).periodMs, 0);

    MessageLibrary onDisk;
    ASSERT_TRUE(onDisk.load(fx.libraryPath()));
    EXPECT_EQ(onDisk.size(), 2);
    EXPECT_EQ(onDisk.at(0).name, QStringLiteral("Engine"));

    EXPECT_EQ(fx.app->saveMessage(QStringLiteral("x"), QStringLiteral("zz"), true, QString(), 0, QString()), -1);
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("ID must be hexadecimal"));
    EXPECT_EQ(fx.app->saveMessage(QStringLiteral("x"), QStringLiteral("1"), true, QStringLiteral("1"), 0, QString()), -1);
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("Payload has an odd number of hex digits"));

    // The trace shows names from the library.
    fx.connectAndReceive({rx(0x18FF9341, "01")});
    EXPECT_EQ(fx.app->trace()->index(0, 0).data(TraceModel::NameRole), QStringLiteral("Engine"));
    EXPECT_EQ(fx.app->traceRowName(0), QStringLiteral("Engine"));
}

TEST(AppController, LibraryLoadedAtStartup)
{
    MessageLibrary lib;
    SavedMessage m;
    m.name = QStringLiteral("Stored");
    m.id = 0x18FF9341;
    lib.add(m);
    Fixture fx(true, lib.toJson());
    ASSERT_EQ(fx.app->library()->count(), 1);
    EXPECT_EQ(fx.app->library()->message(0).name, QStringLiteral("Stored"));
    EXPECT_TRUE(fx.app->errorText().isEmpty());
}

TEST(AppController, BrokenLibraryAtStartupIsReported)
{
    Fixture fx(true, "not json");
    EXPECT_EQ(fx.app->library()->count(), 0);
    EXPECT_TRUE(fx.app->errorText().startsWith(QStringLiteral("Cannot load saved messages: ")));
}

TEST(AppController, LibrarySaveFailureIsReported)
{
    Fixture fx;
    QDir(fx.dir.path()).mkpath(QStringLiteral("lib.canlib.json")); // a directory in the way
    fx.app->saveMessage(QStringLiteral("A"), QStringLiteral("1"), true, QString(), 0, QString());
    EXPECT_TRUE(fx.app->errorText().startsWith(QStringLiteral("Cannot save messages to ")));
}

TEST(AppController, TraceRowActions)
{
    Fixture fx;
    fx.connectAndReceive({rx(0x18FF9341, "32ff"), rx(0x18FF9241, "01")});

    EXPECT_EQ(fx.app->cansendText(0), QStringLiteral("18FF9341#32FF"));
    EXPECT_EQ(fx.app->traceRowId(0), QStringLiteral("18FF9341"));
    EXPECT_EQ(fx.app->traceRowPayload(0), QStringLiteral("32 FF"));
    EXPECT_TRUE(fx.app->traceRowName(0).isEmpty());

    EXPECT_EQ(fx.app->saveTraceRow(0, QStringLiteral("Engine"), QStringLiteral("note"), 100), 0);
    const SavedMessage saved = fx.app->library()->message(0);
    EXPECT_EQ(saved.id, 0x18FF9341u);
    EXPECT_EQ(saved.payload, QByteArray::fromHex("32ff"));
    EXPECT_EQ(saved.periodMs, 100);
    EXPECT_EQ(saved.comment, QStringLiteral("note"));
    EXPECT_EQ(fx.app->saveTraceRow(1, QStringLiteral("  "), QString(), -1), 1);
    EXPECT_EQ(fx.app->library()->message(1).name, QStringLiteral("18FF9241"));
    EXPECT_EQ(fx.app->library()->message(1).periodMs, 0);

    EXPECT_TRUE(fx.app->resendTraceRow(0));
    EXPECT_EQ(fx.script.sent.last().id, 0x18FF9341u);

    fx.app->hideTraceRow(0);
    EXPECT_EQ(fx.app->trace()->count(), 1);
    EXPECT_FALSE(Settings(&fx.qs).hiddenKeys().isEmpty()); // persisted

    // Rows out of range.
    EXPECT_EQ(fx.app->saveTraceRow(9, QStringLiteral("x"), QString(), 0), -1);
    EXPECT_FALSE(fx.app->resendTraceRow(9));
    fx.app->hideTraceRow(9);
    EXPECT_TRUE(fx.app->cansendText(9).isEmpty());
    EXPECT_TRUE(fx.app->traceRowName(-1).isEmpty());
    EXPECT_TRUE(fx.app->traceRowId(9).isEmpty());
    EXPECT_TRUE(fx.app->traceRowPayload(9).isEmpty());
}

TEST(AppController, TraceModeSearchAndFilters)
{
    Fixture fx;
    QSignalSpy mode(fx.app.get(), &AppController::traceModeChanged);
    QSignalSpy search(fx.app.get(), &AppController::filterSearchChanged);
    QSignalSpy newIds(fx.app.get(), &AppController::newIdsVisibleChanged);
    fx.connectAndReceive({rx(0x18FF9341, "", 0), rx(0x18FF9341, "", 100), rx(0x18F00400, "", 0)});

    fx.app->setTraceMode(TraceModel::LatestPerId);
    EXPECT_EQ(fx.app->traceMode(), TraceModel::LatestPerId);
    EXPECT_EQ(mode.count(), 1);
    EXPECT_EQ(fx.app->trace()->count(), 2);

    fx.app->setFilterSearch(QStringLiteral("eec1"));
    fx.app->setFilterSearch(QStringLiteral("eec1"));
    EXPECT_EQ(search.count(), 1);
    EXPECT_EQ(fx.app->filterSearch(), QStringLiteral("eec1"));
    EXPECT_EQ(fx.app->filters()->rowCount(), 1);

    fx.app->setNewIdsVisible(false);
    fx.app->setNewIdsVisible(false);
    EXPECT_FALSE(fx.app->newIdsVisible());
    EXPECT_EQ(newIds.count(), 1);
    EXPECT_FALSE(Settings(&fx.qs).newIdsVisible());

    fx.app->clearTrace();
    EXPECT_EQ(fx.app->rxCount(), 0);
    EXPECT_EQ(fx.app->trace()->count(), 0);
    EXPECT_EQ(fx.app->filterModel()->count(), 2); // checkboxes survive a trace clear
    fx.app->filterModel()->setVisible(frameKey(0x18F00400, true), false);
    fx.app->forgetIds();
    EXPECT_EQ(fx.app->filterModel()->count(), 0);
    EXPECT_TRUE(fx.app->filterModel()->hiddenKeys().isEmpty());
    fx.transport->receive({rx(0x18F00400)}); // still on the bus: comes back as a new ID,
    fx.app->flush();                         // hidden because "show new IDs" is off here
    ASSERT_EQ(fx.app->filterModel()->count(), 1);
    EXPECT_FALSE(fx.app->filterModel()->accepts(frameKey(0x18F00400, true)));
    fx.app->setNewIdsVisible(true);
    fx.app->forgetIds();
    fx.transport->receive({rx(0x18F00400)});
    fx.app->flush();
    EXPECT_TRUE(fx.app->filterModel()->accepts(frameKey(0x18F00400, true)));
}

TEST(AppController, LastFrameOfAnId)
{
    Fixture fx;
    fx.app->saveMessage(QStringLiteral("Engine"), QStringLiteral("18FF9341"), true, QString(), 0, QString());
    fx.connectAndReceive({rx(0x18FF9341, "01"), rx(0x18FF9341, "32ff")});
    const QVariantMap m = fx.app->lastFrame(frameKey(0x18FF9341, true));
    EXPECT_EQ(m.value(QStringLiteral("id")), QStringLiteral("18FF9341"));
    EXPECT_EQ(m.value(QStringLiteral("extended")), true);
    EXPECT_EQ(m.value(QStringLiteral("payload")), QStringLiteral("32 FF"));
    EXPECT_EQ(m.value(QStringLiteral("name")), QStringLiteral("Engine"));
    EXPECT_TRUE(fx.app->lastFrame(frameKey(0x1, false)).isEmpty());
}

TEST(AppController, LibrarySendingAndCyclic)
{
    Fixture fx;
    fx.app->saveMessage(QStringLiteral("Fast"), QStringLiteral("1"), true, QStringLiteral("01"), 5, QString());
    fx.app->saveMessage(QStringLiteral("Once"), QStringLiteral("2"), true, QStringLiteral("02"), 0, QString());

    fx.app->toggleCyclic(0);
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("Not connected"));
    EXPECT_FALSE(fx.app->sendLibraryRow(5));
    EXPECT_FALSE(fx.app->sendLibraryRow(-1));

    ASSERT_TRUE(fx.app->connectDevice());
    EXPECT_TRUE(fx.app->sendLibraryRow(1));
    EXPECT_EQ(fx.script.sent.last().id, 2u);

    fx.app->toggleCyclic(1); // period 0: one shot
    EXPECT_EQ(fx.script.sent.size(), 2);
    EXPECT_FALSE(fx.app->library()->data(fx.app->library()->index(1), LibraryModel::RunningRole).toBool());

    QSignalSpy running(fx.app.get(), &AppController::runningCountChanged);
    fx.app->toggleCyclic(0);
    EXPECT_EQ(fx.app->runningCount(), 1);
    EXPECT_EQ(running.count(), 1);
    EXPECT_TRUE(fx.app->library()->data(fx.app->library()->index(0), LibraryModel::RunningRole).toBool());
    EXPECT_TRUE(QTest::qWaitFor([&] { return fx.script.sent.size() >= 5; }, 2000));

    // Editing a running message changes what is sent.
    fx.app->library()->setData(fx.app->library()->index(0), QStringLiteral("AA"), LibraryModel::PayloadRole);
    fx.script.sent.clear();
    EXPECT_TRUE(QTest::qWaitFor([&] { return !fx.script.sent.isEmpty(); }, 2000));
    EXPECT_EQ(fx.script.sent.last().data[0], 0xAA);

    fx.app->toggleCyclic(0); // stop
    EXPECT_FALSE(fx.app->library()->data(fx.app->library()->index(0), LibraryModel::RunningRole).toBool());
    fx.app->toggleCyclic(9); // ignored

    fx.app->toggleCyclic(0);
    fx.app->library()->remove(0); // removing a running message stops it
    fx.script.sent.clear();
    QTest::qWait(30);
    EXPECT_TRUE(fx.script.sent.isEmpty());

    fx.app->toggleCyclic(0); // "Once" is now row 0
    fx.app->stopAllCyclic();
}

TEST(AppController, ImportingLibraryStopsCyclicJobs)
{
    Fixture fx;
    ASSERT_TRUE(fx.app->connectDevice());
    fx.app->saveMessage(QStringLiteral("Fast"), QStringLiteral("1"), true, QString(), 5, QString());
    fx.app->toggleCyclic(0);

    const QString other = fx.dir.filePath(QStringLiteral("other.canlib.json"));
    ASSERT_TRUE(fx.app->exportLibrary(QUrl::fromLocalFile(other).toString()));
    ASSERT_TRUE(fx.app->importLibrary(other));
    EXPECT_EQ(fx.app->library()->count(), 1);
    fx.script.sent.clear();
    QTest::qWait(30);
    EXPECT_TRUE(fx.script.sent.isEmpty());

    EXPECT_FALSE(fx.app->exportLibrary(fx.dir.path()));
    EXPECT_TRUE(fx.app->errorText().startsWith(QStringLiteral("Cannot write ")));
    EXPECT_FALSE(fx.app->importLibrary(fx.dir.filePath(QStringLiteral("missing.json"))));
    EXPECT_TRUE(fx.app->errorText().startsWith(QStringLiteral("Cannot load ")));
}

TEST(AppController, ExportAndImportTrace)
{
    Fixture fx;
    fx.app->saveMessage(QStringLiteral("Engine"), QStringLiteral("18FF9341"), true, QString(), 0, QString());
    fx.connectAndReceive({rx(0x18FF9341, "32ff", 1000), rx(0x18FF9241, "01", 1100)});

    const QString csv = fx.dir.filePath(QStringLiteral("t.csv"));
    ASSERT_TRUE(fx.app->exportTrace(QUrl::fromLocalFile(csv).toString()));
    QFile f(csv);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    EXPECT_TRUE(f.readAll().contains("Engine"));

    fx.app->setBackend(1);
    const QString log = fx.dir.filePath(QStringLiteral("t.log"));
    ASSERT_TRUE(fx.app->exportTrace(log));
    QFile l(log);
    ASSERT_TRUE(l.open(QIODevice::ReadOnly));
    EXPECT_TRUE(l.readAll().contains(" wscan0 18FF9341#32FF"));
    fx.app->setBackend(0);
    ASSERT_TRUE(fx.app->exportTrace(fx.dir.filePath(QStringLiteral("t.asc"))));

    EXPECT_FALSE(fx.app->exportTrace(fx.dir.filePath(QStringLiteral("t.txt"))));
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("Unknown trace format: use .csv, .log or .asc"));
    EXPECT_FALSE(fx.app->exportTrace(fx.dir.filePath(QStringLiteral("no/such/dir/t.csv"))));
    EXPECT_TRUE(fx.app->errorText().startsWith(QStringLiteral("Cannot write ")));

    fx.app->clearTrace();
    ASSERT_TRUE(fx.app->importTrace(csv));
    EXPECT_EQ(fx.app->trace()->count(), 2);

    EXPECT_FALSE(fx.app->importTrace(fx.dir.filePath(QStringLiteral("t.txt"))));
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("Unknown trace format: use .csv or .log"));
    EXPECT_FALSE(fx.app->importTrace(fx.dir.filePath(QStringLiteral("missing.log"))));
    EXPECT_TRUE(fx.app->errorText().startsWith(QStringLiteral("Cannot read ")));
    EXPECT_FALSE(fx.app->importTrace(fx.dir.filePath(QStringLiteral("t.asc"))));
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("Importing Vector ASC files is not supported"));

    const QString partial = fx.dir.filePath(QStringLiteral("partial.log"));
    writeText(partial, "(1.0) can0 123#01\ngarbage\n");
    ASSERT_TRUE(fx.app->importTrace(partial));
    EXPECT_EQ(fx.app->errorText(), QStringLiteral("Imported 1 frames, skipped 1 invalid lines"));
}

TEST(AppController, DestroyWhileConnected)
{
    Fixture fx;
    ASSERT_TRUE(fx.app->connectDevice());
    fx.app.reset();
    EXPECT_EQ(fx.script.closes, 1);
}
