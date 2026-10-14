import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One trace row. Click to expand into decoded details; right-click for actions.
Item {
    id: root
    objectName: "frameDelegate"

    required property int index
    required property var key
    required property var seq
    required property string time
    required property string direction
    required property string canId
    required property string name
    required property int dlc
    required property string payload
    required property int changedMask
    required property var count
    required property real periodMs
    required property bool isJ1939
    required property string pgn
    required property int pgnDecimal
    required property string pgnName
    required property int priority
    required property string sourceAddress
    required property string destinationAddress
    required property string pduFormat
    required property var idFields
    required property var bytes
    required property bool rtr

    property bool latestMode: false
    property bool expanded: false

    signal toggleRequested
    signal saveRequested
    signal resendRequested
    signal copyRequested
    signal hideRequested

    implicitHeight: column.implicitHeight

    Rectangle {
        anchors.fill: parent
        color: root.expanded ? Theme.selection : (mouse.containsMouse ? Qt.rgba(0.5, 0.5, 0.5, 0.08) : "transparent")
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: event => {
            if (event.button === Qt.RightButton)
                menu.popup()
            else
                root.toggleRequested()
        }
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 0

        RowLayout {
            objectName: "frameRow"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.rowHeight
            spacing: Theme.spacing

            Label {
                text: root.expanded ? "▾" : "▸"
                color: Theme.subtle
                Layout.preferredWidth: 18
                horizontalAlignment: Text.AlignHCenter
            }
            Label {
                visible: !root.latestMode
                text: root.time
                font.family: Theme.mono
                color: Theme.subtle
                Layout.preferredWidth: 96
            }
            Label {
                visible: !root.latestMode
                text: root.direction
                font.bold: true
                color: root.direction === "TX" ? Theme.tx : Theme.rx
                Layout.preferredWidth: 34
            }
            Label {
                objectName: "idLabel"
                text: root.canId
                font.family: Theme.mono
                font.bold: true
                Layout.preferredWidth: 84
            }
            Label {
                text: root.name !== "" ? root.name : (root.isJ1939 && root.pgnName !== "" ? root.pgnName : "")
                color: root.name !== "" ? Material.foreground : Theme.subtle
                font.italic: root.name === ""
                elide: Text.ElideRight
                Layout.preferredWidth: 150
                Layout.minimumWidth: 150
                Layout.fillWidth: true
                Layout.maximumWidth: 220
            }
            Label {
                text: root.dlc
                font.family: Theme.mono
                Layout.preferredWidth: 30
            }
            Row {
                objectName: "payloadBytes"
                Layout.fillWidth: true
                Layout.minimumWidth: implicitWidth
                spacing: 2
                visible: !root.rtr
                Repeater {
                    model: root.bytes
                    delegate: Rectangle {
                        required property var modelData
                        width: byteText.implicitWidth + 4
                        height: byteText.implicitHeight + 2
                        radius: 3
                        color: modelData.changed ? Theme.changed : "transparent"
                        Behavior on color {
                            ColorAnimation { duration: 300 }
                        }
                        Label {
                            id: byteText
                            anchors.centerIn: parent
                            text: modelData.hex
                            font.family: Theme.mono
                            color: modelData.changed ? Theme.onBright : Theme.foreground
                        }
                    }
                }
            }
            Label {
                visible: root.rtr
                text: qsTr("remote request")
                font.italic: true
                color: Theme.subtle
                Layout.fillWidth: true
            }
            Label {
                visible: root.latestMode
                text: root.count
                font.family: Theme.mono
                horizontalAlignment: Text.AlignRight
                Layout.preferredWidth: 60
            }
            Label {
                visible: root.latestMode
                text: root.periodMs > 0 ? root.periodMs.toFixed(1) + " ms" : "–"
                font.family: Theme.mono
                color: Theme.subtle
                horizontalAlignment: Text.AlignRight
                Layout.preferredWidth: 76
                Layout.rightMargin: Theme.spacing
            }
        }

        Loader {
            objectName: "detailsLoader"
            Layout.fillWidth: true
            Layout.leftMargin: 26
            Layout.rightMargin: Theme.spacing
            Layout.bottomMargin: Theme.spacing
            active: root.expanded
            visible: active
            sourceComponent: FrameDetails {
                frame: root
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.divider
        }
    }

    Menu {
        id: menu
        MenuItem {
            text: qsTr("Save to library…")
            onTriggered: root.saveRequested()
        }
        MenuItem {
            text: qsTr("Send again")
            onTriggered: root.resendRequested()
        }
        MenuItem {
            text: qsTr("Copy as cansend")
            onTriggered: root.copyRequested()
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Hide this ID")
            onTriggered: root.hideRequested()
        }
    }
}
