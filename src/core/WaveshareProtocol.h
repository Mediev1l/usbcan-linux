#pragma once

#include "CanFrame.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QList>

#include <optional>

// Serial protocol of the Waveshare USB-CAN-A (CH340 based) adapter, as used by
// the vendor "USBCAN V2" app in "Variable length" mode.
//
//   config  : AA 55 12 <speed> <ftype> <filter:4> <mask:4> <mode> <once> 00 00 00 00 <sum>
//             sum = (sum of bytes[2..18]) & 0xFF
//   status  : AA 55 04 <rx err> <tx err> 00 ... (20 bytes, same layout)
//   frame   : AA <type> <id: 2 (std) or 4 (ext) bytes LE> <data: dlc bytes> 55
//             type = 0b11 E R dddd  (E = extended, R = remote, dddd = DLC)
namespace usbcan::waveshare {

constexpr int kSerialBaudRate = 2000000;
constexpr int kControlPacketSize = 20;

enum class Mode : quint8 { Normal = 0, Loopback = 1, Silent = 2, LoopbackSilent = 3 };
enum class FrameType : quint8 { Standard = 1, Extended = 2 };

struct Config
{
    int bitrate = 250000;
    Mode mode = Mode::Normal;
    FrameType frameType = FrameType::Extended;
    // false = automatic retransmission (vendor app default, "Only send once" unchecked)
    bool sendOnce = false;
};

struct Status
{
    quint8 rxErrors = 0;
    quint8 txErrors = 0;
};

const QList<int> &supportedBitrates();
std::optional<quint8> bitrateCode(int bitrate);

// Empty result when the bitrate is not supported by the adapter.
QByteArray encodeConfig(const Config &config);
QByteArray encodeStatusRequest();
// Remote frames carry DLC zero bytes so the length always matches the DLC.
QByteArray encodeFrame(const CanFrame &frame);

class Parser
{
public:
    struct Event
    {
        enum class Kind { Frame, Status };
        Kind kind = Kind::Frame;
        CanFrame frame;
        Status status;
    };

    // Feeds raw serial bytes; returns every complete frame/status found.
    // Invalid bytes are skipped one at a time until the stream resyncs.
    QList<Event> feed(QByteArrayView bytes, qint64 timestampUs);

    void reset();
    qsizetype pendingBytes() const { return m_buffer.size(); }
    quint64 droppedBytes() const { return m_dropped; }

private:
    QByteArray m_buffer;
    quint64 m_dropped = 0;
};

} // namespace usbcan::waveshare
