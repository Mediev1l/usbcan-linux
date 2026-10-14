#include "QmlRegistration.h"

#include "AppController.h"

#include <QQmlEngine>

namespace usbcan {

void registerQmlTypes(AppController *controller)
{
    constexpr auto uri = "UsbCan.Backend";
    const QString reason = QStringLiteral("Provided by App");
    qmlRegisterUncreatableType<TraceModel>(uri, 1, 0, "TraceModel", reason);
    qmlRegisterUncreatableType<TraceFilterProxy>(uri, 1, 0, "TraceFilterProxy", reason);
    qmlRegisterUncreatableType<FilterModel>(uri, 1, 0, "FilterModel", reason);
    qmlRegisterUncreatableType<LibraryModel>(uri, 1, 0, "LibraryModel", reason);
    qmlRegisterSingletonInstance(uri, 1, 0, "App", controller);
}

} // namespace usbcan
