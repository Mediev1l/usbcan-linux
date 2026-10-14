#include "J1939.h"

#include <QHash>

namespace usbcan::j1939 {

namespace {

QString binary(quint32 value, int width)
{
    return QStringLiteral("%1").arg(value, width, 2, QLatin1Char('0'));
}

const QHash<quint32, QString> &knownPgns()
{
    static const QHash<quint32, QString> names{
        {0x00E800, QStringLiteral("ACKM - Acknowledgment")},
        {0x00EA00, QStringLiteral("RQST - Request")},
        {0x00EB00, QStringLiteral("TP.DT - Transport Protocol Data Transfer")},
        {0x00EC00, QStringLiteral("TP.CM - Transport Protocol Connection Management")},
        {0x00EE00, QStringLiteral("AC - Address Claimed")},
        {0x00EF00, QStringLiteral("PROPA - Proprietary A")},
        {0x01EF00, QStringLiteral("PROPA2 - Proprietary A2")},
        {0x00F001, QStringLiteral("EBC1 - Electronic Brake Controller 1")},
        {0x00F003, QStringLiteral("EEC2 - Electronic Engine Controller 2")},
        {0x00F004, QStringLiteral("EEC1 - Electronic Engine Controller 1")},
        {0x00F005, QStringLiteral("ETC2 - Electronic Transmission Controller 2")},
        {0x00FE6C, QStringLiteral("TCO1 - Tachograph")},
        {0x00FEBF, QStringLiteral("EBC2 - Wheel Speed Information")},
        {0x00FECA, QStringLiteral("DM1 - Active Diagnostic Trouble Codes")},
        {0x00FECB, QStringLiteral("DM2 - Previously Active Diagnostic Trouble Codes")},
        {0x00FEDA, QStringLiteral("SOFT - Software Identification")},
        {0x00FEE5, QStringLiteral("HOURS - Engine Hours, Revolutions")},
        {0x00FEE6, QStringLiteral("TD - Time/Date")},
        {0x00FEEC, QStringLiteral("VI - Vehicle Identification")},
        {0x00FEEE, QStringLiteral("ET1 - Engine Temperature 1")},
        {0x00FEEF, QStringLiteral("EFL/P1 - Engine Fluid Level/Pressure 1")},
        {0x00FEF1, QStringLiteral("CCVS1 - Cruise Control/Vehicle Speed 1")},
        {0x00FEF2, QStringLiteral("LFE1 - Fuel Economy (Liquid)")},
        {0x00FEF5, QStringLiteral("AMB - Ambient Conditions")},
        {0x00FEF7, QStringLiteral("VEP1 - Vehicle Electrical Power 1")},
    };
    return names;
}

} // namespace

Id decode(quint32 canId)
{
    Id id;
    id.priority = (canId >> 26) & 0x07;
    id.edp = (canId >> 25) & 0x01;
    id.dp = (canId >> 24) & 0x01;
    id.pf = (canId >> 16) & 0xFF;
    id.ps = (canId >> 8) & 0xFF;
    id.sa = canId & 0xFF;
    id.pdu1 = id.pf < 240;
    id.pgn = (quint32(id.edp) << 17) | (quint32(id.dp) << 16) | (quint32(id.pf) << 8);
    if (id.pdu1) {
        id.da = id.ps;
        id.broadcast = id.da == kGlobalAddress;
    } else {
        id.pgn |= id.ps;
        id.da = kGlobalAddress;
        id.broadcast = true;
    }
    return id;
}

QString pgnName(quint32 pgn)
{
    const auto it = knownPgns().constFind(pgn);
    if (it != knownPgns().constEnd())
        return it.value();
    if ((pgn & 0x2FF00) == 0x0FF00) // xFF00-xFFFF on either data page, EDP 0
        return QStringLiteral("PROPB - Proprietary B");
    return {};
}

QString pduFormatName(const Id &id)
{
    return id.pdu1 ? QStringLiteral("PDU1 (destination specific)")
                   : QStringLiteral("PDU2 (broadcast)");
}

QList<BitField> idBitFields(quint32 canId)
{
    const Id id = decode(canId);
    return {
        {QStringLiteral("Priority"), binary(id.priority, 3), id.priority},
        {QStringLiteral("EDP"), binary(id.edp, 1), id.edp},
        {QStringLiteral("DP"), binary(id.dp, 1), id.dp},
        {QStringLiteral("PF"), binary(id.pf, 8), id.pf},
        {id.pdu1 ? QStringLiteral("PS (DA)") : QStringLiteral("PS (GE)"), binary(id.ps, 8), id.ps},
        {QStringLiteral("SA"), binary(id.sa, 8), id.sa},
    };
}

} // namespace usbcan::j1939
