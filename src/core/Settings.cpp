#include "Settings.h"

#include <QStandardPaths>

#include <algorithm>

namespace usbcan {

namespace {

const QString kDefaultPort = QStringLiteral("/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0");

} // namespace

Settings::Settings(QSettings *settings)
    : m_settings(settings)
{
}

QString Settings::defaultLibraryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/library.canlib.json");
}

ConnectionSettings Settings::connection() const
{
    ConnectionSettings c;
    c.backend = m_settings->value(QStringLiteral("connection/backend")).toInt() == int(Backend::SocketCan)
        ? Backend::SocketCan
        : Backend::Waveshare;
    c.port = m_settings->value(QStringLiteral("connection/port"), kDefaultPort).toString();
    c.interface = m_settings->value(QStringLiteral("connection/interface")).toString();
    c.waveshare.bitrate = m_settings->value(QStringLiteral("connection/bitrate"), 250000).toInt();
    c.waveshare.mode = waveshare::Mode(
        std::clamp(m_settings->value(QStringLiteral("connection/mode"), 0).toInt(), 0, 3));
    c.waveshare.frameType = m_settings->value(QStringLiteral("connection/extended"), true).toBool()
        ? waveshare::FrameType::Extended
        : waveshare::FrameType::Standard;
    return c;
}

void Settings::setConnection(const ConnectionSettings &c)
{
    m_settings->setValue(QStringLiteral("connection/backend"), int(c.backend));
    m_settings->setValue(QStringLiteral("connection/port"), c.port);
    m_settings->setValue(QStringLiteral("connection/interface"), c.interface);
    m_settings->setValue(QStringLiteral("connection/bitrate"), c.waveshare.bitrate);
    m_settings->setValue(QStringLiteral("connection/mode"), int(c.waveshare.mode));
    m_settings->setValue(QStringLiteral("connection/extended"),
                         c.waveshare.frameType == waveshare::FrameType::Extended);
}

QString Settings::libraryPath() const
{
    return m_settings->value(QStringLiteral("library/path"), defaultLibraryPath()).toString();
}

void Settings::setLibraryPath(const QString &path)
{
    m_settings->setValue(QStringLiteral("library/path"), path);
}

bool Settings::newIdsVisible() const
{
    return m_settings->value(QStringLiteral("filters/newIdsVisible"), true).toBool();
}

void Settings::setNewIdsVisible(bool visible)
{
    m_settings->setValue(QStringLiteral("filters/newIdsVisible"), visible);
}

QSet<quint64> Settings::hiddenKeys() const
{
    QSet<quint64> keys;
    const QStringList list = m_settings->value(QStringLiteral("filters/hidden")).toStringList();
    for (const QString &s : list) {
        bool ok = false;
        const quint64 key = s.toULongLong(&ok, 16);
        if (ok)
            keys.insert(key);
    }
    return keys;
}

void Settings::setHiddenKeys(const QSet<quint64> &keys)
{
    QList<quint64> sorted(keys.begin(), keys.end());
    std::sort(sorted.begin(), sorted.end());
    QStringList list;
    for (quint64 key : sorted)
        list.append(QString::number(key, 16));
    m_settings->setValue(QStringLiteral("filters/hidden"), list);
}

} // namespace usbcan
