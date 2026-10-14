#include "SocketApi.h"

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace usbcan {

int PosixSocketApi::openRawCan()
{
    return ::socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, CAN_RAW);
}

int PosixSocketApi::interfaceIndex(int fd, const QString &name)
{
    ifreq ifr{};
    const QByteArray n = name.toLocal8Bit().left(IFNAMSIZ - 1);
    std::memcpy(ifr.ifr_name, n.constData(), size_t(n.size()));
    if (::ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
        return -1;
    return ifr.ifr_ifindex;
}

int PosixSocketApi::bindCan(int fd, int ifindex)
{
    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifindex;
    return ::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
}

ssize_t PosixSocketApi::readFrame(int fd, can_frame *frame)
{
    return ::read(fd, frame, sizeof(can_frame));
}

ssize_t PosixSocketApi::writeFrame(int fd, const can_frame &frame)
{
    return ::write(fd, &frame, sizeof(can_frame));
}

void PosixSocketApi::closeSocket(int fd)
{
    ::close(fd);
}

int PosixSocketApi::lastError() const
{
    return errno;
}

} // namespace usbcan
