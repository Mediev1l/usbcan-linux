add_library(usbcan_backend STATIC
    AppController.h AppController.cpp
    LibraryModel.h LibraryModel.cpp
    QmlRegistration.h QmlRegistration.cpp
)

target_include_directories(usbcan_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(usbcan_backend PUBLIC usbcan_core Qt6::Qml)
target_compile_options(usbcan_backend PRIVATE -Wall -Wextra -Wpedantic)
usbcan_instrument(usbcan_backend)
