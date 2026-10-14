#include "TransportFactory.h"

#include "SocketCanTransport.h"
#include "WaveshareTransport.h"

namespace usbcan {

std::unique_ptr<ICanTransport> createTransport(const ConnectionSettings &settings)
{
    if (settings.backend == Backend::SocketCan)
        return std::make_unique<SocketCanTransport>(settings.interface);
    return std::make_unique<WaveshareTransport>(settings.port, settings.waveshare);
}

} // namespace usbcan
