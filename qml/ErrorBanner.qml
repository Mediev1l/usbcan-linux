import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import UsbCan.Backend

// Last error from the backend; stays until dismissed or replaced.
Rectangle {
    objectName: "errorBanner"
    visible: App.errorText !== ""
    implicitHeight: visible ? row.implicitHeight + 12 : 0
    color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.18)

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 4

        Label {
            text: "⚠"
            color: Theme.error
            font.pixelSize: 16
        }
        Label {
            objectName: "errorText"
            Layout.fillWidth: true
            text: App.errorText
            wrapMode: Text.Wrap
        }
        ToolButton {
            text: "✕"
            onClicked: App.dismissError()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Dismiss")
        }
    }
}
