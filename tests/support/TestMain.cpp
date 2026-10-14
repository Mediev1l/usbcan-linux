#include <QCoreApplication>

#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("usbcan-linux-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("usbcan-linux-tests"));
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
