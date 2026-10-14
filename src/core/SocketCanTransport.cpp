#include "SocketCanTransport.h"

#include <linux/can.h>

#include <cerrno>
#include <cstring>

namespace usbcan {

SocketCanTransport::SocketCanTransport(QString interfaceName, std::unique_ptr<ISocketApi> api,
                                       QObject *parent)
    : ICanTransport(parent)
    , m_interface(std::move(interfaceName))
    , m_api(std::move(api))
{
}

SocketCanTransport::~SocketCanTransport()
{
    close();
}

bool SocketCanTransport::open()
{
    close();
    m_fd = m_api->openRawCan();
    if (m_fd < 0) {
        fail(QStringLiteral("Cannot create CAN socket"));
        return false;
    }
    const int index = m_api->interfaceIndex(m_fd, m_interface);
    if (index < 0) {
        fail(QStringLiteral("Unknown CAN interface %1").arg(m_interface));
        return false;
    }
    if (m_api->bindCan(m_fd, index) < 0) {
        fail(QStringLiteral("Cannot bind to %1").arg(m_interface));
        return false;
    }
    m_notifier = std::make_unique<QSocketNotifier>(m_fd, QSocketNotifier::Read);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, &SocketCanTransport::readPending);
    return true;
}

void SocketCanTransport::close()
{
    m_notifier.reset();
    if (m_fd >= 0) {
        m_api->closeSocket(m_fd);
        m_fd = -1;
    }
}

bool SocketCanTransport::isOpen() const
{
    return m_fd >= 0;
}

bool SocketCanTransport::send(const CanFrame &frame)
{
    if (m_fd < 0)
        return false;
    can_frame raw{};
    raw.can_id = frame.id;
    if (frame.extended)
        raw.can_id |= CAN_EFF_FLAG;
    if (frame.rtr)
        raw.can_id |= CAN_RTR_FLAG;
    raw.len = std::min<quint8>(frame.dlc, kMaxDlc);
    std::memcpy(raw.data, frame.data.data(), raw.len);
    if (m_api->writeFrame(m_fd, raw) != ssize_t(sizeof(raw))) {
        fail(QStringLiteral("Send on %1 failed").arg(m_interface));
        return false;
    }
    return true;
}

QString SocketCanTransport::description() const
{
    return m_interface;
}

void SocketCanTransport::readPending()
{
    QList<CanFrame> frames;
    while (m_fd >= 0) {
        can_frame raw{};
        const ssize_t n = m_api->readFrame(m_fd, &raw);
        if (n < 0) {
            const int err = m_api->lastError();
            if (err != EAGAIN && err != EWOULDBLOCK)
                fail(QStringLiteral("Read from %1 failed").arg(m_interface));
            break;
        }
        if (n != ssize_t(sizeof(raw)) || (raw.can_id & CAN_ERR_FLAG))
            continue;
        CanFrame f;
        f.extended = raw.can_id & CAN_EFF_FLAG;
        f.rtr = raw.can_id & CAN_RTR_FLAG;
        f.id = raw.can_id & (f.extended ? CAN_EFF_MASK : CAN_SFF_MASK);
        f.dlc = std::min<quint8>(raw.len, kMaxDlc);
        if (!f.rtr)
            std::memcpy(f.data.data(), raw.data, f.dlc);
        f.timestampUs = nowUs();
        frames.append(f);
    }
    if (!frames.isEmpty())
        emit framesReceived(frames);
}

void SocketCanTransport::fail(const QString &what)
{
    const QString reason = QString::fromLocal8Bit(std::strerror(m_api->lastError()));
    close();
    emit errorOccurred(QStringLiteral("%1: %2").arg(what, reason));
}

} // namespace usbcan
