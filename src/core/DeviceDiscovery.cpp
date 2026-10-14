#include "DeviceDiscovery.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace usbcan::discovery {

namespace {

constexpr int kArphrdCan = 280;

// "usb-1a86_USB_Serial-if00-port0" -> "1a86 USB Serial"
QString prettyById(const QString &name)
{
    QString s = name;
    if (s.startsWith(QLatin1String("usb-")))
        s.remove(0, 4);
    const qsizetype ifPos = s.lastIndexOf(QLatin1String("-if"));
    if (ifPos > 0)
        s.truncate(ifPos);
    return s.replace(QLatin1Char('_'), QLatin1Char(' '));
}

} // namespace

QList<SerialPort> serialPorts(const QString &byIdDir, const QString &devDir)
{
    QList<SerialPort> ports;
    QSet<QString> covered;

    const QFileInfoList links = QDir(byIdDir).entryInfoList(QDir::Files | QDir::System, QDir::Name);
    for (const QFileInfo &link : links) {
        const QString target = link.canonicalFilePath();
        const QString device = QFileInfo(target).fileName();
        covered.insert(device);
        SerialPort p;
        p.path = link.absoluteFilePath();
        p.label = device.isEmpty() ? prettyById(link.fileName())
                                   : QStringLiteral("%1 (%2)").arg(prettyById(link.fileName()), device);
        p.likelyWaveshare = link.fileName().contains(QLatin1String("1a86"));
        ports.append(p);
    }
    std::stable_sort(ports.begin(), ports.end(), [](const SerialPort &a, const SerialPort &b) {
        return a.likelyWaveshare && !b.likelyWaveshare;
    });

    const QStringList devices = QDir(devDir).entryList(
        {QStringLiteral("ttyUSB*"), QStringLiteral("ttyACM*")}, QDir::Files | QDir::System, QDir::Name);
    for (const QString &device : devices) {
        if (!covered.contains(device))
            ports.append({QDir(devDir).filePath(device), device, false});
    }
    return ports;
}

QStringList canInterfaces(const QString &sysNetDir)
{
    QStringList result;
    const QDir dir(sysNetDir);
    const QStringList names = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : names) {
        QFile type(dir.filePath(name + QStringLiteral("/type")));
        if (type.open(QIODevice::ReadOnly) && type.readAll().trimmed().toInt() == kArphrdCan)
            result.append(name);
    }
    return result;
}

} // namespace usbcan::discovery
