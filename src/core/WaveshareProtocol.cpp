#include "WaveshareProtocol.h"

#include <QMap>

namespace usbcan::waveshare {

namespace {

constexpr quint8 kStart = 0xAA;
constexpr quint8 kEnd = 0x55;
constexpr quint8 kControl = 0x55; // second byte of a 20-byte control packet
constexpr quint8 kCmdConfig = 0x12; // "set and start", variable length protocol
constexpr quint8 kCmdStatus = 0x04;

const QMap<int, quint8> &bitrateTable()
{
    static const QMap<int, quint8> table{
        {1000000, 0x01}, {800000, 0x02}, {500000, 0x03}, {400000, 0x04},
        {250000, 0x05},  {200000, 0x06}, {125000, 0x07}, {100000, 0x08},
        {50000, 0x09},   {20000, 0x0A},  {10000, 0x0B},  {5000, 0x0C},
    };
    return table;
}

QByteArray controlPacket(quint8 command, QByteArrayView body)
{
    QByteArray p;
    p.reserve(kControlPacketSize);
    p.append(char(kStart));
    p.append(char(kControl));
    p.append(char(command));
    p.append(body.toByteArray());
    p.append(QByteArray(kControlPacketSize - 1 - p.size(), '\0'));
    quint8 sum = 0;
    for (qsizetype i = 2; i < p.size(); ++i)
        sum += quint8(p[i]);
    p.append(char(sum));
    return p;
}

} // namespace

const QList<int> &supportedBitrates()
{
    static const QList<int> list = [] {
        QList<int> keys = bitrateTable().keys();
        std::reverse(keys.begin(), keys.end());
        return keys;
    }();
    return list;
}

std::optional<quint8> bitrateCode(int bitrate)
{
    const auto it = bitrateTable().constFind(bitrate);
    if (it == bitrateTable().constEnd())
        return std::nullopt;
    return it.value();
}

QByteArray encodeConfig(const Config &config)
{
    const auto code = bitrateCode(config.bitrate);
    if (!code)
        return {};
    QByteArray body;
    body.append(char(*code));
    body.append(char(config.frameType));
    body.append(QByteArray(8, '\0')); // filter id + mask: accept everything
    body.append(char(config.mode));
    body.append(char(config.sendOnce ? 1 : 0));
    return controlPacket(kCmdConfig, body);
}

QByteArray encodeStatusRequest()
{
    return controlPacket(kCmdStatus, {});
}

QByteArray encodeFrame(const CanFrame &frame)
{
    const quint8 dlc = std::min<quint8>(frame.dlc, kMaxDlc);
    quint8 type = 0xC0 | dlc;
    if (frame.extended)
        type |= 0x20;
    if (frame.rtr)
        type |= 0x10;

    QByteArray out;
    out.append(char(kStart));
    out.append(char(type));
    const int idBytes = frame.extended ? 4 : 2;
    for (int i = 0; i < idBytes; ++i)
        out.append(char((frame.id >> (8 * i)) & 0xFF));
    for (int i = 0; i < dlc; ++i)
        out.append(frame.rtr ? '\0' : char(frame.data[i]));
    out.append(char(kEnd));
    return out;
}

QList<Parser::Event> Parser::feed(QByteArrayView bytes, qint64 timestampUs)
{
    m_buffer.append(bytes.toByteArray());
    QList<Event> events;

    auto drop = [this](qsizetype n) {
        m_buffer.remove(0, n);
        m_dropped += quint64(n);
    };

    while (true) {
        const qsizetype start = m_buffer.indexOf(char(kStart));
        if (start < 0) {
            drop(m_buffer.size());
            break;
        }
        drop(start);
        if (m_buffer.size() < 2)
            break;

        const auto type = quint8(m_buffer[1]);
        if (type == kControl) {
            if (m_buffer.size() < kControlPacketSize)
                break;
            Event e;
            e.kind = Event::Kind::Status;
            e.status.rxErrors = quint8(m_buffer[3]);
            e.status.txErrors = quint8(m_buffer[4]);
            events.append(e);
            m_buffer.remove(0, kControlPacketSize);
            continue;
        }

        const quint8 dlc = type & 0x0F;
        if ((type & 0xC0) != 0xC0 || dlc > kMaxDlc) {
            drop(1);
            continue;
        }
        const bool extended = type & 0x20;
        const int idBytes = extended ? 4 : 2;
        const qsizetype total = 2 + idBytes + dlc + 1;
        if (m_buffer.size() < total)
            break;
        if (quint8(m_buffer[total - 1]) != kEnd) {
            drop(1);
            continue;
        }

        Event e;
        CanFrame &f = e.frame;
        f.extended = extended;
        f.rtr = type & 0x10;
        f.dlc = dlc;
        for (int i = 0; i < idBytes; ++i)
            f.id |= quint32(quint8(m_buffer[2 + i])) << (8 * i);
        if (!f.rtr) {
            for (int i = 0; i < dlc; ++i)
                f.data[i] = quint8(m_buffer[2 + idBytes + i]);
        }
        f.timestampUs = timestampUs;
        f.dir = Direction::Rx;
        events.append(e);
        m_buffer.remove(0, total);
    }
    return events;
}

void Parser::reset()
{
    m_buffer.clear();
    m_dropped = 0;
}

} // namespace usbcan::waveshare
