#include "DeviceDiscovery.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace usbcan::discovery;

namespace {

void touch(const QString &path, const QByteArray &content = {})
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(content);
}

} // namespace

TEST(DeviceDiscovery, SerialPortsPreferStableNamesAndWaveshare)
{
    QTemporaryDir root;
    QDir(root.path()).mkpath(QStringLiteral("dev"));
    QDir(root.path()).mkpath(QStringLiteral("by-id"));
    const QString dev = root.filePath(QStringLiteral("dev"));
    const QString byId = root.filePath(QStringLiteral("by-id"));
    for (const char *name : {"ttyUSB0", "ttyUSB1", "ttyACM0", "random"})
        touch(dev + QLatin1Char('/') + QLatin1String(name));

    QFile::link(dev + QStringLiteral("/ttyUSB1"), byId + QStringLiteral("/usb-FTDI_FT232R-if00-port0"));
    QFile::link(dev + QStringLiteral("/ttyUSB0"), byId + QStringLiteral("/usb-1a86_USB_Serial-if00-port0"));
    QFile::link(dev + QStringLiteral("/gone"), byId + QStringLiteral("/plain_name"));

    const QList<SerialPort> ports = serialPorts(byId, dev);
    ASSERT_EQ(ports.size(), 4);
    EXPECT_EQ(ports[0], (SerialPort{byId + QStringLiteral("/usb-1a86_USB_Serial-if00-port0"),
                                    QStringLiteral("1a86 USB Serial (ttyUSB0)"), true}));
    // The rest keep alphabetical order.
    EXPECT_EQ(ports[1].label, QStringLiteral("plain name")); // dangling link
    EXPECT_EQ(ports[2].label, QStringLiteral("FTDI FT232R (ttyUSB1)"));
    EXPECT_FALSE(ports[2].likelyWaveshare);
    EXPECT_EQ(ports[3], (SerialPort{dev + QStringLiteral("/ttyACM0"), QStringLiteral("ttyACM0"), false}));
}

TEST(DeviceDiscovery, SerialPortsWithMissingDirectories)
{
    EXPECT_TRUE(serialPorts(QStringLiteral("/nonexistent/a"), QStringLiteral("/nonexistent/b")).isEmpty());
}

TEST(DeviceDiscovery, CanInterfacesByType)
{
    QTemporaryDir root;
    QDir d(root.path());
    for (const char *name : {"can0", "eth0", "vcan9", "notype"})
        d.mkpath(QLatin1String(name));
    touch(d.filePath(QStringLiteral("can0/type")), "280\n");
    touch(d.filePath(QStringLiteral("vcan9/type")), "280\n");
    touch(d.filePath(QStringLiteral("eth0/type")), "1\n");

    EXPECT_EQ(canInterfaces(root.path()), (QStringList{QStringLiteral("can0"), QStringLiteral("vcan9")}));
}
