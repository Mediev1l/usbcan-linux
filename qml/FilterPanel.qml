import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import UsbCan.Backend

// Every identifier seen so far, with a checkbox deciding whether it is shown.
Pane {
    id: root
    objectName: "filterPanel"
    implicitWidth: 240
    Material.background: Theme.surface

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        PanelHeader {
            Layout.fillWidth: true
            title: qsTr("Filters")
            subtitle: App.filterModel.hiddenCount > 0
                      ? qsTr("%1 IDs, %2 hidden").arg(App.filterModel.count).arg(App.filterModel.hiddenCount)
                      : qsTr("%1 IDs").arg(App.filterModel.count)
        }

        TextField {
            objectName: "filterSearch"
            Layout.fillWidth: true
            placeholderText: qsTr("Search ID, name, PGN…")
            text: App.filterSearch
            onTextEdited: App.filterSearch = text
            selectByMouse: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            Button {
                objectName: "showAllButton"
                text: qsTr("All")
                flat: true
                onClicked: App.filterModel.setAll(true)
            }
            Button {
                objectName: "hideAllButton"
                text: qsTr("None")
                flat: true
                onClicked: App.filterModel.setAll(false)
            }
            Button {
                objectName: "invertButton"
                text: qsTr("Invert")
                flat: true
                onClicked: App.filterModel.invert()
            }
            Item { Layout.fillWidth: true }
            Button {
                objectName: "forgetButton"
                text: qsTr("Forget")
                flat: true
                onClicked: App.forgetIds()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Clear the list and unhide all IDs. IDs still on the bus reappear as their next frame arrives; IDs that went quiet disappear.")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Switch {
                objectName: "newIdsSwitch"
                text: qsTr("Show new IDs")
                checked: App.newIdsVisible
                onToggled: App.newIdsVisible = checked
                ToolTip.visible: hovered
                ToolTip.text: qsTr("When off, newly appearing IDs are added unchecked")
            }
            Item { Layout.fillWidth: true }
            CheckBox {
                objectName: "showRxCheck"
                text: "RX"
                checked: App.trace.showRx
                onToggled: App.trace.showRx = checked
            }
            CheckBox {
                objectName: "showTxCheck"
                text: "TX"
                checked: App.trace.showTx
                onToggled: App.trace.showTx = checked
            }
        }

        ListView {
            id: list
            objectName: "filterList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.filters
            spacing: 2
            ScrollBar.vertical: ScrollBar {}

            add: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 400 }
            }

            delegate: ItemDelegate {
                id: row
                objectName: "filterRow"
                required property int index
                required property var key
                required property string canId
                required property string name
                required property string pgn
                required property string pgnName
                required property string sourceAddress
                required property var count
                required property real rateHz
                required property var model

                // "checked" is a FINAL property of ItemDelegate: read the role via model.
                readonly property bool shown: row.model.checked

                // Brief highlight after every toggle, so a click is visibly registered.
                readonly property bool flashing: row.down || flashTimer.running
                readonly property color textColor: flashing ? Theme.pink : Theme.foreground

                function toggle() {
                    row.model.checked = !row.shown
                    flashTimer.restart()
                }

                width: list.width
                padding: 6
                onClicked: toggle()
                onDoubleClicked: App.filterModel.showOnlyKey(row.key)
                ToolTip.visible: hovered && row.pgnName !== ""
                ToolTip.text: row.pgnName
                ToolTip.delay: 600

                background: Rectangle {
                    radius: Theme.radius
                    color: row.flashing ? Theme.currentLine
                                        : (row.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent")
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }

                Timer {
                    id: flashTimer
                    interval: 350
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: rowMenu.popup()
                }

                Menu {
                    id: rowMenu
                    objectName: "filterRowMenu"
                    MenuItem {
                        objectName: "saveLastFrameItem"
                        text: qsTr("Save last frame to library…")
                        onTriggered: saveDialog.openForFrame(App.lastFrame(row.key))
                    }
                    MenuItem {
                        text: qsTr("Show only this ID")
                        onTriggered: App.filterModel.showOnlyKey(row.key)
                    }
                    MenuItem {
                        text: row.shown ? qsTr("Hide this ID") : qsTr("Show this ID")
                        onTriggered: row.toggle()
                    }
                }

                contentItem: RowLayout {
                    spacing: Theme.spacing
                    CheckBox {
                        objectName: "filterCheck"
                        checked: row.shown
                        onToggled: {
                            row.model.checked = checked
                            flashTimer.restart()
                        }
                        padding: 0
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        RowLayout {
                            Label {
                                text: row.canId
                                font.family: Theme.mono
                                font.bold: true
                                color: row.textColor
                                opacity: row.shown || row.flashing ? 1 : 0.5
                            }
                            Label {
                                text: row.name
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                color: row.textColor
                                opacity: row.shown || row.flashing ? 1 : 0.5
                            }
                        }
                        Label {
                            visible: row.pgn !== ""
                            text: qsTr("PGN %1 · SA %2").arg(row.pgn).arg(row.sourceAddress)
                            color: Theme.subtle
                            font.pixelSize: 11
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: row.count
                            font.family: Theme.mono
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: row.rateHz > 0 ? row.rateHz.toFixed(1) + " Hz" : ""
                            color: Theme.subtle
                            font.pixelSize: 11
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 24
                visible: list.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.subtle
                text: qsTr("IDs appear here as soon as they are seen on the bus.")
            }
        }
    }

    SaveMessageDialog {
        id: saveDialog
    }
}
