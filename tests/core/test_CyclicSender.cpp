#include "CyclicSender.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

using namespace usbcan;

namespace {

CanFrame frame(quint8 b)
{
    return makeFrame(0x18FF9341, true, QByteArray(1, char(b)), Direction::Tx);
}

} // namespace

TEST(CyclicSender, SendsImmediatelyAndPeriodically)
{
    CyclicSender s;
    QSignalSpy sent(&s, &CyclicSender::sendRequested);
    QSignalSpy running(&s, &CyclicSender::runningChanged);

    s.start(1, frame(1), 5);
    EXPECT_EQ(sent.count(), 1);
    EXPECT_TRUE(s.isRunning(1));
    EXPECT_EQ(s.runningCount(), 1);
    ASSERT_EQ(running.count(), 1);
    EXPECT_EQ(running[0][0].toULongLong(), 1u);
    EXPECT_EQ(running[0][1].toBool(), true);

    EXPECT_TRUE(sent.wait(1000));
    EXPECT_GE(sent.count(), 2);
    EXPECT_EQ(sent.last()[0].value<CanFrame>().data[0], 1);
}

TEST(CyclicSender, ZeroPeriodSendsOnce)
{
    CyclicSender s;
    QSignalSpy sent(&s, &CyclicSender::sendRequested);
    QSignalSpy running(&s, &CyclicSender::runningChanged);
    s.start(1, frame(1), 0);
    EXPECT_EQ(sent.count(), 1);
    EXPECT_FALSE(s.isRunning(1));
    EXPECT_EQ(running.count(), 0);
}

TEST(CyclicSender, RestartKeepsRunningStateQuiet)
{
    CyclicSender s;
    QSignalSpy running(&s, &CyclicSender::runningChanged);
    s.start(1, frame(1), 1000);
    s.start(1, frame(2), 1000);
    EXPECT_EQ(running.count(), 1);
    s.start(1, frame(3), 0); // restart as one-shot stops the job
    EXPECT_EQ(running.count(), 2);
    EXPECT_EQ(running[1][1].toBool(), false);
    EXPECT_FALSE(s.isRunning(1));
}

TEST(CyclicSender, UpdateChangesFrameAndPeriod)
{
    CyclicSender s;
    QSignalSpy sent(&s, &CyclicSender::sendRequested);
    s.start(1, frame(1), 1000);
    s.update(1, frame(9), 5);
    EXPECT_TRUE(sent.wait(1000));
    EXPECT_EQ(sent.last()[0].value<CanFrame>().data[0], 9);

    QSignalSpy running(&s, &CyclicSender::runningChanged);
    s.update(2, frame(1), 5); // unknown handle: ignored
    EXPECT_EQ(running.count(), 0);
    s.update(1, frame(1), 0); // period 0 stops it
    EXPECT_FALSE(s.isRunning(1));
    EXPECT_EQ(running.count(), 1);
}

TEST(CyclicSender, StopAndStopAll)
{
    CyclicSender s;
    QSignalSpy running(&s, &CyclicSender::runningChanged);
    s.start(1, frame(1), 1000);
    s.start(2, frame(2), 1000);
    s.stop(3);
    EXPECT_EQ(running.count(), 2);
    s.stop(1);
    EXPECT_EQ(running.count(), 3);
    s.stopAll();
    EXPECT_EQ(s.runningCount(), 0);
    EXPECT_EQ(running.count(), 4);
}
