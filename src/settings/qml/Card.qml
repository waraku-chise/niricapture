import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var colors
    property int padding: 16
    default property alias contentData: column.data

    implicitWidth: column.implicitWidth + padding * 2
    implicitHeight: column.implicitHeight + padding * 2
    radius: 12
    color: colors.surfaceContainerHigh

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: root.padding
        spacing: 12
    }
}
