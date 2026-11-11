import QtQuick
import QtQuick.Controls

Rectangle {
    id: snack
    property var colors
    property string message: ""
    property bool shown: false

    implicitHeight: 44
    implicitWidth: label.implicitWidth + 48
    radius: 8
    color: colors.surfaceContainerHighest
    opacity: shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    Label {
        id: label
        anchors.centerIn: parent
        text: snack.message
        color: snack.colors.onSurface
        font.pixelSize: 13
    }

    Timer {
        id: timer
        interval: 2400
        onTriggered: snack.shown = false
    }

    function show(text) {
        message = text
        shown = true
        timer.restart()
    }
}
