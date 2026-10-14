#pragma once

#include "CanFrame.h"

#include <QList>
#include <QString>

#include <functional>
#include <optional>

namespace usbcan::traceio {

enum class Format { Csv, Candump, Asc };

using NameLookup = std::function<QString(const CanFrame &)>;

// By file extension: .csv, .log/.candump, .asc
std::optional<Format> formatForPath(const QString &path);

// CSV:     time_s,direction,id,extended,rtr,dlc,data,name   (absolute epoch seconds)
// candump: (1697200000.123456) <iface> 18FF9341#32FF...     (`candump -l` format)
// ASC:     Vector ASCII log, timestamps relative to the first frame
QString exportText(const QList<CanFrame> &frames, Format format, const NameLookup &nameFor = {},
                   const QString &interfaceName = QStringLiteral("can0"));

struct ImportResult
{
    bool ok = false;
    QList<CanFrame> frames;
    int skippedLines = 0;
    QString error;
};

// CSV (as exported above) and candump logs; ASC is export only.
ImportResult importText(const QString &text, Format format);

bool writeFile(const QString &path, const QString &text, QString *error = nullptr);
std::optional<QString> readFile(const QString &path, QString *error = nullptr);

} // namespace usbcan::traceio
