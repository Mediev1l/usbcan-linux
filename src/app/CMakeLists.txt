qt_add_executable(usbcan-linux main.cpp)

# Window icon as PNGs (the SVG needs the optional Qt SVG plugin at runtime).
set(USBCAN_ICON_SIZES 16 24 32 48 64 128 256)
set(USBCAN_ICON_PNGS)
foreach(size IN LISTS USBCAN_ICON_SIZES)
    list(APPEND USBCAN_ICON_PNGS ${PROJECT_SOURCE_DIR}/resources/icons/usbcan-linux-${size}.png)
endforeach()
qt_add_resources(usbcan-linux "icons"
    PREFIX /
    BASE ${PROJECT_SOURCE_DIR}/resources
    FILES ${USBCAN_ICON_PNGS}
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
foreach(size IN LISTS USBCAN_ICON_SIZES)
    install(FILES ${PROJECT_SOURCE_DIR}/resources/icons/usbcan-linux-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME usbcan-linux.png)
endforeach()
