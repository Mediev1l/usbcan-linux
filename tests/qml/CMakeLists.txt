find_package(Qt6 REQUIRED COMPONENTS QuickTest)

file(GLOB QML_TEST_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tst_*.qml)

qt_add_executable(qml_tests tst_main.cpp ../support/FakeTransport.h)
target_include_directories(qml_tests PRIVATE ../support)
target_compile_definitions(qml_tests PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qml_tests PRIVATE
    usbcan_backend
    usbcan_qmlplugin
    Qt6::QuickTest
    Qt6::Quick
    Qt6::QuickControls2
)
target_sources(qml_tests PRIVATE ${QML_TEST_FILES})
set_source_files_properties(${QML_TEST_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

add_test(NAME qml_tests COMMAND qml_tests -platform offscreen)
set_tests_properties(qml_tests PROPERTIES ENVIRONMENT "${USBCAN_TEST_ENV}" TIMEOUT 120)
