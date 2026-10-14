#pragma once

// Readable gtest failure messages for Qt and project types (force-included in all tests).

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <ostream>

inline void PrintTo(const QString &s, std::ostream *os)
{
    *os << '"' << s.toStdString() << '"';
}

inline void PrintTo(const QByteArray &b, std::ostream *os)
{
    *os << "hex[" << b.toHex(' ').toStdString() << ']';
}

inline void PrintTo(const QVariant &v, std::ostream *os)
{
    *os << "QVariant(" << v.typeName() << ": " << v.toString().toStdString() << ')';
}
