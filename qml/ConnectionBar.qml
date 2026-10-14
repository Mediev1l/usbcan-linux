import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import UsbCan.Backend

// Adapter selection, bus settings and the connect button.
ToolBar {
    id: bar
    objectName: "connectionBar"
    Material.background: Theme.surface
    Material.foreground: Theme.foreground

    // Bus settings can only change while disconnected.
    readonly property bool settingsEditable: !App.connected

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        Rectangle {
            objectName: "connectionDot"
            implicitWidth: 12
            implicitHeight: 12
            radius: 6
            color: App.connected ? Theme.ok : (App.errorText !== "" ? Theme.error : Theme.subtle)
            ToolTip.visible: dotArea.containsMouse
            ToolTip.text: App.connectionLabel
            MouseArea {
                id: dotArea
                anchors.fill: parent
                hoverEnabled: true
            }
        }

        ComboBox {
            objectName: "backendCombo"
            enabled: bar.settingsEditable
            model: [qsTr("Waveshare USB-CAN-A"), qsTr("SocketCAN")]
            currentIndex: App.backend
            onActivated: index => App.backend = index
            Layout.preferredWidth: 240
            Layout.fillWidth: true
            Layout.maximumWidth: 240
            Layout.minimumWidth: 90
        }

        // --- Waveshare -----------------------------------------------------
        ComboBox {
            id: portCombo
            objectName: "portCombo"
            visible: App.backend === 0
            enabled: bar.settingsEditable
            editable: true
            model: App.ports
            textRole: "label"
            valueRole: "path"
            Layout.preferredWidth: 300
            Layout.fillWidth: true
            Layout.maximumWidth: 300
            Layout.minimumWidth: 90
            displayText: {
                for (let i = 0; i < App.ports.length; ++i) {
                    if (App.ports[i].path === App.port)
                        return App.ports[i].label
                }
                return App.port
            }
            onActivated: index => App.port = App.ports[index].path
            onAccepted: App.port = editText
            ToolTip.visible: hovered
            ToolTip.text: App.port
        }
        ComboBox {
            objectName: "bitrateCombo"
            visible: App.backend === 0
            enabled: bar.settingsEditable
            model: App.bitrates
            displayText: (App.bitrate / 1000) + " kbit/s"
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                text: (modelData / 1000) + " kbit/s"
                highlighted: modelData === App.bitrate
            }
            onActivated: index => App.bitrate = App.bitrates[index]
            Layout.preferredWidth: 140
            Layout.fillWidth: true
            Layout.maximumWidth: 140
            Layout.minimumWidth: 90
        }
        ComboBox {
            objectName: "modeCombo"
            visible: App.backend === 0
            enabled: bar.settingsEditable
            model: [qsTr("Normal"), qsTr("Loopback"), qsTr("Silent (listen only)"), qsTr("Loopback + silent")]
            currentIndex: App.mode
            onActivated: index => App.mode = index
            Layout.preferredWidth: 190
            Layout.fillWidth: true
            Layout.maximumWidth: 190
            Layout.minimumWidth: 90
            ToolTip.visible: hovered && App.mode === 2
            ToolTip.text: qsTr("Silent mode sends no ACKs: frames are only received if another node acknowledges them.")
        }
        ComboBox {
            objectName: "frameTypeCombo"
            visible: App.backend === 0
            enabled: bar.settingsEditable
            model: [qsTr("Standard (11 bit)"), qsTr("Extended (29 bit)")]
            currentIndex: App.extendedFrames ? 1 : 0
            onActivated: index => App.extendedFrames = index === 1
            Layout.preferredWidth: 195
            Layout.fillWidth: true
            Layout.maximumWidth: 195
            Layout.minimumWidth: 90
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Adapter frame type setting (as in the vendor tool)")
        }

        // --- SocketCAN -----------------------------------------------------
        ComboBox {
            objectName: "interfaceCombo"
            visible: App.backend === 1
            enabled: bar.settingsEditable
            model: App.interfaces
            currentIndex: App.interfaces.indexOf(App.interfaceName)
            onActivated: index => App.interfaceName = App.interfaces[index]
            Layout.preferredWidth: 160
            Layout.fillWidth: true
            Layout.maximumWidth: 160
            Layout.minimumWidth: 90
        }

        ToolButton {
            objectName: "refreshButton"
            enabled: bar.settingsEditable
            text: "⟳"
            font.pixelSize: 18
            onClicked: App.refreshDevices()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Rescan ports and interfaces")
        }

        Item {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
        }

        Button {
            objectName: "connectButton"
            text: App.connected ? qsTr("Disconnect") : qsTr("Connect")
            // Explicit colours: the toolbar's Material.background would otherwise be inherited.
            Material.background: App.connected ? Theme.error : Theme.ok
            Material.foreground: Theme.onBright
            font.bold: true
            onClicked: App.toggleConnection()
            Layout.preferredWidth: 140
            Layout.minimumWidth: 140
        }
    }
}
