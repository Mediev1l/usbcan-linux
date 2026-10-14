find_package(GTest REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Test)
include(GoogleTest)

set(USBCAN_TEST_ENV "QT_QPA_PLATFORM=offscreen")

add_library(usbcan_test_support STATIC
    support/TestMain.cpp
    support/FakeSerialDevice.h
    support/FakeSocketApi.h
    support/FakeTransport.h
)
target_include_directories(usbcan_test_support PUBLIC support)
target_compile_options(usbcan_test_support PUBLIC -include ${CMAKE_CURRENT_SOURCE_DIR}/support/QtPrinters.h)
target_link_libraries(usbcan_test_support PUBLIC usbcan_core Qt6::Test GTest::gtest GTest::gmock)
usbcan_instrument(usbcan_test_support)

file(GLOB CORE_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/core/test_*.cpp)
add_executable(core_tests ${CORE_TESTS})
target_link_libraries(core_tests PRIVATE usbcan_core usbcan_test_support)
usbcan_instrument(core_tests)
gtest_discover_tests(core_tests PROPERTIES ENVIRONMENT "${USBCAN_TEST_ENV}" DISCOVERY_TIMEOUT 30 DISCOVERY_MODE PRE_TEST)

file(GLOB BACKEND_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/backend/test_*.cpp)
if(BACKEND_TESTS)
    add_executable(backend_tests ${BACKEND_TESTS})
    target_link_libraries(backend_tests PRIVATE usbcan_backend usbcan_test_support Qt6::Qml)
    usbcan_instrument(backend_tests)
    gtest_discover_tests(backend_tests PROPERTIES ENVIRONMENT "${USBCAN_TEST_ENV}" DISCOVERY_TIMEOUT 30 DISCOVERY_MODE PRE_TEST)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/qml/CMakeLists.txt)
    add_subdirectory(qml)
endif()
