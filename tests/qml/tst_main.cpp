#include "AppController.h"
#include "FakeTransport.h"
#include "HexInput.h"
#include "QmlRegistration.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QtQuickTest>

Q_IMPORT_QML_PLUGIN(UsbCanPlugin)

using namespace usbcan;

// Lets QML tests drive the fake adapter behind the real AppController.
class TestHelper : public QObject
{
    Q_OBJECT

public:
    TestHelper()
    {
        m_settings = std::make_unique<QSettings>(m_dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat);
        m_settings->setValue(QStringLiteral("library/path"), m_dir.filePath(QStringLiteral("lib.json")));
        AppController::Dependencies deps;
        deps.settings = m_settings.get();
        deps.sysNetDir = m_dir.path();
        deps.serialByIdDir = m_dir.path();
        deps.devDir = m_dir.path();
        deps.flushIntervalMs = 3600 * 1000;
        deps.transportFactory = [this](const ConnectionSettings &) {
            auto t = std::make_unique<FakeTransport>(&m_script);
            m_transport = t.get();
            return t;
        };
        m_app = std::make_unique<AppController>(deps);
    }

    AppController *app() const { return m_app.get(); }

    Q_INVOKABLE void reset()
    {
        m_app->disconnectDevice();
        m_app->setPaused(false);
        m_app->setTraceMode(TraceModel::Chronological);
        m_app->clearTrace();
        m_app->forgetIds();
        m_app->filterModel()->setHiddenKeys({});
        m_app->setNewIdsVisible(true);
        m_app->setFilterSearch({});
        m_app->trace()->setSearchText({});
        m_app->library()->setLibrary({});
        m_app->dismissError();
        m_script = FakeTransport::Script{};
    }

    Q_INVOKABLE bool connectFake() { return m_app->connectDevice(); }

    Q_INVOKABLE void receive(const QString &id, const QString &payload)
    {
        const auto bytes = hexinput::parsePayload(payload).bytes;
        const bool extended = hexinput::looksExtended(id);
        m_transport->receive({makeFrame(hexinput::parseId(id, extended).id, extended, bytes,
                                        Direction::Rx, nowUs())});
        m_app->flush();
    }

    Q_INVOKABLE void flush() { m_app->flush(); }
    Q_INVOKABLE int sentCount() const { return int(m_script.sent.size()); }
    Q_INVOKABLE QString lastSent() const
    {
        return m_script.sent.isEmpty() ? QString() : toCansend(m_script.sent.last());
    }

private:
    QTemporaryDir m_dir;
    std::unique_ptr<QSettings> m_settings;
    FakeTransport::Script m_script;
    QPointer<FakeTransport> m_transport;
    std::unique_ptr<AppController> m_app;
};

class Setup : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        // QML Settings (window geometry) must not touch the user's ~/.config.
        qputenv("XDG_CONFIG_HOME", m_config.path().toLocal8Bit());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QCoreApplication::setOrganizationName(QStringLiteral("usbcan-linux-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("usbcan-linux-qml-tests"));
        m_helper = std::make_unique<TestHelper>();
        registerQmlTypes(m_helper->app());
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QStringLiteral("qrc:/"));
        engine->rootContext()->setContextProperty(QStringLiteral("Helper"), m_helper.get());
    }

    void cleanupTestCase() { m_helper.reset(); }

private:
    QTemporaryDir m_config;
    std::unique_ptr<TestHelper> m_helper;
};

QUICK_TEST_MAIN_WITH_SETUP(usbcan_qml, Setup)

#include "tst_main.moc"
