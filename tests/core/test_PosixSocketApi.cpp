#include "SocketApi.h"

#include <linux/can.h>

#include <gtest/gtest.h>

#include <cerrno>

using namespace usbcan;

// Real syscalls, but only against the loopback interface and an unbound socket:
// no CAN interface is opened, so nothing reaches any bus.
TEST(PosixSocketApi, SyscallsBehave)
{
    PosixSocketApi api;
    const int fd = api.openRawCan();
    if (fd < 0)
        GTEST_SKIP() << "CAN sockets unavailable (can module not loaded)";

    const int lo = api.interfaceIndex(fd, QStringLiteral("lo"));
    EXPECT_GT(lo, 0);

    EXPECT_EQ(api.interfaceIndex(fd, QStringLiteral("nonexistent-x")), -1);
    EXPECT_EQ(api.lastError(), ENODEV);

    EXPECT_EQ(api.bindCan(fd, lo), -1); // loopback is not a CAN device

    can_frame frame{};
    EXPECT_EQ(api.readFrame(fd, &frame), -1); // unbound, non-blocking
    EXPECT_EQ(api.writeFrame(fd, frame), -1);

    api.closeSocket(fd);
}
