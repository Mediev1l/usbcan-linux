#pragma once

#include "SocketApi.h"

#include <QString>

#include <linux/can.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <deque>
#include <memory>
#include <variant>
#include <vector>

// Scriptable syscalls. The "socket" is a real eventfd so QSocketNotifier works;
// write to it with signalReadable() to make the notifier fire.
class FakeSocketApi : public usbcan::ISocketApi
{
public:
    struct Short {}; // a read that returns fewer bytes than a can_frame
    using ReadResult = std::variant<can_frame, int /* errno */, Short>;

    // Shared so a test can keep inspecting after the transport took ownership.
    struct State
    {
        bool failSocket = false;
        int ifIndex = 7;
        int bindResult = 0;
        ssize_t writeResult = sizeof(can_frame);
        int error = 0;
        std::deque<ReadResult> reads;
        std::vector<can_frame> writes;
        std::vector<int> closed;
        QString lastInterface;
        int boundIndex = -1;
        int fd = -1;
    };

    explicit FakeSocketApi(std::shared_ptr<State> state)
        : m_state(std::move(state))
    {
    }

    static void signalReadable(int fd)
    {
        const uint64_t one = 1;
        [[maybe_unused]] auto n = ::write(fd, &one, sizeof(one));
    }

    int openRawCan() override
    {
        if (m_state->failSocket) {
            m_state->error = EAFNOSUPPORT;
            return -1;
        }
        m_state->fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        return m_state->fd;
    }

    int interfaceIndex(int, const QString &name) override
    {
        m_state->lastInterface = name;
        if (m_state->ifIndex < 0)
            m_state->error = ENODEV;
        return m_state->ifIndex;
    }

    int bindCan(int, int ifindex) override
    {
        m_state->boundIndex = ifindex;
        if (m_state->bindResult < 0)
            m_state->error = EADDRNOTAVAIL;
        return m_state->bindResult;
    }

    ssize_t readFrame(int fd, can_frame *frame) override
    {
        if (m_state->reads.empty()) {
            uint64_t drained = 0;
            [[maybe_unused]] auto n = ::read(fd, &drained, sizeof(drained));
            m_state->error = EAGAIN;
            return -1;
        }
        ReadResult r = m_state->reads.front();
        m_state->reads.pop_front();
        if (auto *err = std::get_if<int>(&r)) {
            m_state->error = *err;
            return -1;
        }
        if (std::holds_alternative<Short>(r))
            return 3;
        *frame = std::get<can_frame>(r);
        return sizeof(can_frame);
    }

    ssize_t writeFrame(int, const can_frame &frame) override
    {
        m_state->writes.push_back(frame);
        if (m_state->writeResult < 0)
            m_state->error = ENOBUFS;
        return m_state->writeResult;
    }

    void closeSocket(int fd) override
    {
        m_state->closed.push_back(fd);
        ::close(fd);
    }

    int lastError() const override { return m_state->error; }

private:
    std::shared_ptr<State> m_state;
};
