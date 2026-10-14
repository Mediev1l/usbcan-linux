qt_add_executable(usbcan-linux main.cpp)

qt_add_resources(usbcan-linux "icons"
    PREFIX /icons
    BASE ${PROJECT_SOURCE_DIR}/resources
    FILES ${PROJECT_SOURCE_DIR}/resources/usbcan-linux.svg
)

target_compile_definitions(usbcan-linux PRIVATE USBCAN_VERSION="${PROJECT_VERSION}")
target_link_libraries(usbcan-linux PRIVATE
    usbcan_backend
    usbcan_qmlplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)

include(GNUInstallDirs)
install(TARGETS usbcan-linux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(${PROJECT_SOURCE_DIR}/resources/usbcan-linux.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/usbcan-linux.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/usbcan-linux.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${PROJECT_SOURCE_DIR}/resources/usbcan-linux.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
