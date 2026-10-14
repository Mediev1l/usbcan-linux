#include "SocketCanTransport.h"
#include "TransportFactory.h"
#include "WaveshareTransport.h"

#include <gtest/gtest.h>

using namespace usbcan;

TEST(TransportFactory, CreatesBackendSpecificTransport)
{
    ConnectionSettings c;
    c.port = QStringLiteral("/dev/ttyUSB7");
    c.interface = QStringLiteral("vcan9");

    auto ws = createTransport(c);
    EXPECT_NE(dynamic_cast<WaveshareTransport *>(ws.get()), nullptr);
    EXPECT_EQ(ws->description(), QStringLiteral("/dev/ttyUSB7"));

    c.backend = Backend::SocketCan;
    auto sc = createTransport(c);
    EXPECT_NE(dynamic_cast<SocketCanTransport *>(sc.get()), nullptr);
    EXPECT_EQ(sc->description(), QStringLiteral("vcan9"));
    EXPECT_FALSE(sc->isOpen());
}
