pragma Singleton
import QtQuick

// Dracula colour scheme (https://draculatheme.com) plus shared sizes and fonts.
QtObject {
    // Palette
    readonly property color background: "#282a36"
    readonly property color surface: "#21222c"      // toolbars, side panels
    readonly property color currentLine: "#44475a"
    readonly property color foreground: "#f8f8f2"
    readonly property color comment: "#6272a4"
    readonly property color cyan: "#8be9fd"
    readonly property color green: "#50fa7b"
    readonly property color orange: "#ffb86c"
    readonly property color pink: "#ff79c6"
    readonly property color purple: "#bd93f9"
    readonly property color red: "#ff5555"
    readonly property color yellow: "#f1fa8c"

    // Roles
    readonly property color rx: cyan
    readonly property color tx: orange
    readonly property color ok: green
    readonly property color error: red
    readonly property color accent: purple
    readonly property color subtle: "#a3abd1"       // secondary text, readable on background
    readonly property color card: "#343746"
    readonly property color divider: "#3c3f51"
    readonly property color selection: currentLine
    readonly property color changed: yellow         // background of changed bytes
    readonly property color onBright: background    // text on green/red/yellow

    readonly property string mono: "monospace"
    readonly property int spacing: 8
    readonly property int radius: 6
    readonly property int rowHeight: 30
}
