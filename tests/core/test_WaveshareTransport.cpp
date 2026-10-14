#include "FakeSerialDevice.h"
#include "WaveshareTransport.h"

#include <QFile>
#include <QSerialPort>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace usbcan;

namespace {

struct Fixture
{
    QTemporaryDir dir;
    QString port = dir.filePath(QStringLiteral("ttyUSB0"));
    FakeSerialDevice *device = nullptr; // owned by the transport once open
    bool failOpen = false;
    bool failWrite = false;
    WaveshareTransport transport{port, waveshare::Config{}, [this](const QString &) {
                                     auto d = std::make_unique<FakeSerialDevice>();
                                     d->failOpen = failOpen;
                                     d->failWrite = failWrite;
                                     device = d.get();
                                     return d;
                                 }};

    Fixture()
    {
        QFile f(port);
        f.open(QIODevice::WriteOnly); // the "device node" checked for presence
    }
};

} // namespace

TEST(WaveshareTransport, OpenSendsConfiguration)
{
    Fixture fx;
    EXPECT_FALSE(fx.transport.isOpen());
    ASSERT_TRUE(fx.transport.open());
    EXPECT_TRUE(fx.transport.isOpen());
    EXPECT_EQ(fx.device->written, waveshare::encodeConfig(waveshare::Config{}));
    EXPECT_EQ(fx.transport.description(), fx.port);
}

TEST(WaveshareTransport, ReceivesFramesAndStatus)
{
    Fixture fx;
    ASSERT_TRUE(fx.transport.open());
    QSignalSpy frames(&fx.transport, &ICanTransport::framesReceived);
    QSignalSpy status(&fx.transport, &ICanTransport::busStatus);

    const CanFrame a = makeFrame(0x18FF9341, true, QByteArray::fromHex("32ff"));
    const CanFrame b = makeFrame(0x123, false, QByteArray::fromHex("01"));
    QByteArray st = waveshare::encodeStatusRequest();
    st[3] = char(3);
    fx.device->inject(waveshare::encodeFrame(a) + st + waveshare::encodeFrame(b));

    ASSERT_EQ(frames.count(), 1);
    const auto list = frames[0][0].value<QList<CanFrame>>();
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list[0].id, a.id);
    EXPECT_EQ(list[1].id, b.id);
    EXPECT_GT(list[0].timestampUs, 0);
    ASSERT_EQ(status.count(), 1);
    EXPECT_EQ(status[0][0].toInt(), 3);

    fx.device->inject(QByteArray::fromHex("0102")); // garbage only: nothing emitted
    EXPECT_EQ(frames.count(), 1);
}

TEST(WaveshareTransport, SendAndStatusRequest)
{
    Fixture fx;
    const CanFrame f = makeFrame(0x18FF9341, true, QByteArray::fromHex("01"));
    EXPECT_FALSE(fx.transport.send(f));
    EXPECT_FALSE(fx.transport.requestStatus());

    ASSERT_TRUE(fx.transport.open());
    fx.device->written.clear();
    EXPECT_TRUE(fx.transport.send(f));
    EXPECT_EQ(fx.device->written, waveshare::encodeFrame(f));
    fx.device->written.clear();
    EXPECT_TRUE(fx.transport.requestStatus());
    EXPECT_EQ(fx.device->written, waveshare::encodeStatusRequest());
}

TEST(WaveshareTransport, UnsupportedBitrate)
{
    waveshare::Config c;
    c.bitrate = 120000;
    WaveshareTransport t(QStringLiteral("/dev/null"), c, [](const QString &) {
        ADD_FAILURE() << "device must not be created";
        return std::make_unique<FakeSerialDevice>();
    });
    QSignalSpy error(&t, &ICanTransport::errorOccurred);
    EXPECT_FALSE(t.open());
    ASSERT_EQ(error.count(), 1);
    EXPECT_EQ(error[0][0].toString(), QStringLiteral("Bitrate 120000 is not supported by the adapter"));
}

TEST(WaveshareTransport, OpenFailure)
{
    Fixture fx;
    fx.failOpen = true;
    QSignalSpy error(&fx.transport, &ICanTransport::errorOccurred);
    EXPECT_FALSE(fx.transport.open());
    EXPECT_FALSE(fx.transport.isOpen());
    ASSERT_EQ(error.count(), 1);
    EXPECT_EQ(error[0][0].toString(), QStringLiteral("Cannot open %1: Permission denied").arg(fx.port));
}

TEST(WaveshareTransport, WriteFailureClosesTheConnection)
{
    Fixture fx;
    fx.failWrite = true;
    QSignalSpy error(&fx.transport, &ICanTransport::errorOccurred);
    EXPECT_FALSE(fx.transport.open()); // the configuration write already fails
    EXPECT_FALSE(fx.transport.isOpen());
    ASSERT_EQ(error.count(), 1);
    EXPECT_EQ(error[0][0].toString(), QStringLiteral("Write to %1 failed: I/O error").arg(fx.port));

    fx.failWrite = false;
    ASSERT_TRUE(fx.transport.open());
    fx.device->failWrite = true;
    EXPECT_FALSE(fx.transport.send(makeFrame(1, false, {})));
    EXPECT_FALSE(fx.transport.isOpen());
}

TEST(WaveshareTransport, DetectsDisappearingPort)
{
    Fixture fx;
    fx.transport.setPresenceCheckInterval(5);
    ASSERT_TRUE(fx.transport.open());
    QSignalSpy error(&fx.transport, &ICanTransport::errorOccurred);
    EXPECT_FALSE(error.wait(30)); // still there: no error
    QFile::remove(fx.port);
    ASSERT_TRUE(error.wait(1000));
    EXPECT_EQ(error[0][0].toString(), QStringLiteral("Adapter %1 disappeared (unplugged?)").arg(fx.port));
    EXPECT_FALSE(fx.transport.isOpen());
}

TEST(WaveshareTransport, CloseIsIdempotent)
{
    Fixture fx;
    ASSERT_TRUE(fx.transport.open());
    fx.transport.close();
    fx.transport.close();
    EXPECT_FALSE(fx.transport.isOpen());
}

TEST(WaveshareTransport, RealSerialPortFactory)
{
    auto device = WaveshareTransport::createSerialPort(QStringLiteral("/dev/nonexistent-usbcan"));
    auto *serial = qobject_cast<QSerialPort *>(device.get());
    ASSERT_NE(serial, nullptr);
    EXPECT_EQ(serial->baudRate(), waveshare::kSerialBaudRate);
    EXPECT_EQ(serial->dataBits(), QSerialPort::Data8);
    EXPECT_EQ(serial->parity(), QSerialPort::NoParity);
    EXPECT_EQ(serial->stopBits(), QSerialPort::OneStop);
    EXPECT_EQ(serial->flowControl(), QSerialPort::NoFlowControl);

    // Default factory against a port that does not exist.
    WaveshareTransport t(QStringLiteral("/dev/nonexistent-usbcan"), waveshare::Config{});
    QSignalSpy error(&t, &ICanTransport::errorOccurred);
    EXPECT_FALSE(t.open());
    EXPECT_EQ(error.count(), 1);
}
