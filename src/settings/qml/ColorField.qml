import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AbstractButton {
    id: control
    property var colors
    property color value: "#000000"
    signal activated()

    implicitHeight: 44
    implicitWidth: 220
    leftPadding: 12
    rightPadding: 12

    background: Rectangle {
        radius: 10
        color: control.hovered ? control.colors.hover
                               : control.colors.surfaceContainerHigh
        border.width: 1
        border.color: control.colors.outlineVariant
    }

    contentItem: RowLayout {
        spacing: 10
        Rectangle {
            width: 22
            height: 22
            radius: 6
            color: control.value
            border.width: 1
            border.color: control.colors.outline
        }
        Label {
            Layout.fillWidth: true
            text: String(control.value).toUpperCase()
            color: control.colors.onSurface
            font.pixelSize: 13
        }
        Label {
            text: "编辑"
            color: control.colors.primary
            font.pixelSize: 12
            font.weight: Font.Medium
        }
    }

    onClicked: control.activated()
    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
