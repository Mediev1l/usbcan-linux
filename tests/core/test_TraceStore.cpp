#include "TraceStore.h"

#include <gtest/gtest.h>

using namespace usbcan;

namespace {

CanFrame f(quint32 id, const char *hex, qint64 ms)
{
    return makeFrame(id, true, QByteArray::fromHex(hex), Direction::Rx, ms * 1000);
}

} // namespace

TEST(TraceStore, CapacityIsAtLeastOne)
{
    EXPECT_EQ(TraceStore(0).capacity(), 1);
    EXPECT_EQ(TraceStore(5).capacity(), 5);
}

TEST(TraceStore, StatsPerIdentifier)
{
    TraceStore s;
    s.append(f(0xA, "0102", 0));
    s.append(f(0xB, "", 50));
    s.append(f(0xA, "0103", 100));
    s.append(f(0xA, "010300", 300));

    EXPECT_EQ(s.size(), 4);
    EXPECT_EQ(s.keys(), (QList<quint64>{frameKey(0xA, true), frameKey(0xB, true)}));
    EXPECT_TRUE(s.contains(frameKey(0xA, true)));
    EXPECT_FALSE(s.contains(frameKey(0xA, false)));

    const IdStats &a = s.stats(frameKey(0xA, true));
    EXPECT_EQ(a.id, 0xAu);
    EXPECT_TRUE(a.extended);
    EXPECT_EQ(a.count, 3u);
    EXPECT_DOUBLE_EQ(a.periodMs, 0.8 * 100 + 0.2 * 200);
    EXPECT_DOUBLE_EQ(a.rateHz(), 1000.0 / 120.0);
    EXPECT_EQ(a.last.frame.dlc, 3);

    EXPECT_EQ(s.at(0).seq, 1u);
    EXPECT_EQ(s.at(3).seq, 4u);
    EXPECT_EQ(s.at(0).changedMask, 0);       // first frame of A
    EXPECT_EQ(s.at(2).changedMask, 0b10);    // byte 1 changed
    EXPECT_EQ(s.at(3).changedMask, 0b100);   // byte 2 appeared
    EXPECT_DOUBLE_EQ(s.stats(frameKey(0xB, true)).rateHz(), 0.0);
}

TEST(TraceStore, ShorterFrameMarksRemovedBytes)
{
    TraceStore s;
    s.append(f(0xA, "0102", 0));
    EXPECT_EQ(s.append(f(0xA, "01", 10)).changedMask, 0b10);
}

TEST(TraceStore, EvictsOldestButKeepsStats)
{
    TraceStore s(2);
    s.append(f(0xA, "01", 0));
    s.append(f(0xB, "02", 1));
    s.append(f(0xC, "03", 2));
    EXPECT_EQ(s.size(), 2);
    EXPECT_EQ(s.at(0).frame.id, 0xBu);
    EXPECT_EQ(s.keys().size(), 3);
    EXPECT_EQ(s.stats(frameKey(0xA, true)).count, 1u);
}

TEST(TraceStore, OverflowAndDrop)
{
    TraceStore s(3);
    s.append(f(0xA, "", 0));
    s.append(f(0xA, "", 1));
    EXPECT_EQ(s.overflowFor(1), 0);
    EXPECT_EQ(s.overflowFor(2), 1);
    EXPECT_EQ(s.overflowFor(10), 2);
    s.dropOldest(5);
    EXPECT_EQ(s.size(), 0);
    s.dropOldest(-1);
    EXPECT_EQ(s.size(), 0);
}

TEST(TraceStore, Clear)
{
    TraceStore s;
    s.append(f(0xA, "", 0));
    s.clear();
    EXPECT_EQ(s.size(), 0);
    EXPECT_TRUE(s.keys().isEmpty());
    EXPECT_FALSE(s.contains(frameKey(0xA, true)));
}
