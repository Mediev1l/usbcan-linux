#include "HexInput.h"

#include "CanFrame.h"

#include <QRegularExpression>

namespace usbcan::hexinput {

namespace {

QString stripPrefix(QString s)
{
    if (s.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        s.remove(0, 2);
    return s;
}

bool isHex(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^[0-9A-Fa-f]+$"));
    return re.match(s).hasMatch();
}

} // namespace

IdResult parseId(const QString &text, bool extended)
{
    const QString s = stripPrefix(text.trimmed());
    if (s.isEmpty())
        return {false, 0, QStringLiteral("ID is empty")};
    if (!isHex(s))
        return {false, 0, QStringLiteral("ID must be hexadecimal")};
    if (s.size() > 8)
        return {false, 0, QStringLiteral("ID is too long")};

    const quint32 id = s.toUInt(nullptr, 16);
    const quint32 max = extended ? kMaxExtendedId : kMaxStandardId;
    if (id > max) {
        return {false, 0,
                extended ? QStringLiteral("Extended ID must be at most 1FFFFFFF")
                         : QStringLiteral("Standard ID must be at most 7FF")};
    }
    return {true, id, {}};
}

bool looksExtended(const QString &text)
{
    const QString s = stripPrefix(text.trimmed());
    return s.size() > 3 || s.toUInt(nullptr, 16) > kMaxStandardId;
}

PayloadResult parsePayload(const QString &text)
{
    QStringList tokens = text.split(QRegularExpression(QStringLiteral("[\\s,;:\\-]+")),
                                    Qt::SkipEmptyParts);
    QString hex;
    for (const QString &t : tokens)
        hex += stripPrefix(t);

    if (!hex.isEmpty() && !isHex(hex))
        return {false, {}, QStringLiteral("Payload must be hexadecimal bytes")};
    if (hex.size() % 2 != 0)
        return {false, {}, QStringLiteral("Payload has an odd number of hex digits")};
    if (hex.size() > kMaxDlc * 2)
        return {false, {}, QStringLiteral("Payload is longer than 8 bytes")};
    return {true, QByteArray::fromHex(hex.toLatin1()), {}};
}

} // namespace usbcan::hexinput
