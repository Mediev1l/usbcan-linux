import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.settings
import UsbCan.Backend

ApplicationWindow {
    id: window
    objectName: "mainWindow"
    width: 1440
    height: 860
    minimumWidth: 960
    minimumHeight: 560
    visible: true
    title: "usbcan-linux"

    // Dracula: dark Material style recoloured with the Theme palette.
    Material.theme: Material.Dark
    Material.background: Theme.background
    Material.foreground: Theme.foreground
    Material.accent: Theme.accent
    Material.primary: Theme.currentLine
    color: Theme.background

    // Hidden editor used to put text on the clipboard.
    function copyText(text) {
        clipboardHelper.text = text
        clipboardHelper.selectAll()
        clipboardHelper.copy()
    }

    function openSaveDialog(traceRow) {
        saveDialog.openForTraceRow(traceRow)
    }

    TextEdit {
        id: clipboardHelper
        visible: false
    }

    Settings {
        category: "window"
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
        property alias filtersVisible: filterPanel.visible
        property alias sendVisible: sendPanel.visible
        property alias splitState: window.splitState
    }
    property var splitState

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("Export trace…")
                onTriggered: exportTraceDialog.open()
            }
            Action {
                text: qsTr("Import trace…")
                onTriggered: importTraceDialog.open()
            }
            MenuSeparator {}
            Action {
                text: qsTr("Import saved messages…")
                onTriggered: importLibraryDialog.open()
            }
            Action {
                text: qsTr("Export saved messages…")
                onTriggered: exportLibraryDialog.open()
            }
            MenuSeparator {}
            Action {
                text: qsTr("Quit")
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: qsTr("&View")
            Action {
                text: qsTr("Filters panel")
                checkable: true
                checked: filterPanel.visible
                onTriggered: filterPanel.visible = checked
            }
            Action {
                text: qsTr("Send panel")
                checkable: true
                checked: sendPanel.visible
                onTriggered: sendPanel.visible = checked
            }
        }
    }

    header: ConnectionBar {}
    footer: StatusBar {}

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ErrorBanner {
            Layout.fillWidth: true
        }

        SplitView {
            id: split
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            Component.onCompleted: if (window.splitState) split.restoreState(window.splitState)
            Component.onDestruction: window.splitState = split.saveState()

            FilterPanel {
                id: filterPanel
                SplitView.preferredWidth: 330
                SplitView.minimumWidth: 240
            }
            TraceView {
                id: traceView
                SplitView.fillWidth: true
                SplitView.minimumWidth: 420
                onSaveRequested: row => window.openSaveDialog(row)
                onCopyRequested: text => window.copyText(text)
            }
            SendPanel {
                id: sendPanel
                SplitView.preferredWidth: 430
                SplitView.minimumWidth: 320
            }
        }
    }

    SaveMessageDialog {
        id: saveDialog
    }

    FileDialog {
        id: exportTraceDialog
        title: qsTr("Export trace")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("CSV (*.csv)"), qsTr("candump log (*.log)"), qsTr("Vector ASC (*.asc)")]
        onAccepted: App.exportTrace(selectedFile.toString())
    }
    FileDialog {
        id: importTraceDialog
        title: qsTr("Import trace")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Trace files (*.csv *.log *.candump)")]
        onAccepted: App.importTrace(selectedFile.toString())
    }
    FileDialog {
        id: importLibraryDialog
        title: qsTr("Import saved messages")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Saved messages (*.canlib.json *.json)")]
        onAccepted: App.importLibrary(selectedFile.toString())
    }
    FileDialog {
        id: exportLibraryDialog
        title: qsTr("Export saved messages")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "canlib.json"
        nameFilters: [qsTr("Saved messages (*.canlib.json)")]
        onAccepted: App.exportLibrary(selectedFile.toString())
    }
}
