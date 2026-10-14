#pragma once

#include <QByteArray>
#include <QString>

namespace usbcan::hexinput {

struct IdResult
{
    bool ok = false;
    quint32 id = 0;
    QString error;
};

struct PayloadResult
{
    bool ok = false;
    QByteArray bytes;
    QString error;
};

// Accepts "18ff9341", "0x18FF9341", " 123 ". Range is checked against the
// frame type (11-bit or 29-bit).
IdResult parseId(const QString &text, bool extended);

// True when the text only fits a 29-bit identifier (more than 3 digits or > 0x7FF).
bool looksExtended(const QString &text);

// Accepts "32 FF 0F", "32FF0F", "32,ff,0f", "0x32 0xFF"; at most 8 bytes.
PayloadResult parsePayload(const QString &text);

} // namespace usbcan::hexinput
