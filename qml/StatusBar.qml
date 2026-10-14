import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import UsbCan.Backend

ToolBar {
    objectName: "statusBar"
    Material.background: Theme.surface
    Material.foreground: Theme.foreground
    implicitHeight: 28

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 18

        Label {
            objectName: "statusConnection"
            text: App.connectionLabel
        }
        Label {
            objectName: "statusRx"
            text: qsTr("RX %1  (%2 fps)").arg(App.rxCount).arg(App.rxRate.toFixed(0))
            color: Theme.rx
            font.family: Theme.mono
        }
        Label {
            objectName: "statusTx"
            text: qsTr("TX %1").arg(App.txCount)
            color: Theme.tx
            font.family: Theme.mono
        }
        Label {
            visible: App.busStatus !== ""
            text: App.busStatus
            color: Theme.subtle
        }
        Item { Layout.fillWidth: true }
        Label {
            visible: App.paused
            text: qsTr("PAUSED")
            color: Theme.tx
            font.bold: true
        }
        Label {
            objectName: "statusShown"
            text: qsTr("%1 rows shown").arg(App.trace.count)
            color: Theme.subtle
        }
    }
}
