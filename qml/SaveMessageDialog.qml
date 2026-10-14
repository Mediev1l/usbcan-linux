import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UsbCan.Backend

// Asks for a name (and optional comment/period) before adding a frame to the library.
Dialog {
    id: root
    objectName: "saveMessageDialog"
    title: qsTr("Save message")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 420
    standardButtons: Dialog.Save | Dialog.Cancel

    // Either a trace row or an ID/payload typed in the editor.
    property int traceRow: -1
    property string idText
    property bool extended: true
    property string payloadText

    function openForTraceRow(row) {
        traceRow = row
        idText = App.traceRowId(row)
        extended = App.looksExtended(idText)
        payloadText = App.traceRowPayload(row)
        nameField.text = App.traceRowName(row)
        commentField.text = ""
        periodField.value = 0
        open()
        nameField.forceActiveFocus()
    }

    function openForNew(id, ext, payload, name) {
        traceRow = -1
        idText = id
        extended = ext
        payloadText = payload
        nameField.text = name || ""
        commentField.text = ""
        periodField.value = 0
        open()
        nameField.forceActiveFocus()
    }

    // `frame` as returned by App.lastFrame(): {id, extended, payload, name}.
    function openForFrame(frame) {
        openForNew(frame.id, frame.extended, frame.payload, frame.name)
    }

    onAccepted: {
        if (traceRow >= 0)
            App.saveTraceRow(traceRow, nameField.text, commentField.text, periodField.value)
        else
            App.saveMessage(nameField.text, idText, extended, payloadText, periodField.value, commentField.text)
    }

    GridLayout {
        anchors.fill: parent
        columns: 2
        columnSpacing: Theme.spacing

        Label { text: qsTr("ID") }
        RowLayout {
            Label {
                objectName: "dialogId"
                text: App.displayId(root.idText, root.extended)
                font.family: Theme.mono
                font.bold: true
            }
            Label {
                text: root.extended ? qsTr("29-bit") : qsTr("11-bit")
                color: Theme.subtle
            }
        }
        Label { text: qsTr("Payload") }
        Label {
            objectName: "dialogPayload"
            readonly property string value: App.displayPayload(root.payloadText)
            text: value !== "" ? value : qsTr("no data")
            font.family: Theme.mono
            font.italic: value === ""
            color: value !== "" ? Theme.foreground : Theme.subtle
        }
        Label { text: qsTr("Name") }
        TextField {
            id: nameField
            objectName: "dialogName"
            Layout.fillWidth: true
            placeholderText: qsTr("e.g. Display brightness request")
            selectByMouse: true
            onAccepted: root.accept()
        }
        Label { text: qsTr("Period") }
        RowLayout {
            SpinBox {
                id: periodField
                objectName: "dialogPeriod"
                from: 0
                to: 600000
                stepSize: 10
                editable: true
            }
            Label { text: qsTr("ms (0 = send once)") }
        }
        Label { text: qsTr("Comment") }
        TextField {
            id: commentField
            objectName: "dialogComment"
            Layout.fillWidth: true
            selectByMouse: true
        }
    }
}
