#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QMetaType>
#include <QString>

#include <array>
#include <cstdint>

namespace usbcan {

enum class Direction : quint8 { Rx, Tx };

constexpr quint32 kMaxStandardId = 0x7FF;
constexpr quint32 kMaxExtendedId = 0x1FFFFFFF;
constexpr int kMaxDlc = 8;

struct CanFrame
{
    quint32 id = 0;
    bool extended = false;
    bool rtr = false;
    quint8 dlc = 0;
    std::array<quint8, kMaxDlc> data{};
    qint64 timestampUs = 0;
    Direction dir = Direction::Rx;

    QByteArray payload() const;
    bool operator==(const CanFrame &) const = default;
};

// Builds a frame; payload longer than 8 bytes is truncated.
CanFrame makeFrame(quint32 id, bool extended, QByteArrayView payload,
                   Direction dir = Direction::Rx, qint64 timestampUs = 0);

// Unique key of an identifier (standard and extended IDs never collide).
constexpr quint64 frameKey(quint32 id, bool extended)
{
    return (quint64(extended) << 32) | id;
}
constexpr quint32 keyId(quint64 key) { return quint32(key & 0xFFFFFFFFu); }
constexpr bool keyExtended(quint64 key) { return (key >> 32) != 0; }

// "18FF9341" for extended, "123" for standard identifiers.
QString formatId(quint32 id, bool extended);
// "32 FF 0F" (upper case, space separated).
QString formatPayload(QByteArrayView payload);
QString formatPayload(const CanFrame &frame);
// can-utils cansend notation: "18FF9341#32FF0F", "123#R".
QString toCansend(const CanFrame &frame);

qint64 nowUs();

} // namespace usbcan

Q_DECLARE_METATYPE(usbcan::CanFrame)
