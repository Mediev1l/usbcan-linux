#include "FakeSocketApi.h"
#include "SocketCanTransport.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

#include <cstring>

using namespace usbcan;

namespace {

can_frame raw(canid_t id, std::initializer_list<quint8> data)
{
    can_frame f{};
    f.can_id = id;
    f.len = quint8(data.size());
    std::copy(data.begin(), data.end(), f.data);
    return f;
}

struct Fixture
{
    std::shared_ptr<FakeSocketApi::State> state = std::make_shared<FakeSocketApi::State>();
    SocketCanTransport transport{QStringLiteral("vcan9"), std::make_unique<FakeSocketApi>(state)};
};

} // namespace

TEST(SocketCanTransport, OpenBindsToInterface)
{
    Fixture fx;
    EXPECT_FALSE(fx.transport.isOpen());
    ASSERT_TRUE(fx.transport.open());
    EXPECT_TRUE(fx.transport.isOpen());
    EXPECT_EQ(fx.state->lastInterface, QStringLiteral("vcan9"));
    EXPECT_EQ(fx.state->boundIndex, 7);
    EXPECT_EQ(fx.transport.description(), QStringLiteral("vcan9"));

    const int fd = fx.state->fd;
    fx.transport.close();
    EXPECT_EQ(fx.state->closed, std::vector<int>{fd});
    EXPECT_FALSE(fx.transport.isOpen());
}

TEST(SocketCanTransport, OpenFailures)
{
    struct Case
    {
        void (*setup)(FakeSocketApi::State &);
        QString message;
        size_t closes;
    };
    const QList<Case> cases{
        {[](FakeSocketApi::State &s) { s.failSocket = true; },
         QStringLiteral("Cannot create CAN socket: ") + QString::fromLocal8Bit(std::strerror(EAFNOSUPPORT)), 0},
        {[](FakeSocketApi::State &s) { s.ifIndex = -1; },
         QStringLiteral("Unknown CAN interface vcan9: ") + QString::fromLocal8Bit(std::strerror(ENODEV)), 1},
        {[](FakeSocketApi::State &s) { s.bindResult = -1; },
         QStringLiteral("Cannot bind to vcan9: ") + QString::fromLocal8Bit(std::strerror(EADDRNOTAVAIL)), 1},
    };
    for (const Case &c : cases) {
        Fixture fx;
        c.setup(*fx.state);
        QSignalSpy error(&fx.transport, &ICanTransport::errorOccurred);
        EXPECT_FALSE(fx.transport.open());
        EXPECT_FALSE(fx.transport.isOpen());
        ASSERT_EQ(error.count(), 1);
        EXPECT_EQ(error[0][0].toString(), c.message);
        EXPECT_EQ(fx.state->closed.size(), c.closes);
    }
}

TEST(SocketCanTransport, SendSetsFlags)
{
    Fixture fx;
    EXPECT_FALSE(fx.transport.send(makeFrame(1, false, {})));
    ASSERT_TRUE(fx.transport.open());

    EXPECT_TRUE(fx.transport.send(makeFrame(0x18FF9341, true, QByteArray::fromHex("32ff"))));
    CanFrame rtr = makeFrame(0x123, false, {});
    rtr.rtr = true;
    rtr.dlc = 2;
    EXPECT_TRUE(fx.transport.send(rtr));

    ASSERT_EQ(fx.state->writes.size(), 2u);
    EXPECT_EQ(fx.state->writes[0].can_id, 0x18FF9341u | CAN_EFF_FLAG);
    EXPECT_EQ(fx.state->writes[0].len, 2);
    EXPECT_EQ(fx.state->writes[0].data[1], 0xFF);
    EXPECT_EQ(fx.state->writes[1].can_id, 0x123u | CAN_RTR_FLAG);
}

TEST(SocketCanTransport, SendFailureCloses)
{
    Fixture fx;
    ASSERT_TRUE(fx.transport.open());
    fx.state->writeResult = -1;
    QSignalSpy error(&fx.transport, &ICanTransport::errorOccurred);
    EXPECT_FALSE(fx.transport.send(makeFrame(1, false, {})));
    ASSERT_EQ(error.count(), 1);
    EXPECT_TRUE(error[0][0].toString().startsWith(QStringLiteral("Send on vcan9 failed: ")));
    EXPECT_FALSE(fx.transport.isOpen());
}

TEST(SocketCanTransport, ReadsAllPendingFrames)
{
    Fixture fx;
    ASSERT_TRUE(fx.transport.open());
    QSignalSpy frames(&fx.transport, &ICanTransport::framesReceived);
    fx.state->reads = {
        raw(0x18FF9341 | CAN_EFF_FLAG, {0x32, 0xFF}),
        raw(0x123, {0x01}),
        raw(0x7FF | CAN_RTR_FLAG, {0xAA, 0xBB}),
        raw(CAN_ERR_FLAG | 0x4, {}), // error frame: skipped
        FakeSocketApi::Short{},      // truncated read: skipped
    };
    fx.transport.readPending();

    ASSERT_EQ(frames.count(), 1);
    const auto list = frames[0][0].value<QList<CanFrame>>();
    ASSERT_EQ(list.size(), 3);
    EXPECT_EQ(list[0].id, 0x18FF9341u);
    EXPECT_TRUE(list[0].extended);
    EXPECT_EQ(list[0].payload(), QByteArray::fromHex("32ff"));
    EXPECT_EQ(list[1].id, 0x123u);
    EXPECT_FALSE(list[1].extended);
    EXPECT_TRUE(list[2].rtr);
    EXPECT_EQ(list[2].dlc, 2);
    EXPECT_EQ(list[2].data[0], 0); // remote frames carry no data
    EXPECT_GT(list[0].timestampUs, 0);

    fx.transport.readPending(); // nothing queued: no signal
    EXPECT_EQ(frames.count(), 1);
}

TEST(SocketCanTransport, NotifierTriggersRead)
{
    Fixture fx;
    ASSERT_TRUE(fx.transport.open());
    QSignalSpy frames(&fx.transport, &ICanTransport::framesReceived);
    fx.state->reads = {raw(0x1, {})};
    FakeSocketApi::signalReadable(fx.state->fd);
    EXPECT_TRUE(frames.wait(1000));
}

TEST(SocketCanTransport, ReadErrorCloses)
{
    Fixture fx;
    ASSERT_TRUE(fx.transport.open());
    QSignalSpy error(&fx.transport, &ICanTransport::errorOccurred);
    QSignalSpy frames(&fx.transport, &ICanTransport::framesReceived);
    fx.state->reads = {raw(0x1, {}), EWOULDBLOCK};
    fx.transport.readPending();
    EXPECT_EQ(error.count(), 0);
    EXPECT_EQ(frames.count(), 1);

    fx.state->reads = {raw(0x2, {}), ENETDOWN};
    fx.transport.readPending();
    ASSERT_EQ(error.count(), 1);
    EXPECT_TRUE(error[0][0].toString().startsWith(QStringLiteral("Read from vcan9 failed: ")));
    EXPECT_EQ(frames.count(), 2); // frames read before the error are still delivered
    EXPECT_FALSE(fx.transport.isOpen());
}
