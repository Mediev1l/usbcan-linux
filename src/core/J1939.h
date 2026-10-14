#pragma once

#include <QList>
#include <QString>

// SAE J1939 decoding of a 29-bit CAN identifier.
//
//   bits 28..26 priority | 25 EDP | 24 DP | 23..16 PF | 15..8 PS | 7..0 SA
//   PF < 240  -> PDU1, PS is the destination address, PGN = EDP DP PF 00
//   PF >= 240 -> PDU2, PS is the group extension,     PGN = EDP DP PF PS
namespace usbcan::j1939 {

constexpr quint8 kGlobalAddress = 0xFF;

struct Id
{
    quint8 priority = 0;
    quint8 edp = 0;
    quint8 dp = 0;
    quint8 pf = 0;
    quint8 ps = 0;
    quint8 sa = 0;
    quint32 pgn = 0;
    bool pdu1 = false;
    // Destination address: PS for PDU1, global (0xFF) for PDU2.
    quint8 da = kGlobalAddress;
    bool broadcast = true;
};

struct BitField
{
    QString label;
    QString bits;
    quint32 value = 0;
};

Id decode(quint32 canId);

// Name of a well-known PGN, e.g. "EEC1 - Electronic Engine Controller 1";
// "Proprietary B" for the FF00-FFFF range; empty when unknown.
QString pgnName(quint32 pgn);

QString pduFormatName(const Id &id);

// Identifier split into the J1939 fields, as binary strings.
QList<BitField> idBitFields(quint32 canId);

} // namespace usbcan::j1939
