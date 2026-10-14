#include "J1939.h"

#include <gtest/gtest.h>

using namespace usbcan::j1939;

TEST(J1939, DecodesFramesSeenOnTheBench)
{
    const Id id = decode(0x18FF9341);
    EXPECT_EQ(id.priority, 6);
    EXPECT_EQ(id.edp, 0);
    EXPECT_EQ(id.dp, 0);
    EXPECT_EQ(id.pf, 0xFF);
    EXPECT_EQ(id.ps, 0x93);
    EXPECT_EQ(id.sa, 0x41);
    EXPECT_EQ(id.pgn, 0xFF93u);
    EXPECT_FALSE(id.pdu1);
    EXPECT_EQ(id.da, kGlobalAddress);
    EXPECT_TRUE(id.broadcast);
    EXPECT_EQ(pgnName(id.pgn), QStringLiteral("PROPB - Proprietary B"));
    EXPECT_EQ(pduFormatName(id), QStringLiteral("PDU2 (broadcast)"));
}

TEST(J1939, Pdu1HasDestinationAddress)
{
    const Id id = decode(0x18EA00F9); // Request from F9 to 00
    EXPECT_TRUE(id.pdu1);
    EXPECT_EQ(id.pgn, 0xEA00u);
    EXPECT_EQ(id.da, 0x00);
    EXPECT_EQ(id.sa, 0xF9);
    EXPECT_FALSE(id.broadcast);
    EXPECT_EQ(pgnName(id.pgn), QStringLiteral("RQST - Request"));
    EXPECT_EQ(pduFormatName(id), QStringLiteral("PDU1 (destination specific)"));

    EXPECT_TRUE(decode(0x18EAFFF9).broadcast); // PDU1 sent to the global address
}

TEST(J1939, PduFormatBoundary)
{
    EXPECT_TRUE(decode(0x00EF0000).pdu1);   // PF 239
    EXPECT_FALSE(decode(0x00F00000).pdu1);  // PF 240
    EXPECT_EQ(decode(0x00EF1200).pgn, 0xEF00u);
    EXPECT_EQ(decode(0x00F01200).pgn, 0xF012u);
}

TEST(J1939, DataPageAndExtendedDataPage)
{
    EXPECT_EQ(decode(0x19FF0000).dp, 1);
    EXPECT_EQ(decode(0x19FF0000).pgn, 0x1FF00u);
    EXPECT_EQ(decode(0x1A000000).edp, 1);
    EXPECT_EQ(decode(0x1A000000).pgn, 0x20000u);
}

TEST(J1939, PgnNames)
{
    EXPECT_EQ(pgnName(0xF004), QStringLiteral("EEC1 - Electronic Engine Controller 1"));
    EXPECT_EQ(pgnName(0x1EF00), QStringLiteral("PROPA2 - Proprietary A2"));
    EXPECT_EQ(pgnName(0x1FF93), QStringLiteral("PROPB - Proprietary B"));
    EXPECT_TRUE(pgnName(0x2FF00).isEmpty());
    EXPECT_TRUE(pgnName(0x1234).isEmpty());
}

TEST(J1939, BitFields)
{
    const auto pdu2 = idBitFields(0x18FF9341);
    ASSERT_EQ(pdu2.size(), 6);
    EXPECT_EQ(pdu2[0].label, QStringLiteral("Priority"));
    EXPECT_EQ(pdu2[0].bits, QStringLiteral("110"));
    EXPECT_EQ(pdu2[0].value, 6u);
    EXPECT_EQ(pdu2[3].bits, QStringLiteral("11111111"));
    EXPECT_EQ(pdu2[4].label, QStringLiteral("PS (GE)"));
    EXPECT_EQ(pdu2[5].bits, QStringLiteral("01000001"));

    EXPECT_EQ(idBitFields(0x18EA00F9)[4].label, QStringLiteral("PS (DA)"));
}
