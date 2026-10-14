import QtQuick
import QtTest
import UsbCan
import UsbCan.Backend

TestCase {
    name: "MainWindow"
    when: windowShown

    property var window: null

    function init() {
        Helper.reset()
        const component = Qt.createComponent("qrc:/UsbCan/Main.qml")
        compare(component.status, Component.Ready, component.errorString())
        window = component.createObject(null)
        verify(window)
        waitForRendering(window.contentItem)
    }

    function cleanup() {
        window.destroy()
        window = null
    }

    function test_connectButtonTogglesConnection() {
        const button = findChild(window, "connectButton")
        verify(button)
        compare(button.text, "Connect")
        mouseClick(button)
        verify(App.connected)
        compare(button.text, "Disconnect")
        compare(findChild(window, "statusConnection").text, "Waveshare fake0 @ 250 kbit/s")
        verify(!findChild(window, "bitrateCombo").enabled)
        mouseClick(button)
        verify(!App.connected)
        verify(findChild(window, "bitrateCombo").enabled)
    }

    function test_backendSwitchShowsInterfaceChoice() {
        verify(findChild(window, "portCombo").visible)
        App.backend = 1
        verify(!findChild(window, "portCombo").visible)
        verify(findChild(window, "interfaceCombo").visible)
        App.backend = 0
    }

    function test_errorsAreShownAndDismissable() {
        App.sendFrame("123", false, "") // not connected
        const banner = findChild(window, "errorBanner")
        verify(banner.visible)
        compare(findChild(window, "errorText").text, "Not connected")
        App.dismissError()
        verify(!banner.visible)
    }

    function test_traceRowCanBeSavedThroughTheDialog() {
        verify(Helper.connectFake())
        Helper.receive("18FF9341", "32 FF")
        window.openSaveDialog(0)
        const dialog = findChild(window, "saveMessageDialog")
        verify(dialog)
        tryCompare(dialog, "opened", true)
        compare(findChild(dialog, "dialogId").text, "18FF9341")
        compare(findChild(dialog, "dialogPayload").text, "32 FF")
        findChild(dialog, "dialogName").text = "Engine"
        findChild(dialog, "dialogPeriod").value = 100
        dialog.accept()
        tryCompare(App.library, "count", 1)
        compare(App.traceRowName(0), "Engine")
    }

    function test_statusBarCounts() {
        verify(Helper.connectFake())
        Helper.receive("123", "")
        tryCompare(findChild(window, "statusShown"), "text", "1 rows shown")
        verify(findChild(window, "statusRx").text.startsWith("RX 1"))
    }
}
