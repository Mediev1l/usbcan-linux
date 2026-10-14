#include "CanFrame.h"

#include <gtest/gtest.h>

using namespace usbcan;

TEST(CanFrame, MakeFrameCopiesPayload)
{
    const CanFrame f = makeFrame(0x18FF9341, true, QByteArray::fromHex("32FF0F"), Direction::Tx, 42);
    EXPECT_EQ(f.id, 0x18FF9341u);
    EXPECT_TRUE(f.extended);
    EXPECT_FALSE(f.rtr);
    EXPECT_EQ(f.dlc, 3);
    EXPECT_EQ(f.payload(), QByteArray::fromHex("32FF0F"));
    EXPECT_EQ(f.dir, Direction::Tx);
    EXPECT_EQ(f.timestampUs, 42);
}

TEST(CanFrame, MakeFrameTruncatesToEightBytes)
{
    const CanFrame f = makeFrame(0x123, false, QByteArray(12, '\x11'));
    EXPECT_EQ(f.dlc, 8);
    EXPECT_EQ(f.payload(), QByteArray(8, '\x11'));
    EXPECT_EQ(f.dir, Direction::Rx);
}

TEST(CanFrame, PayloadIsClampedEvenWithBogusDlc)
{
    CanFrame f;
    f.dlc = 15;
    EXPECT_EQ(f.payload().size(), 8);
}

TEST(CanFrame, Keys)
{
    constexpr quint64 ext = frameKey(0x123, true);
    constexpr quint64 std = frameKey(0x123, false);
    EXPECT_NE(ext, std);
    EXPECT_EQ(keyId(ext), 0x123u);
    EXPECT_TRUE(keyExtended(ext));
    EXPECT_FALSE(keyExtended(std));
}

TEST(CanFrame, Formatting)
{
    EXPECT_EQ(formatId(0x18FF9341, true), QStringLiteral("18FF9341"));
    EXPECT_EQ(formatId(0x41, true), QStringLiteral("00000041"));
    EXPECT_EQ(formatId(0x7F, false), QStringLiteral("07F"));
    EXPECT_EQ(formatPayload(QByteArray::fromHex("0aff")), QStringLiteral("0A FF"));
    EXPECT_EQ(formatPayload(QByteArray()), QString());
    EXPECT_EQ(formatPayload(makeFrame(1, false, QByteArray::fromHex("01"))), QStringLiteral("01"));
}

TEST(CanFrame, Cansend)
{
    EXPECT_EQ(toCansend(makeFrame(0x18FF9341, true, QByteArray::fromHex("32ff"))),
              QStringLiteral("18FF9341#32FF"));
    CanFrame rtr = makeFrame(0x123, false, {});
    rtr.rtr = true;
    EXPECT_EQ(toCansend(rtr), QStringLiteral("123#R"));
}

TEST(CanFrame, NowIsEpochMicroseconds)
{
    EXPECT_GT(nowUs(), qint64(1'600'000'000) * 1'000'000);
}
