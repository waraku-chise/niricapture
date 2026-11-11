import QtQuick
import QtQuick.Controls

AbstractButton {
    id: control
    property var colors

    implicitWidth: 24
    implicitHeight: 24
    checkable: true

    opacity: enabled ? 1 : 0.45
    background: Item {}

    contentItem: Item {
        Rectangle {
            anchors.centerIn: parent
            width: 18
            height: 18
            radius: 4
            color: control.checked ? control.colors.primary : "transparent"
            border.width: control.checked ? 0 : 1.5
            border.color: control.colors.onSurfaceVariant
        }
        Icon {
            anchors.centerIn: parent
            visible: control.checked
            name: "check"
            size: 14
            color: control.colors.onPrimary
        }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
