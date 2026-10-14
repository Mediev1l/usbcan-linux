import QtQuick
import QtTest
import UsbCan
import UsbCan.Backend

Item {
    id: root
    width: 1200
    height: 700

    TraceView {
        id: view
        anchors.fill: parent
    }

    TestCase {
        name: "TraceView"
        when: windowShown

        function init() {
            Helper.reset()
            verify(Helper.connectFake())
        }

        function delegateAt(row) {
            const list = findChild(view, "traceList")
            list.forceLayout()
            list.positionViewAtIndex(row, ListView.Contain)
            let found = null
            for (let i = 0; i < list.contentItem.children.length; ++i) {
                const item = list.contentItem.children[i]
                if (item.objectName === "frameDelegate" && item.index === row)
                    found = item
            }
            return found
        }

        function test_frameAppearsWithDecodedId() {
            Helper.receive("18FF9341", "32 FF FF FF FF FF 0F FF")
            const list = findChild(view, "traceList")
            tryCompare(list, "count", 1)
            const row = delegateAt(0)
            verify(row)
            compare(findChild(row, "idLabel").text, "18FF9341")
            compare(row.direction, "RX")
            compare(row.pgnName, "PROPB - Proprietary B")
        }

        function test_clickExpandsJ1939Details() {
            Helper.receive("18FF9341", "32 FF")
            tryCompare(findChild(view, "traceList"), "count", 1)
            const row = delegateAt(0)
            verify(!row.expanded)
            mouseClick(row, 40, 10)
            tryCompare(row, "expanded", true)
            const pgn = findChild(row, "pgnValue")
            verify(pgn)
            compare(pgn.text, "FF93  (65427)")
            verify(findChild(row, "bytesCard").visible)
            verify(findChild(row, "idBitsCard").visible)

            mouseClick(row, 40, 10)
            tryCompare(row, "expanded", false)
        }

        function test_latestModeCollapsesById() {
            Helper.receive("18FF9341", "01")
            Helper.receive("18FF9341", "02")
            Helper.receive("18FF9241", "03")
            const list = findChild(view, "traceList")
            tryCompare(list, "count", 3)
            mouseClick(findChild(view, "latestButton"))
            compare(App.traceMode, TraceModel.LatestPerId)
            tryCompare(list, "count", 2)
            compare(delegateAt(0).payload, "02")
            compare(delegateAt(0).count, 2)
            mouseClick(findChild(view, "chronologicalButton"))
            tryCompare(list, "count", 3)
        }

        function test_pauseAndClear() {
            mouseClick(findChild(view, "pauseButton"))
            verify(App.paused)
            Helper.receive("123", "01")
            compare(findChild(view, "traceList").count, 0)
            compare(App.rxCount, 1)
            mouseClick(findChild(view, "pauseButton"))
            verify(!App.paused)

            Helper.receive("123", "01")
            tryCompare(findChild(view, "traceList"), "count", 1)
            mouseClick(findChild(view, "clearButton"))
            tryCompare(findChild(view, "traceList"), "count", 0)
        }

        function test_searchField() {
            Helper.receive("18FF9341", "01")
            Helper.receive("18FF9241", "02")
            const field = findChild(view, "traceSearch")
            field.forceActiveFocus()
            for (const c of "ff92")
                keyClick(c)
            compare(App.trace.searchText, "ff92")
            tryCompare(findChild(view, "traceList"), "count", 1)
        }

        function test_rowActionsReachTheController() {
            Helper.receive("18FF9341", "32 FF")
            tryCompare(findChild(view, "traceList"), "count", 1)
            const row = delegateAt(0)
            row.resendRequested()
            compare(Helper.lastSent(), "18FF9341#32FF")

            let copied = ""
            view.copyRequested.connect(text => copied = text)
            row.copyRequested()
            compare(copied, "18FF9341#32FF")

            let savedRow = -1
            view.saveRequested.connect(r => savedRow = r)
            row.saveRequested()
            compare(savedRow, 0)

            row.hideRequested() // hides the ID in both directions, including the resend
            Helper.flush()
            tryCompare(findChild(view, "traceList"), "count", 0)
            compare(App.filterModel.hiddenCount, 1)
        }
    }
}
