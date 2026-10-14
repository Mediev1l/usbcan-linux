#pragma once

#include <QList>
#include <QString>

namespace usbcan::discovery {

struct SerialPort
{
    QString path;  // stable /dev/serial/by-id/... path when available
    QString label; // "1a86 USB Serial (ttyUSB0)"
    bool likelyWaveshare = false; // CH340 (1a86:7523)
    bool operator==(const SerialPort &) const = default;
};

// Ports from byIdDir (stable names, Waveshare first), followed by ttyUSB*/ttyACM*
// in devDir that are not already covered by a by-id link.
QList<SerialPort> serialPorts(const QString &byIdDir = QStringLiteral("/dev/serial/by-id"),
                              const QString &devDir = QStringLiteral("/dev"));

// Network interfaces of type ARPHRD_CAN (280): can0, vcan0, ...
QStringList canInterfaces(const QString &sysNetDir = QStringLiteral("/sys/class/net"));

} // namespace usbcan::discovery
