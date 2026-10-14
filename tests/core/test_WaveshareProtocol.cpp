#include "WaveshareProtocol.h"

#include <gtest/gtest.h>

using namespace usbcan;
using namespace usbcan::waveshare;
using Kind = Parser::Event::Kind;

namespace {

QByteArray hex(const char *text)
{
    return QByteArray::fromHex(QByteArray(text).replace(' ', ""));
}

QList<CanFrame> framesOf(const QList<Parser::Event> &events)
{
    QList<CanFrame> frames;
    for (const auto &e : events) {
        if (e.kind == Kind::Frame)
            frames.append(e.frame);
    }
    return frames;
}

} // namespace

TEST(WaveshareConfig, MatchesPacketVerifiedOnHardware)
{
    // 250k, extended, normal, retransmission on: what USBCAN V2.12 sends.
    EXPECT_EQ(encodeConfig(Config{}), hex("aa 55 12 05 02 00000000 00000000 00 00 00000000 19"));
}

TEST(WaveshareConfig, EncodesAllFields)
{
    Config c;
    c.bitrate = 1000000;
    c.mode = Mode::Silent;
    c.frameType = FrameType::Standard;
    c.sendOnce = true;
    const QByteArray p = encodeConfig(c);
    ASSERT_EQ(p.size(), kControlPacketSize);
    EXPECT_EQ(quint8(p[3]), 0x01);
    EXPECT_EQ(quint8(p[4]), 0x01);
    EXPECT_EQ(quint8(p[13]), 0x02);
    EXPECT_EQ(quint8(p[14]), 0x01);
    EXPECT_EQ(quint8(p[19]), quint8(0x12 + 0x01 + 0x01 + 0x02 + 0x01));
}

TEST(WaveshareConfig, UnsupportedBitrateGivesEmptyPacket)
{
    Config c;
    c.bitrate = 120000;
    EXPECT_TRUE(encodeConfig(c).isEmpty());
}

TEST(WaveshareConfig, BitrateTable)
{
    const QList<int> &rates = supportedBitrates();
    ASSERT_EQ(rates.size(), 12);
    EXPECT_EQ(rates.first(), 1000000);
    EXPECT_EQ(rates.last(), 5000);
    EXPECT_EQ(bitrateCode(250000), quint8(0x05));
    EXPECT_EQ(bitrateCode(5000), quint8(0x0C));
    EXPECT_FALSE(bitrateCode(123).has_value());
}

TEST(WaveshareConfig, StatusRequest)
{
    EXPECT_EQ(encodeStatusRequest(), hex("aa 55 04 00000000000000000000000000000000 04"));
}

TEST(WaveshareFrame, EncodesExtended)
{
    const CanFrame f = makeFrame(0x18FF9341, true, hex("32 ff ff ff ff ff 0f ff"));
    EXPECT_EQ(encodeFrame(f), hex("aa e8 41 93 ff 18 32 ff ff ff ff ff 0f ff 55"));
}

TEST(WaveshareFrame, EncodesStandard)
{
    EXPECT_EQ(encodeFrame(makeFrame(0x123, false, hex("01 02"))), hex("aa c2 23 01 01 02 55"));
}

TEST(WaveshareFrame, EncodesRemoteWithZeroData)
{
    CanFrame f = makeFrame(0x123, false, {});
    f.rtr = true;
    f.dlc = 2;
    f.data[0] = 0x99;
    EXPECT_EQ(encodeFrame(f), hex("aa d2 23 01 00 00 55"));
}

TEST(WaveshareFrame, ClampsDlc)
{
    CanFrame f = makeFrame(0x1, false, QByteArray(8, '\x01'));
    f.dlc = 12;
    EXPECT_EQ(encodeFrame(f).size(), 2 + 2 + 8 + 1);
}

class WaveshareRoundTrip : public ::testing::TestWithParam<std::tuple<int, bool, bool>>
{
};

TEST_P(WaveshareRoundTrip, ParserDecodesEncodedFrame)
{
    const auto [dlc, extended, rtr] = GetParam();
    CanFrame f = makeFrame(extended ? 0x1ABCDEF1 : 0x5A5, extended, QByteArray(dlc, '\x3C'));
    f.rtr = rtr;
    if (rtr)
        f.data.fill(0);

    Parser parser;
    const auto events = parser.feed(encodeFrame(f), 77);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].kind, Kind::Frame);
    f.timestampUs = 77;
    EXPECT_EQ(events[0].frame, f);
    EXPECT_EQ(parser.pendingBytes(), 0);
    EXPECT_EQ(parser.droppedBytes(), 0u);
}

INSTANTIATE_TEST_SUITE_P(AllShapes, WaveshareRoundTrip,
                         ::testing::Combine(::testing::Range(0, 9), ::testing::Bool(), ::testing::Bool()));

TEST(WaveshareParser, SplitAtEveryOffset)
{
    const QByteArray stream = encodeFrame(makeFrame(0x18FF9341, true, hex("32ffffffffff0fff")))
        + encodeFrame(makeFrame(0x18FF9241, true, hex("00f0ffff0102c3ff")));
    for (int cut = 0; cut <= stream.size(); ++cut) {
        Parser parser;
        auto events = parser.feed(stream.left(cut), 1);
        events += parser.feed(stream.mid(cut), 2);
        const auto frames = framesOf(events);
        ASSERT_EQ(frames.size(), 2) << "cut at " << cut;
        EXPECT_EQ(frames[0].id, 0x18FF9341u);
        EXPECT_EQ(frames[1].id, 0x18FF9241u);
    }
}

TEST(WaveshareParser, SkipsGarbageAndCountsIt)
{
    Parser parser;
    const auto events = parser.feed(hex("13 37") + encodeFrame(makeFrame(0x1, false, hex("aa"))), 0);
    ASSERT_EQ(framesOf(events).size(), 1);
    EXPECT_EQ(framesOf(events)[0].data[0], 0xAA);
    EXPECT_EQ(parser.droppedBytes(), 2u);

    EXPECT_TRUE(parser.feed(hex("01 02 03"), 0).isEmpty());
    EXPECT_EQ(parser.pendingBytes(), 0);
    EXPECT_EQ(parser.droppedBytes(), 5u);
}

TEST(WaveshareParser, ResyncsAfterTruncatedFrame)
{
    // A frame cut short, then a complete one: the complete one must survive.
    const QByteArray good = encodeFrame(makeFrame(0x18FF9241, true, hex("00f0ffff0102c3ff")));
    Parser parser;
    const auto frames = framesOf(parser.feed(good.left(5) + good, 0));
    ASSERT_EQ(frames.size(), 1);
    EXPECT_EQ(frames[0].id, 0x18FF9241u);
}

TEST(WaveshareParser, RejectsBadTypeByteAndOversizedDlc)
{
    Parser parser;
    // 0xAA 0x12: not a frame type; 0xAA 0xCF: DLC 15.
    const auto frames = framesOf(parser.feed(hex("aa 12 aa cf") + encodeFrame(makeFrame(0x7, false, {})), 0));
    ASSERT_EQ(frames.size(), 1);
    EXPECT_EQ(frames[0].id, 0x7u);
}

TEST(WaveshareParser, ParsesStatusPackets)
{
    QByteArray status = encodeStatusRequest();
    status[3] = char(12);
    status[4] = char(34);
    Parser parser;
    const auto events = parser.feed(status, 0);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].kind, Kind::Status);
    EXPECT_EQ(events[0].status.rxErrors, 12);
    EXPECT_EQ(events[0].status.txErrors, 34);
}

TEST(WaveshareParser, WaitsForCompletePackets)
{
    Parser parser;
    EXPECT_TRUE(parser.feed(hex("aa"), 0).isEmpty());
    EXPECT_EQ(parser.pendingBytes(), 1);
    EXPECT_TRUE(parser.feed(hex("55 04 00"), 0).isEmpty()); // partial status
    EXPECT_EQ(parser.pendingBytes(), 4);
    parser.reset();
    EXPECT_EQ(parser.pendingBytes(), 0);
    EXPECT_TRUE(parser.feed(hex("aa e8 41 93"), 0).isEmpty()); // partial frame
    EXPECT_EQ(parser.pendingBytes(), 4);
}
