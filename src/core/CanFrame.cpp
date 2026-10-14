#include "CanFrame.h"

#include <QDateTime>

#include <algorithm>

namespace usbcan {

QByteArray CanFrame::payload() const
{
    return QByteArray(reinterpret_cast<const char *>(data.data()), std::min<int>(dlc, kMaxDlc));
}

CanFrame makeFrame(quint32 id, bool extended, QByteArrayView payload, Direction dir,
                   qint64 timestampUs)
{
    CanFrame f;
    f.id = id;
    f.extended = extended;
    f.dlc = quint8(std::min<qsizetype>(payload.size(), kMaxDlc));
    std::copy_n(reinterpret_cast<const quint8 *>(payload.data()), f.dlc, f.data.begin());
    f.dir = dir;
    f.timestampUs = timestampUs;
    return f;
}

QString formatId(quint32 id, bool extended)
{
    return QStringLiteral("%1").arg(id, extended ? 8 : 3, 16, QLatin1Char('0')).toUpper();
}

QString formatPayload(QByteArrayView payload)
{
    return QString::fromLatin1(payload.toByteArray().toHex(' ').toUpper());
}

QString formatPayload(const CanFrame &frame)
{
    return formatPayload(frame.payload());
}

QString toCansend(const CanFrame &frame)
{
    const QString id = formatId(frame.id, frame.extended);
    if (frame.rtr)
        return id + QStringLiteral("#R");
    return id + QLatin1Char('#') + QString::fromLatin1(frame.payload().toHex().toUpper());
}

qint64 nowUs()
{
    return QDateTime::currentMSecsSinceEpoch() * 1000;
}

} // namespace usbcan
