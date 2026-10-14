#include "TraceIo.h"

#include "HexInput.h"

#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

namespace usbcan::traceio {

namespace {

const QString kCsvHeader = QStringLiteral("time_s,direction,id,extended,rtr,dlc,data,name");

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QString seconds(qint64 us)
{
    const qint64 sign = us < 0 ? -1 : 1;
    const qint64 abs = us * sign;
    return QStringLiteral("%1%2.%3")
        .arg(sign < 0 ? QStringLiteral("-") : QString())
        .arg(abs / 1000000)
        .arg(abs % 1000000, 6, 10, QLatin1Char('0'));
}

QString csvField(const QString &value)
{
    if (!value.contains(QLatin1Char(',')) && !value.contains(QLatin1Char('"'))
        && !value.contains(QLatin1Char('\n')))
        return value;
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QStringList splitCsvLine(const QString &line)
{
    QStringList fields;
    QString current;
    bool quoted = false;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (quoted) {
            if (c == QLatin1Char('"') && i + 1 < line.size() && line[i + 1] == QLatin1Char('"')) {
                current += c;
                ++i;
            } else if (c == QLatin1Char('"')) {
                quoted = false;
            } else {
                current += c;
            }
        } else if (c == QLatin1Char('"')) {
            quoted = true;
        } else if (c == QLatin1Char(',')) {
            fields.append(current);
            current.clear();
        } else {
            current += c;
        }
    }
    fields.append(current);
    return fields;
}

std::optional<qint64> parseSeconds(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^(\\d+)(?:\\.(\\d{1,6}))?$"));
    const auto m = re.match(text.trimmed());
    if (!m.hasMatch())
        return std::nullopt;
    const QString frac = m.captured(2).leftJustified(6, QLatin1Char('0'));
    return m.captured(1).toLongLong() * 1000000 + frac.toLongLong();
}

QString exportCsv(const QList<CanFrame> &frames, const NameLookup &nameFor)
{
    QString out = kCsvHeader + QLatin1Char('\n');
    for (const CanFrame &f : frames) {
        out += QStringList{
                   seconds(f.timestampUs),
                   f.dir == Direction::Tx ? QStringLiteral("TX") : QStringLiteral("RX"),
                   formatId(f.id, f.extended),
                   f.extended ? QStringLiteral("1") : QStringLiteral("0"),
                   f.rtr ? QStringLiteral("1") : QStringLiteral("0"),
                   QString::number(f.dlc),
                   formatPayload(f),
                   csvField(nameFor ? nameFor(f) : QString()),
               }.join(QLatin1Char(','))
            + QLatin1Char('\n');
    }
    return out;
}

QString exportCandump(const QList<CanFrame> &frames, const QString &iface)
{
    QString out;
    for (const CanFrame &f : frames)
        out += QStringLiteral("(%1) %2 %3\n").arg(seconds(f.timestampUs), iface, toCansend(f));
    return out;
}

QString exportAsc(const QList<CanFrame> &frames)
{
    const qint64 startUs = frames.isEmpty() ? 0 : frames.first().timestampUs;
    const QDateTime start = QDateTime::fromMSecsSinceEpoch(startUs / 1000, Qt::UTC);
    const QString date = QLocale::c().toString(start, QStringLiteral("ddd MMM dd hh:mm:ss.zzz yyyy"));

    QString out;
    out += QStringLiteral("date %1\n").arg(date);
    out += QStringLiteral("base hex  timestamps absolute\n");
    out += QStringLiteral("internal events logged\n");
    out += QStringLiteral("// version 9.0.0\n");
    out += QStringLiteral("Begin Triggerblock %1\n").arg(date);
    for (const CanFrame &f : frames) {
        const QString id = QString::number(f.id, 16).toUpper() + (f.extended ? QStringLiteral("x") : QString());
        const QString dir = f.dir == Direction::Tx ? QStringLiteral("Tx") : QStringLiteral("Rx");
        QString line = QStringLiteral("%1 1  %2 %3   ")
                           .arg(seconds(f.timestampUs - startUs).rightJustified(11))
                           .arg(id.leftJustified(15), dir);
        if (f.rtr)
            line += QStringLiteral("r %1").arg(f.dlc);
        else
            line += QStringLiteral("d %1 %2").arg(f.dlc).arg(formatPayload(f));
        while (line.endsWith(QLatin1Char(' ')))
            line.chop(1);
        out += line + QLatin1Char('\n');
    }
    out += QStringLiteral("End TriggerBlock\n");
    return out;
}

std::optional<CanFrame> parseCsvLine(const QString &line)
{
    const QStringList c = splitCsvLine(line);
    if (c.size() < 7)
        return std::nullopt;
    const auto ts = parseSeconds(c[0]);
    const bool tx = c[1].trimmed().compare(QLatin1String("TX"), Qt::CaseInsensitive) == 0;
    const bool extended = c[3].trimmed() == QLatin1String("1");
    const bool rtr = c[4].trimmed() == QLatin1String("1");
    bool dlcOk = false;
    const int dlc = c[5].trimmed().toInt(&dlcOk);
    const auto id = hexinput::parseId(c[2], extended);
    const auto payload = hexinput::parsePayload(c[6]);
    if (!ts || !id.ok || !payload.ok || !dlcOk || dlc < 0 || dlc > kMaxDlc)
        return std::nullopt;

    CanFrame f = makeFrame(id.id, extended, payload.bytes, tx ? Direction::Tx : Direction::Rx, *ts);
    f.rtr = rtr;
    f.dlc = quint8(dlc);
    return f;
}

std::optional<CanFrame> parseCandumpLine(const QString &line)
{
    // (seconds) iface ID#DATA | ID#R
    static const QRegularExpression re(
        QStringLiteral("^\\(([0-9.]+)\\)\\s+\\S+\\s+([0-9A-Fa-f]+)#(R\\d?|[0-9A-Fa-f]*)\\s*$"));
    const auto m = re.match(line);
    if (!m.hasMatch())
        return std::nullopt;
    const auto ts = parseSeconds(m.captured(1));
    const QString idText = m.captured(2);
    const bool extended = idText.size() == 8;
    const auto id = hexinput::parseId(idText, extended);
    const QString data = m.captured(3);
    if (!ts || !id.ok)
        return std::nullopt;

    if (data.startsWith(QLatin1Char('R'))) {
        CanFrame f = makeFrame(id.id, extended, {}, Direction::Rx, *ts);
        f.rtr = true;
        f.dlc = quint8(data.size() > 1 ? data.mid(1).toInt() : 0);
        return f;
    }
    const auto payload = hexinput::parsePayload(data);
    if (!payload.ok)
        return std::nullopt;
    return makeFrame(id.id, extended, payload.bytes, Direction::Rx, *ts);
}

} // namespace

std::optional<Format> formatForPath(const QString &path)
{
    const QString lower = path.toLower();
    if (lower.endsWith(QLatin1String(".csv")))
        return Format::Csv;
    if (lower.endsWith(QLatin1String(".log")) || lower.endsWith(QLatin1String(".candump")))
        return Format::Candump;
    if (lower.endsWith(QLatin1String(".asc")))
        return Format::Asc;
    return std::nullopt;
}

QString exportText(const QList<CanFrame> &frames, Format format, const NameLookup &nameFor,
                   const QString &interfaceName)
{
    switch (format) {
    case Format::Csv:
        return exportCsv(frames, nameFor);
    case Format::Candump:
        return exportCandump(frames, interfaceName);
    case Format::Asc:
        break;
    }
    return exportAsc(frames);
}

ImportResult importText(const QString &text, Format format)
{
    ImportResult result;
    if (format == Format::Asc) {
        result.error = QStringLiteral("Importing Vector ASC files is not supported");
        return result;
    }

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (format == Format::Csv && line.startsWith(QLatin1String("time_s")))
            continue;
        const auto frame = format == Format::Csv ? parseCsvLine(line) : parseCandumpLine(line);
        if (frame)
            result.frames.append(*frame);
        else
            ++result.skippedLines;
    }
    result.ok = !result.frames.isEmpty() || result.skippedLines == 0;
    if (!result.ok)
        result.error = QStringLiteral("No valid frames found");
    return result;
}

bool writeFile(const QString &path, const QString &text, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit()) {
        setError(error, file.errorString());
        return false;
    }
    return true;
}

std::optional<QString> readFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return std::nullopt;
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace usbcan::traceio
