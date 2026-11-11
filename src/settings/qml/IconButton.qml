import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property var colors
    property string icon: "close"
    property string tooltip: ""
    signal clicked()

    implicitWidth: 40
    implicitHeight: 40
    width: implicitWidth
    height: implicitHeight
    radius: 20
    color: hover.hovered && enabled ? colors.hover : "transparent"
    opacity: enabled ? 1 : 0.38

    Icon {
        anchors.centerIn: parent
        name: root.icon
        size: 20
        color: root.colors.onSurfaceVariant
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: tooltip !== "" && hover.hovered
    ToolTip.text: tooltip
}
