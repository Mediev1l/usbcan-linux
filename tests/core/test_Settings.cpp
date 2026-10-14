#include "Settings.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace usbcan;

namespace {

struct Fixture
{
    QTemporaryDir dir;
    QSettings qs{dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat};
    Settings settings{&qs};
};

} // namespace

TEST(Settings, Defaults)
{
    Fixture fx;
    const ConnectionSettings c = fx.settings.connection();
    EXPECT_EQ(c.backend, Backend::Waveshare);
    EXPECT_EQ(c.port, QStringLiteral("/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0"));
    EXPECT_TRUE(c.interface.isEmpty());
    EXPECT_EQ(c.waveshare.bitrate, 250000);
    EXPECT_EQ(c.waveshare.mode, waveshare::Mode::Normal);
    EXPECT_EQ(c.waveshare.frameType, waveshare::FrameType::Extended);
    EXPECT_TRUE(fx.settings.libraryPath().endsWith(QStringLiteral("/library.canlib.json")));
    EXPECT_EQ(fx.settings.libraryPath(), Settings::defaultLibraryPath());
    EXPECT_TRUE(fx.settings.newIdsVisible());
    EXPECT_TRUE(fx.settings.hiddenKeys().isEmpty());
}

TEST(Settings, RoundTrip)
{
    Fixture fx;
    ConnectionSettings c;
    c.backend = Backend::SocketCan;
    c.port = QStringLiteral("/dev/ttyUSB3");
    c.interface = QStringLiteral("wscan0");
    c.waveshare.bitrate = 500000;
    c.waveshare.mode = waveshare::Mode::Silent;
    c.waveshare.frameType = waveshare::FrameType::Standard;
    fx.settings.setConnection(c);
    fx.settings.setLibraryPath(QStringLiteral("/tmp/x.json"));
    fx.settings.setNewIdsVisible(false);
    fx.settings.setHiddenKeys({frameKey(0x18FF9341, true), frameKey(0x1, false)});
    fx.settings.sync();

    QSettings reopened(fx.dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat);
    Settings s(&reopened);
    const ConnectionSettings back = s.connection();
    EXPECT_EQ(back.backend, Backend::SocketCan);
    EXPECT_EQ(back.port, c.port);
    EXPECT_EQ(back.interface, c.interface);
    EXPECT_EQ(back.waveshare.bitrate, 500000);
    EXPECT_EQ(back.waveshare.mode, waveshare::Mode::Silent);
    EXPECT_EQ(back.waveshare.frameType, waveshare::FrameType::Standard);
    EXPECT_EQ(s.libraryPath(), QStringLiteral("/tmp/x.json"));
    EXPECT_FALSE(s.newIdsVisible());
    EXPECT_EQ(s.hiddenKeys(), (QSet<quint64>{frameKey(0x18FF9341, true), frameKey(0x1, false)}));
}

TEST(Settings, ToleratesBadValues)
{
    Fixture fx;
    fx.qs.setValue(QStringLiteral("connection/mode"), 9);
    fx.qs.setValue(QStringLiteral("connection/backend"), 5);
    fx.qs.setValue(QStringLiteral("filters/hidden"), QStringList{QStringLiteral("zz"), QStringLiteral("1f")});
    EXPECT_EQ(fx.settings.connection().waveshare.mode, waveshare::Mode::LoopbackSilent);
    EXPECT_EQ(fx.settings.connection().backend, Backend::Waveshare);
    EXPECT_EQ(fx.settings.hiddenKeys(), QSet<quint64>{0x1F});
}
