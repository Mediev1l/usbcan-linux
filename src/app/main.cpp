#include "AppController.h"
#include "QmlRegistration.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QSettings>

Q_IMPORT_QML_PLUGIN(UsbCanPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("usbcan-linux"));
    QGuiApplication::setApplicationName(QStringLiteral("usbcan-linux"));
    QGuiApplication::setApplicationVersion(QStringLiteral(USBCAN_VERSION));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/usbcan-linux.svg")));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QSettings settings;
    usbcan::AppController::Dependencies deps;
    deps.settings = &settings;
    usbcan::AppController controller(deps);
    usbcan::registerQmlTypes(&controller);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/UsbCan/Main.qml")));
    return QGuiApplication::exec();
}
