import QtQuick
import QtQuick.Controls

AbstractButton {
    id: control
    property var colors
    property string kind: "filled"
    property int contentPadding: 20
    implicitHeight: 40
    implicitWidth: Math.max(64, label.implicitWidth + contentPadding * 2)

    background: Rectangle {
        radius: control.height / 2
        color: {
            if (!control.enabled)
                return control.colors.surfaceContainerHigh
            if (control.kind === "filled")
                return control.pressed ? Qt.darker(control.colors.primary, 1.12)
                                       : control.hovered ? Qt.lighter(control.colors.primary, 1.08)
                                                         : control.colors.primary
            if (control.kind === "tonal")
                return control.hovered ? control.colors.surfaceContainerHighest
                                       : control.colors.secondaryContainer
            return control.hovered ? control.colors.hover : "transparent"
        }
        border.width: control.kind === "outlined" ? 1 : 0
        border.color: control.colors.outline
    }

    contentItem: Label {
        id: label
        text: control.text
        font.pixelSize: 14
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.contentPadding
        rightPadding: control.contentPadding
        color: !control.enabled
               ? control.colors.onSurfaceVariant
               : control.kind === "filled" ? control.colors.onPrimary
                                           : control.colors.primary
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
