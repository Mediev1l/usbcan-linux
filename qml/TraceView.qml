import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import UsbCan.Backend

// The frame list: toolbar, column header and expandable rows.
Pane {
    id: root
    objectName: "traceView"
    padding: 0
    // Keep SplitView from growing this panel to the toolbar's natural width
    // (it would then slide under the send panel).
    implicitWidth: 420

    signal saveRequested(int row)
    signal copyRequested(string text)

    readonly property bool latestMode: App.traceMode === TraceModel.LatestPerId
    property bool autoScroll: true
    // Expanded rows: by identifier in "latest" mode, by frame otherwise.
    property var expanded: ({})

    function rowUid(key, seq) {
        return latestMode ? "id" + key : "f" + seq
    }
    function toggleExpanded(uid) {
        const e = Object.assign({}, expanded)
        if (e[uid])
            delete e[uid]
        else
            e[uid] = true
        expanded = e
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- toolbar (wraps on narrow windows) ---------------------------------
        Flow {
            Layout.fillWidth: true
            Layout.margins: Theme.spacing
            spacing: Theme.spacing

            TabBar {
                id: modeTabs
                objectName: "modeTabs"
                currentIndex: App.traceMode
                onCurrentIndexChanged: if (currentIndex !== App.traceMode) App.traceMode = currentIndex
                Connections {
                    target: App
                    function onTraceModeChanged() { modeTabs.currentIndex = App.traceMode }
                }
                TabButton {
                    objectName: "chronologicalButton"
                    text: qsTr("Chronological")
                    width: implicitWidth
                }
                TabButton {
                    objectName: "latestButton"
                    text: qsTr("Latest per ID")
                    width: implicitWidth
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("One row per identifier, changed bytes highlighted")
                }
            }
            Button {
                objectName: "pauseButton"
                text: App.paused ? qsTr("▶ Resume") : qsTr("⏸ Pause")
                flat: true
                onClicked: App.paused = !App.paused
            }
            Button {
                objectName: "clearButton"
                text: qsTr("Clear")
                flat: true
                onClicked: {
                    root.expanded = {}
                    App.clearTrace()
                }
            }
            CheckBox {
                objectName: "autoScrollCheck"
                text: qsTr("Auto-scroll")
                checked: root.autoScroll
                enabled: !root.latestMode
                onToggled: root.autoScroll = checked
            }
            TextField {
                objectName: "traceSearch"
                width: 240
                placeholderText: qsTr("Search ID, name, PGN, data…")
                text: App.trace.searchText
                onTextEdited: App.trace.searchText = text
                selectByMouse: true
            }
        }

        // --- rows (scroll sideways when the panel is narrower than the columns) ---
        ListView {
            id: list
            objectName: "traceList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.trace
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.AutoFlickIfNeeded
            contentWidth: Math.max(width, headerItem ? headerItem.implicitWidth : 0)
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AlwaysOn
            }
            ScrollBar.horizontal: ScrollBar {}

            headerPositioning: ListView.OverlayHeader
            header: Rectangle {
                z: 2
                width: list.contentWidth
                implicitWidth: headerRow.implicitWidth
                implicitHeight: headerRow.implicitHeight + 8
                color: Theme.card

                FrameColumns {
                    id: headerRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    latestMode: root.latestMode
                }
            }

            delegate: FrameDelegate {
                width: list.contentWidth
                latestMode: root.latestMode
                expanded: root.expanded[root.rowUid(key, seq)] === true
                onToggleRequested: root.toggleExpanded(root.rowUid(key, seq))
                onSaveRequested: root.saveRequested(index)
                onResendRequested: App.resendTraceRow(index)
                onCopyRequested: root.copyRequested(App.cansendText(index))
                onHideRequested: App.hideTraceRow(index)
            }

            onCountChanged: {
                if (root.autoScroll && !root.latestMode)
                    Qt.callLater(list.positionViewAtEnd)
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                color: Theme.subtle
                horizontalAlignment: Text.AlignHCenter
                text: App.connected
                      ? qsTr("Waiting for frames…")
                      : qsTr("Not connected.\nChoose an adapter above and press Connect.")
            }
        }
    }

    component FrameColumns: RowLayout {
        id: columns
        property bool latestMode: false
        spacing: Theme.spacing

        Item { implicitWidth: 18 }
        Label { text: qsTr("Time"); visible: !columns.latestMode; Layout.preferredWidth: 96; font.bold: true }
        Label { text: qsTr("Dir"); visible: !columns.latestMode; Layout.preferredWidth: 34; font.bold: true }
        Label { text: qsTr("ID"); Layout.preferredWidth: 84; font.bold: true }
        Label { text: qsTr("Name"); Layout.preferredWidth: 150; Layout.minimumWidth: 150; Layout.fillWidth: true; Layout.maximumWidth: 220; font.bold: true }
        Label { text: qsTr("DLC"); Layout.preferredWidth: 30; font.bold: true }
        Label { text: qsTr("Data"); Layout.fillWidth: true; Layout.minimumWidth: 8 * 24; Layout.preferredWidth: 8 * 24; font.bold: true }
        Label { text: qsTr("Count"); visible: columns.latestMode; Layout.preferredWidth: 60; horizontalAlignment: Text.AlignRight; font.bold: true }
        Label { text: qsTr("Period"); visible: columns.latestMode; Layout.preferredWidth: 76; Layout.rightMargin: Theme.spacing; horizontalAlignment: Text.AlignRight; font.bold: true }
    }
}
