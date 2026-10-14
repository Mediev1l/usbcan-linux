#pragma once

#include "TransportFactory.h"

#include <QSet>
#include <QSettings>

namespace usbcan {

// Typed access to the persisted application settings.
class Settings
{
public:
    // Does not take ownership.
    explicit Settings(QSettings *settings);

    static QString defaultLibraryPath();

    ConnectionSettings connection() const;
    void setConnection(const ConnectionSettings &connection);

    QString libraryPath() const;
    void setLibraryPath(const QString &path);

    bool newIdsVisible() const;
    void setNewIdsVisible(bool visible);

    QSet<quint64> hiddenKeys() const;
    void setHiddenKeys(const QSet<quint64> &keys);

    void sync() { m_settings->sync(); }

private:
    QSettings *m_settings;
};

} // namespace usbcan
