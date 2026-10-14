import QtQuick
import QtTest
import UsbCan
import UsbCan.Backend

Item {
    width: 500
    height: 800

    SendPanel {
        id: panel
        anchors.fill: parent
    }

    TestCase {
        name: "SendPanel"
        when: windowShown

        function init() {
            const dialog = findChild(panel, "saveMessageDialog")
            dialog.close()
            tryCompare(dialog, "visible", false) // a closing modal dialog still eats clicks
            Helper.reset()
            verify(Helper.connectFake())
            findChild(panel, "newIdField").clear()
            findChild(panel, "newPayloadField").clear()
        }

        function type(objectName, text) {
            const field = findChild(panel, objectName)
            field.forceActiveFocus()
            for (const c of text)
                keyClick(c)
        }

        function cards() {
            const list = findChild(panel, "libraryList")
            list.forceLayout()
            const result = []
            for (let i = 0; i < list.contentItem.children.length; ++i) {
                const item = list.contentItem.children[i]
                if (item.objectName === "messageCard")
                    result[item.index] = item
            }
            return result
        }

        function test_sendTypedFrame() {
            type("newIdField", "18ff9341")
            verify(findChild(panel, "newExtCheck").checked)
            type("newPayloadField", "32 ff")
            const send = findChild(panel, "sendButton")
            verify(send.enabled)
            mouseClick(send)
            compare(Helper.sentCount(), 1)
            compare(Helper.lastSent(), "18FF9341#32FF")
        }

        function test_invalidInputIsExplained() {
            type("newIdField", "xyz")
            const error = findChild(panel, "newFrameError")
            verify(error.visible)
            compare(error.text, "ID must be hexadecimal")
            verify(!findChild(panel, "sendButton").enabled)
            verify(!findChild(panel, "saveAsButton").enabled)
        }

        function test_saveAsAddsToLibraryAndSends() {
            type("newIdField", "18ff9241")
            type("newPayloadField", "00 f0")
            mouseClick(findChild(panel, "saveAsButton"))
            const dialog = findChild(panel, "saveMessageDialog")
            tryCompare(dialog, "opened", true)
            compare(findChild(dialog, "dialogId").text, "18FF9241")
            compare(findChild(dialog, "dialogPayload").text, "00 F0")
            for (const c of "Dash")
                keyClick(c)
            dialog.accept()
            tryCompare(App.library, "count", 1)
            tryCompare(dialog, "visible", false)

            const card = cards()[0]
            verify(card)
            compare(findChild(card, "messageName").text, "Dash")
            compare(card.canId, "18FF9241")
            mouseClick(findChild(card, "sendOnceButton"))
            compare(Helper.lastSent(), "18FF9241#00F0")
        }

        function test_cyclicAndEdit() {
            App.saveMessage("Tick", "123", false, "01", 20, "")
            tryCompare(App.library, "count", 1)
            const card = cards()[0]
            const cyclic = findChild(card, "cyclicButton")
            const stopAll = findChild(panel, "stopAllButton")
            compare(cyclic.text, "Every 20 ms")
            verify(!stopAll.enabled)
            mouseClick(cyclic)
            tryCompare(card, "running", true)
            verify(stopAll.enabled)
            compare(stopAll.text, "■  Stop all (1)")
            tryVerify(() => Helper.sentCount() >= 3, 2000)
            mouseClick(stopAll)
            tryCompare(card, "running", false)
            verify(!stopAll.enabled)

            card.startEdit()
            verify(card.editing)
            const name = findChild(card, "nameEdit")
            name.text = "Tock"
            findChild(card, "periodEdit").value = 0
            mouseClick(findChild(card, "applyButton"))
            verify(!card.editing)
            compare(App.library.count, 1)
            tryCompare(card, "name", "Tock")
            compare(card.periodMs, 0)
            verify(!findChild(card, "cyclicButton").visible)
        }

        function test_editSwitchesFrameType() {
            App.saveMessage("Ext", "18FF9341", true, "", 0, "")
            const card = cards()[0]

            card.startEdit()
            findChild(card, "idEdit").text = "7FF"
            findChild(card, "extEdit").checked = false
            mouseClick(findChild(card, "applyButton"))
            tryCompare(card, "canId", "7FF")
            verify(!card.extended)

            card.startEdit()
            findChild(card, "idEdit").text = "1FFFFFFF"
            verify(!findChild(card, "applyButton").enabled) // too big for 11 bit
            compare(findChild(card, "editError").text, "Standard ID must be at most 7FF")
            findChild(card, "extEdit").checked = true
            mouseClick(findChild(card, "applyButton"))
            tryCompare(card, "canId", "1FFFFFFF")
            verify(card.extended)
        }
    }
}
