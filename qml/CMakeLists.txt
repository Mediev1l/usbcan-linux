set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(usbcan_qml STATIC)
qt_add_qml_module(usbcan_qml
    URI UsbCan
    VERSION 1.0
    RESOURCE_PREFIX /
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/UsbCan
    QML_FILES
        Theme.qml
        Main.qml
        ConnectionBar.qml
        ErrorBanner.qml
        TraceView.qml
        FrameDelegate.qml
        FrameDetails.qml
        DetailCard.qml
        FilterPanel.qml
        SendPanel.qml
        MessageCard.qml
        SaveMessageDialog.qml
        StatusBar.qml
        PanelHeader.qml
)
target_link_libraries(usbcan_qml PRIVATE Qt6::Quick Qt6::QuickControls2)
