import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import UsbCan.Backend

// One saved message: name, ID, payload and how often to send it.
Rectangle {
    id: root
    objectName: "messageCard"

    required property int index
    required property string name
    required property string canId
    required property bool extended
    required property string payload
    required property int periodMs
    required property string comment
    required property bool running
    required property var model

    property bool editing: false

    implicitHeight: content.implicitHeight + 2 * Theme.spacing
    radius: Theme.radius
    color: Theme.card
    border.color: running ? Theme.ok : Theme.divider
    border.width: running ? 2 : 1

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: 4

        // --- view mode --------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            visible: !root.editing
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label {
                    objectName: "messageName"
                    text: root.name
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: root.canId + "  ·  " + (root.payload !== "" ? root.payload : qsTr("no data"))
                    font.family: Theme.mono
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    visible: root.comment !== ""
                    text: root.comment
                    color: Theme.subtle
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
            Button {
                objectName: "sendOnceButton"
                text: qsTr("Send")
                flat: true
                enabled: App.connected
                onClicked: App.sendLibraryRow(root.index)
            }
            Button {
                objectName: "cyclicButton"
                text: root.running ? qsTr("Stop") : (root.periodMs > 0 ? qsTr("Every %1 ms").arg(root.periodMs) : qsTr("Once"))
                visible: root.periodMs > 0 || root.running
                highlighted: root.running
                enabled: App.connected || root.running
                onClicked: App.toggleCyclic(root.index)
            }
            ToolButton {
                objectName: "cardMenuButton"
                text: "⋮"
                onClicked: cardMenu.popup()
            }
        }

        // --- edit mode ---------------------------------------------------------
        GridLayout {
            objectName: "editForm"
            Layout.fillWidth: true
            visible: root.editing
            columns: 2
            columnSpacing: Theme.spacing
            rowSpacing: 2

            Label { text: qsTr("Name") }
            TextField {
                id: nameEdit
                objectName: "nameEdit"
                Layout.fillWidth: true
                selectByMouse: true
            }
            Label { text: qsTr("ID") }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: idEdit
                    objectName: "idEdit"
                    Layout.fillWidth: true
                    font.family: Theme.mono
                    selectByMouse: true
                    color: App.validateId(text, extEdit.checked) === "" ? Material.foreground : Theme.error
                }
                CheckBox {
                    id: extEdit
                    objectName: "extEdit"
                    text: qsTr("29-bit")
                }
            }
            Label { text: qsTr("Data") }
            TextField {
                id: payloadEdit
                objectName: "payloadEdit"
                Layout.fillWidth: true
                font.family: Theme.mono
                selectByMouse: true
                color: App.validatePayload(text) === "" ? Material.foreground : Theme.error
            }
            Label { text: qsTr("Period") }
            RowLayout {
                SpinBox {
                    id: periodEdit
                    objectName: "periodEdit"
                    from: 0
                    to: 600000
                    stepSize: 10
                    editable: true
                }
                Label { text: qsTr("ms (0 = send once)"); color: Theme.subtle }
            }
            Label { text: qsTr("Comment") }
            TextField {
                id: commentEdit
                Layout.fillWidth: true
                selectByMouse: true
            }
            Item { Layout.columnSpan: 1 }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Label {
                    objectName: "editError"
                    color: Theme.error
                    text: App.validateId(idEdit.text, extEdit.checked) || App.validatePayload(payloadEdit.text)
                    font.pixelSize: 11
                }
                Button {
                    text: qsTr("Cancel")
                    flat: true
                    onClicked: root.editing = false
                }
                Button {
                    objectName: "applyButton"
                    text: qsTr("Save")
                    highlighted: true
                    enabled: App.validateId(idEdit.text, extEdit.checked) === "" && App.validatePayload(payloadEdit.text) === ""
                    onClicked: root.applyEdit()
                }
            }
        }
    }

    function startEdit() {
        nameEdit.text = root.name
        idEdit.text = root.canId
        extEdit.checked = root.extended
        payloadEdit.text = root.payload
        periodEdit.value = root.periodMs
        commentEdit.text = root.comment
        root.editing = true
    }

    function applyEdit() {
        // Frame type first so a standard ID is accepted after switching.
        if (!extEdit.checked) {
            root.model.canId = idEdit.text
            root.model.extended = false
        } else {
            root.model.extended = true
            root.model.canId = idEdit.text
        }
        root.model.name = nameEdit.text
        root.model.payload = payloadEdit.text
        root.model.periodMs = periodEdit.value
        root.model.comment = commentEdit.text
        root.editing = false
    }

    Menu {
        id: cardMenu
        MenuItem {
            text: qsTr("Edit…")
            onTriggered: root.startEdit()
        }
        MenuItem {
            text: qsTr("Move up")
            enabled: root.index > 0
            onTriggered: App.library.move(root.index, root.index - 1)
        }
        MenuItem {
            text: qsTr("Move down")
            enabled: root.index < App.library.count - 1
            onTriggered: App.library.move(root.index, root.index + 1)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Delete")
            onTriggered: App.library.remove(root.index)
        }
    }
}
