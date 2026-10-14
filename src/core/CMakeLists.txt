add_library(usbcan_core STATIC
    CanFrame.h CanFrame.cpp
    HexInput.h HexInput.cpp
    WaveshareProtocol.h WaveshareProtocol.cpp
    J1939.h J1939.cpp
    MessageLibrary.h MessageLibrary.cpp
    TraceIo.h TraceIo.cpp
    ICanTransport.h
    WaveshareTransport.h WaveshareTransport.cpp
    SocketApi.h SocketApi.cpp
    SocketCanTransport.h SocketCanTransport.cpp
    DeviceDiscovery.h DeviceDiscovery.cpp
    TransportFactory.h TransportFactory.cpp
    TraceStore.h TraceStore.cpp
    TraceModel.h TraceModel.cpp
    FilterModel.h FilterModel.cpp
    TraceFilterProxy.h TraceFilterProxy.cpp
    CyclicSender.h CyclicSender.cpp
    Settings.h Settings.cpp
)

target_include_directories(usbcan_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(usbcan_core PUBLIC Qt6::Core Qt6::SerialPort)
target_compile_options(usbcan_core PRIVATE -Wall -Wextra -Wpedantic)
usbcan_instrument(usbcan_core)
