import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Title row used at the top of each side panel.
RowLayout {
    property alias title: titleLabel.text
    property alias subtitle: subtitleLabel.text
    default property alias actions: actionRow.data

    spacing: Theme.spacing

    Label {
        id: titleLabel
        font.pixelSize: 16
        font.bold: true
    }
    Label {
        id: subtitleLabel
        color: Theme.subtle
        Layout.fillWidth: true
        elide: Text.ElideRight
    }
    RowLayout {
        id: actionRow
        spacing: 2
    }
}
