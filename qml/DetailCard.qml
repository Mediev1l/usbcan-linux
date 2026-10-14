import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Titled box used in the expanded frame view.
Rectangle {
    property alias title: titleLabel.text
    default property alias content: body.data

    color: Theme.card
    radius: Theme.radius
    border.color: Theme.divider
    implicitWidth: layout.implicitWidth + 2 * Theme.spacing
    implicitHeight: layout.implicitHeight + 2 * Theme.spacing

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: 4

        Label {
            id: titleLabel
            font.bold: true
            color: Theme.subtle
            font.capitalization: Font.AllUppercase
            font.pixelSize: 11
        }
        ColumnLayout {
            id: body
            spacing: 2
        }
    }
}
