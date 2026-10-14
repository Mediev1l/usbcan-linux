import QtQuick
import QtTest
import UsbCan
import UsbCan.Backend

Item {
    width: 400
    height: 700

    FilterPanel {
        id: panel
        anchors.fill: parent
    }

    TestCase {
        name: "FilterPanel"
        when: windowShown

        function init() {
            Helper.reset()
            verify(Helper.connectFake())
            Helper.receive("18FF9341", "01")
            Helper.receive("18FF9241", "02")
            tryCompare(findChild(panel, "filterList"), "count", 2)
        }

        function rows() {
            const list = findChild(panel, "filterList")
            list.forceLayout()
            const result = []
            for (let i = 0; i < list.contentItem.children.length; ++i) {
                const item = list.contentItem.children[i]
                if (item.objectName === "filterRow")
                    result[item.index] = item
            }
            return result
        }

        function test_newIdsAppearChecked() {
            const r = rows()
            compare(r.length, 2)
            verify(r[0].shown)
            compare(r[0].canId, "18FF9341")
            compare(r[0].pgn, "FF93")
        }

        function test_uncheckingHidesFramesInTheTrace() {
            mouseClick(findChild(rows()[0], "filterCheck"))
            tryCompare(App.filterModel, "hiddenCount", 1)
            compare(App.trace.count, 1)

            mouseClick(findChild(panel, "showAllButton"))
            compare(App.filterModel.hiddenCount, 0)
            mouseClick(findChild(panel, "hideAllButton"))
            compare(App.filterModel.hiddenCount, 2)
            compare(App.trace.count, 0)
            mouseClick(findChild(panel, "invertButton"))
            compare(App.filterModel.hiddenCount, 0)
        }

        function test_clickingRowTogglesAndDoubleClickShowsOnly() {
            const r = rows()
            mouseClick(r[1], r[1].width - 20, 10)
            tryCompare(App.filterModel, "hiddenCount", 1)
            mouseClick(r[1], r[1].width - 20, 10)
            tryCompare(App.filterModel, "hiddenCount", 0)
            mouseDoubleClickSequence(r[0], r[0].width - 20, 10)
            tryCompare(App.filterModel, "hiddenCount", 1)
            verify(rows()[0].shown)
        }

        function test_clickFlashesTheRow() {
            const r = rows()[0]
            verify(!r.flashing)
            mouseClick(r, r.width - 20, 10)
            verify(r.flashing)
            compare(r.textColor, Theme.pink)
            tryCompare(r, "flashing", false, 2000)
            compare(r.textColor, Theme.foreground)
        }

        function test_forgetDropsQuietIdsAndUnhides() {
            mouseClick(findChild(rows()[0], "filterCheck"))
            tryCompare(App.filterModel, "hiddenCount", 1)
            mouseClick(findChild(panel, "forgetButton"))
            tryCompare(findChild(panel, "filterList"), "count", 0)
            compare(App.filterModel.hiddenCount, 0)
            compare(App.trace.count, 2) // the trace itself is untouched

            Helper.receive("18FF9241", "05") // still on the bus
            tryCompare(findChild(panel, "filterList"), "count", 1)
            compare(rows()[0].canId, "18FF9241")
        }

        function test_rowMenuSavesLastFrame() {
            Helper.receive("18FF9341", "32 FF")
            const r = rows()[0]
            mouseClick(r, r.width / 2, 10, Qt.RightButton)
            const menu = findChild(r, "filterRowMenu")
            tryCompare(menu, "opened", true)
            mouseClick(findChild(menu, "saveLastFrameItem"))
            const dialog = findChild(panel, "saveMessageDialog")
            tryCompare(dialog, "opened", true)
            compare(findChild(dialog, "dialogId").text, "18FF9341")
            compare(findChild(dialog, "dialogPayload").text, "32 FF")
            findChild(dialog, "dialogName").text = "From filters"
            dialog.accept()
            tryCompare(App.library, "count", 1)
            tryCompare(dialog, "visible", false)
            compare(App.lastFrame(r.key).name, "From filters")
        }

        function test_newIdsCanStartHidden() {
            mouseClick(findChild(panel, "newIdsSwitch"))
            verify(!App.newIdsVisible)
            Helper.receive("18F00400", "03")
            tryCompare(findChild(panel, "filterList"), "count", 3)
            compare(App.filterModel.hiddenCount, 1)
        }

        function test_searchAndDirection() {
            const field = findChild(panel, "filterSearch")
            field.forceActiveFocus()
            for (const c of "9241")
                keyClick(c)
            tryCompare(findChild(panel, "filterList"), "count", 1)

            mouseClick(findChild(panel, "showRxCheck"))
            verify(!App.trace.showRx)
            compare(App.trace.count, 0)
            mouseClick(findChild(panel, "showRxCheck"))
            mouseClick(findChild(panel, "showTxCheck"))
            verify(!App.trace.showTx)
            mouseClick(findChild(panel, "showTxCheck"))
        }
    }
}
