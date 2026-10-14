import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import UsbCan.Backend

// Quick frame editor plus the list of saved messages.
Pane {
    id: root
    objectName: "sendPanel"
    implicitWidth: 240
    Material.background: Theme.surface

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        PanelHeader {
            Layout.fillWidth: true
            title: qsTr("Send")
            subtitle: App.connected ? "" : qsTr("not connected")
        }

        // --- new frame ------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: editor.implicitHeight + 2 * Theme.spacing
            radius: Theme.radius
            color: Theme.card
            border.color: Theme.divider

            ColumnLayout {
                id: editor
                anchors.fill: parent
                anchors.margins: Theme.spacing
                spacing: 4

                readonly property string idError: App.validateId(idField.text, extCheck.checked)
                readonly property string payloadError: App.validatePayload(payloadField.text)
                readonly property bool valid: idError === "" && payloadError === ""

                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: idField
                        objectName: "newIdField"
                        Layout.preferredWidth: 130
                        placeholderText: qsTr("ID (hex)")
                        font.family: Theme.mono
                        selectByMouse: true
                        onTextEdited: if (App.looksExtended(text)) extCheck.checked = true
                        color: text !== "" && editor.idError !== "" ? Theme.error : Material.foreground
                    }
                    CheckBox {
                        id: extCheck
                        objectName: "newExtCheck"
                        text: qsTr("29-bit")
                        checked: true
                    }
                    TextField {
                        id: payloadField
                        objectName: "newPayloadField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Data, e.g. 32 FF 0F")
                        font.family: Theme.mono
                        selectByMouse: true
                        color: editor.payloadError !== "" ? Theme.error : Material.foreground
                        onAccepted: sendButton.clicked()
                    }
                }
                Label {
                    objectName: "newFrameError"
                    Layout.fillWidth: true
                    visible: idField.text !== "" && !editor.valid
                    text: editor.idError !== "" ? editor.idError : editor.payloadError
                    color: Theme.error
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "saveAsButton"
                        text: qsTr("Save as…")
                        flat: true
                        enabled: editor.valid
                        onClicked: saveDialog.openForNew(idField.text, extCheck.checked, payloadField.text)
                    }
                    Button {
                        id: sendButton
                        objectName: "sendButton"
                        text: qsTr("Send")
                        highlighted: true
                        enabled: editor.valid && App.connected
                        onClicked: App.sendFrame(idField.text, extCheck.checked, payloadField.text)
                    }
                }
            }
        }

        // --- saved messages ---------------------------------------------------
        PanelHeader {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacing
            title: qsTr("Saved messages")
            subtitle: App.library.count

            Button {
                objectName: "stopAllButton"
                text: App.runningCount > 0 ? qsTr("■  Stop all (%1)").arg(App.runningCount) : qsTr("■  Stop all")
                enabled: App.runningCount > 0
                flat: !enabled
                Material.background: enabled ? Theme.red : "transparent"
                Material.foreground: enabled ? Theme.onBright : Theme.subtle
                font.bold: enabled
                onClicked: App.stopAllCyclic()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Stop all periodic sending")
            }
        }

        ListView {
            id: list
            objectName: "libraryList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: App.library
            ScrollBar.vertical: ScrollBar {}

            delegate: MessageCard {
                width: list.width - 10
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 24
                visible: list.count === 0
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.subtle
                text: qsTr("No saved messages yet.\nRight-click a frame in the trace and choose \"Save to library…\", or use \"Save as…\" above.")
            }
        }
    }

    SaveMessageDialog {
        id: saveDialog
    }
}
