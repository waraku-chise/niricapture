import QtQuick
import QtQuick.Shapes
import "Icons.js" as Icons

Item {
    id: root
    property string name: ""
    property color color: "#FFFFFF"
    property int size: 24
    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        antialiasing: true
        ShapePath {
            strokeColor: "transparent"
            fillColor: root.color
            PathSvg { path: Icons.paths[root.name] || "" }
        }
    }
}
