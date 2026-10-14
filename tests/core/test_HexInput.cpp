#include "HexInput.h"

#include <gtest/gtest.h>

using namespace usbcan::hexinput;

TEST(HexInputId, AcceptsCommonSpellings)
{
    for (const char *text : {"18ff9341", "18FF9341", "0x18FF9341", "  0X18ff9341  "}) {
        const auto r = parseId(QString::fromLatin1(text), true);
        EXPECT_TRUE(r.ok) << text;
        EXPECT_EQ(r.id, 0x18FF9341u) << text;
        EXPECT_TRUE(r.error.isEmpty());
    }
}

TEST(HexInputId, RejectsInvalid)
{
    EXPECT_EQ(parseId(QString(), true).error, QStringLiteral("ID is empty"));
    EXPECT_EQ(parseId(QStringLiteral("0x"), true).error, QStringLiteral("ID is empty"));
    EXPECT_EQ(parseId(QStringLiteral("12G"), true).error, QStringLiteral("ID must be hexadecimal"));
    EXPECT_EQ(parseId(QStringLiteral("123456789"), true).error, QStringLiteral("ID is too long"));
    EXPECT_FALSE(parseId(QStringLiteral("12G"), true).ok);
}

TEST(HexInputId, RangeDependsOnFrameType)
{
    EXPECT_TRUE(parseId(QStringLiteral("7FF"), false).ok);
    EXPECT_EQ(parseId(QStringLiteral("800"), false).error,
              QStringLiteral("Standard ID must be at most 7FF"));
    EXPECT_TRUE(parseId(QStringLiteral("1FFFFFFF"), true).ok);
    EXPECT_EQ(parseId(QStringLiteral("20000000"), true).error,
              QStringLiteral("Extended ID must be at most 1FFFFFFF"));
}

TEST(HexInputId, LooksExtended)
{
    EXPECT_FALSE(looksExtended(QStringLiteral("123")));
    EXPECT_FALSE(looksExtended(QStringLiteral("0x7FF")));
    EXPECT_TRUE(looksExtended(QStringLiteral("800")));
    EXPECT_TRUE(looksExtended(QStringLiteral("0123")));
    EXPECT_TRUE(looksExtended(QStringLiteral("18FF9341")));
}

TEST(HexInputPayload, AcceptsSeparatorsAndPrefixes)
{
    const QByteArray expected = QByteArray::fromHex("32FF0F");
    for (const char *text : {"32 FF 0F", "32ff0f", "32,ff,0f", "0x32 0xFF 0x0F", "32:FF-0F", " 32; ff 0f "}) {
        const auto r = parsePayload(QString::fromLatin1(text));
        EXPECT_TRUE(r.ok) << text;
        EXPECT_EQ(r.bytes, expected) << text;
    }
}

TEST(HexInputPayload, EmptyIsValid)
{
    const auto r = parsePayload(QStringLiteral("  "));
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(r.bytes.isEmpty());
}

TEST(HexInputPayload, RejectsInvalid)
{
    EXPECT_EQ(parsePayload(QStringLiteral("ZZ")).error, QStringLiteral("Payload must be hexadecimal bytes"));
    EXPECT_EQ(parsePayload(QStringLiteral("123")).error,
              QStringLiteral("Payload has an odd number of hex digits"));
    EXPECT_EQ(parsePayload(QStringLiteral("00 11 22 33 44 55 66 77 88")).error,
              QStringLiteral("Payload is longer than 8 bytes"));
    EXPECT_FALSE(parsePayload(QStringLiteral("ZZ")).ok);
}
