cmake_minimum_required(VERSION 3.21)

project(usbcan-linux
    VERSION 0.1.0
    DESCRIPTION "CAN bus monitor for Waveshare USB-CAN-A and SocketCAN"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USBCAN_BUILD_TESTS "Build unit tests" ON)
option(USBCAN_COVERAGE "Instrument core/backend libraries for coverage" OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core SerialPort Gui Qml Quick QuickControls2)
qt_standard_project_setup()

include(cmake/Coverage.cmake)

add_subdirectory(src/core)
add_subdirectory(src/backend)
add_subdirectory(qml)
add_subdirectory(src/app)

if(USBCAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    if(USBCAN_COVERAGE)
        # Always measure freshly built binaries.
        add_dependencies(coverage core_tests backend_tests qml_tests usbcan-linux)
    endif()
endif()
