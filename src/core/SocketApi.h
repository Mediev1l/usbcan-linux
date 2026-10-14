#pragma once

#include <QString>

#include <sys/types.h>

struct can_frame;

namespace usbcan {

// The handful of syscalls SocketCanTransport needs, so tests can replace them.
// Return values follow the POSIX calls (-1 on failure, errno in lastError()).
class ISocketApi
{
public:
    virtual ~ISocketApi() = default;

    // Non-blocking CAN_RAW socket.
    virtual int openRawCan() = 0;
    virtual int interfaceIndex(int fd, const QString &name) = 0;
    virtual int bindCan(int fd, int ifindex) = 0;
    virtual ssize_t readFrame(int fd, can_frame *frame) = 0;
    virtual ssize_t writeFrame(int fd, const can_frame &frame) = 0;
    virtual void closeSocket(int fd) = 0;
    virtual int lastError() const = 0;
};

class PosixSocketApi : public ISocketApi
{
public:
    int openRawCan() override;
    int interfaceIndex(int fd, const QString &name) override;
    int bindCan(int fd, int ifindex) override;
    ssize_t readFrame(int fd, can_frame *frame) override;
    ssize_t writeFrame(int fd, const can_frame &frame) override;
    void closeSocket(int fd) override;
    int lastError() const override;
};

} // namespace usbcan
