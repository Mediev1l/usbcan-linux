import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Decoded view of a frame: J1939 fields, identifier bits and every byte.
Flow {
    id: root
    objectName: "frameDetails"
    // The FrameDelegate showing these details (untyped: the two files reference each other).
    required property var frame
    spacing: Theme.spacing

    DetailCard {
        objectName: "j1939Card"
        visible: root.frame.isJ1939
        title: qsTr("J1939")

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 2

            Label { text: qsTr("PGN"); color: Theme.subtle }
            Label {
                objectName: "pgnValue"
                text: root.frame.pgn + "  (" + root.frame.pgnDecimal + ")"
                font.family: Theme.mono
                font.bold: true
            }
            Label { text: qsTr("Name"); color: Theme.subtle }
            Label { text: root.frame.pgnName !== "" ? root.frame.pgnName : qsTr("unknown") }
            Label { text: qsTr("Priority"); color: Theme.subtle }
            Label { text: root.frame.priority; font.family: Theme.mono }
            Label { text: qsTr("Source"); color: Theme.subtle }
            Label { text: root.frame.sourceAddress; font.family: Theme.mono }
            Label { text: qsTr("Destination"); color: Theme.subtle }
            Label { text: root.frame.destinationAddress; font.family: Theme.mono }
            Label { text: qsTr("Format"); color: Theme.subtle }
            Label { text: root.frame.pduFormat }
        }
    }

    DetailCard {
        objectName: "idBitsCard"
        title: qsTr("Identifier")

        GridLayout {
            columns: 3
            columnSpacing: 12
            rowSpacing: 2
            Repeater {
                model: root.frame.idFields
                delegate: Label {
                    required property var modelData
                    text: modelData.label
                    color: Theme.subtle
                    Layout.row: index
                    Layout.column: 0
                    required property int index
                }
            }
            Repeater {
                model: root.frame.idFields
                delegate: Label {
                    required property var modelData
                    required property int index
                    text: modelData.bits
                    font.family: Theme.mono
                    Layout.row: index
                    Layout.column: 1
                }
            }
            Repeater {
                model: root.frame.idFields
                delegate: Label {
                    required property var modelData
                    required property int index
                    text: "0x" + modelData.value
                    font.family: Theme.mono
                    color: Theme.subtle
                    Layout.row: index
                    Layout.column: 2
                }
            }
        }
    }

    DetailCard {
        objectName: "bytesCard"
        visible: root.frame.dlc > 0 && !root.frame.rtr
        title: qsTr("Bytes")

        GridLayout {
            columns: 4
            columnSpacing: 12
            rowSpacing: 2

            Label { text: "#"; color: Theme.subtle }
            Label { text: "hex"; color: Theme.subtle }
            Label { text: "dec"; color: Theme.subtle }
            Label { text: "bin"; color: Theme.subtle }

            Repeater {
                model: root.frame.bytes
                delegate: Label {
                    required property var modelData
                    required property int index
                    text: modelData.index
                    font.family: Theme.mono
                    color: Theme.subtle
                    Layout.row: index + 1
                    Layout.column: 0
                }
            }
            Repeater {
                model: root.frame.bytes
                delegate: Label {
                    required property var modelData
                    required property int index
                    text: modelData.hex
                    font.family: Theme.mono
                    font.bold: modelData.changed
                    Layout.row: index + 1
                    Layout.column: 1
                }
            }
            Repeater {
                model: root.frame.bytes
                delegate: Label {
                    required property var modelData
                    required property int index
                    text: modelData.dec
                    font.family: Theme.mono
                    Layout.row: index + 1
                    Layout.column: 2
                }
            }
            Repeater {
                model: root.frame.bytes
                delegate: Label {
                    required property var modelData
                    required property int index
                    text: modelData.bin
                    font.family: Theme.mono
                    Layout.row: index + 1
                    Layout.column: 3
                }
            }
        }
    }
}
