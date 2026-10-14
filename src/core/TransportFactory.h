#pragma once

#include "ICanTransport.h"
#include "WaveshareProtocol.h"

#include <functional>
#include <memory>

namespace usbcan {

enum class Backend { Waveshare = 0, SocketCan = 1 };

struct ConnectionSettings
{
    Backend backend = Backend::Waveshare;
    QString port;      // serial port path (Waveshare)
    QString interface; // SocketCAN interface name
    waveshare::Config waveshare;
};

using TransportFactory = std::function<std::unique_ptr<ICanTransport>(const ConnectionSettings &)>;

// Real transports: WaveshareTransport on a QSerialPort, SocketCanTransport on POSIX sockets.
std::unique_ptr<ICanTransport> createTransport(const ConnectionSettings &settings);

} // namespace usbcan
