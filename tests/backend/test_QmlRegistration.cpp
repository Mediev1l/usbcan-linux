#include "AppController.h"
#include "QmlRegistration.h"

#include <QQmlEngine>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace usbcan;

// Checked through the engine API rather than a QML snippet: `import QtQml` would
// pull in QtQml.Models, which is a separate (optional) distribution package.
TEST(QmlRegistration, ExposesControllerAsSingleton)
{
    QTemporaryDir dir;
    QSettings qs(dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat);
    qs.setValue(QStringLiteral("library/path"), dir.filePath(QStringLiteral("lib.json")));
    AppController::Dependencies deps;
    deps.settings = &qs;
    deps.sysNetDir = dir.path();
    AppController app(deps);
    registerQmlTypes(&app);

    const char *uri = "UsbCan.Backend";
    const int appId = qmlTypeId(uri, 1, 0, "App");
    ASSERT_GE(appId, 0);
    for (const char *type : {"TraceModel", "TraceFilterProxy", "FilterModel", "LibraryModel"})
        EXPECT_GE(qmlTypeId(uri, 1, 0, type), 0) << type;

    QQmlEngine engine;
    EXPECT_EQ(engine.singletonInstance<AppController *>(appId), &app);
}
