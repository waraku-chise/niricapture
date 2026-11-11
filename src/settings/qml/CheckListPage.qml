import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    property var colors
    property var bridge
    property var catalog: []
    property var modelIds: []
    property string title: ""
    property string description: ""
    property bool reorder: true
    property bool readOnly: false
    signal edited()

    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    ColumnLayout {
        width: page.availableWidth
        spacing: 0

        Item { Layout.preferredHeight: 4 }
        Label {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 20
            text: page.title
            font.pixelSize: 20
            font.weight: Font.Medium
            color: page.colors.onSurface
        }
        Label {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 4
            Layout.fillWidth: true
            text: page.description
            color: page.colors.onSurfaceVariant
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        Card {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 16
            Layout.fillWidth: true
            colors: page.colors

            ListView {
                id: list
                Layout.fillWidth: true
                implicitHeight: contentHeight
                interactive: false
                model: ListModel { id: rows }
                delegate: Rectangle {
                    id: row
                    width: ListView.view.width
                    height: 56
                    radius: 8
                    color: rowHover.hovered ? page.colors.hover : "transparent"

                    HoverHandler { id: rowHover }

                    MdCheck {
                        id: check
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        colors: page.colors
                        checked: model.checked
                        onToggled: {
                            rows.setProperty(index, "checked", checked)
                            page.edited()
                        }
                    }

                    Column {
                        anchors.left: check.right
                        anchors.leftMargin: 14
                        anchors.right: actions.left
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Label {
                            width: parent.width
                            text: model.label
                            color: page.colors.onSurface
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                        Label {
                            visible: text !== ""
                            width: parent.width
                            text: model.subtitle
                            color: page.colors.onSurfaceVariant
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    Row {
                        id: actions
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        IconButton {
                            visible: page.reorder
                            icon: "arrow_up"
                            colors: page.colors
                            enabled: index > 0
                            tooltip: "上移"
                            onClicked: page.moveRow(index, -1)
                        }
                        IconButton {
                            visible: page.reorder
                            icon: "arrow_down"
                            colors: page.colors
                            enabled: index < rows.count - 1
                            tooltip: "下移"
                            onClicked: page.moveRow(index, 1)
                        }
                    }
                }
            }
        }
        Item { Layout.preferredHeight: 24 }
        Item { Layout.fillHeight: true }
    }

    Component.onCompleted: rebuild()
    onCatalogChanged: rebuild()
    onModelIdsChanged: rebuild()

    function rebuild() {
        rows.clear()
        var entries = page.catalog || []
        var byId = ({})
        for (var i = 0; i < entries.length; ++i)
            byId[entries[i].id] = entries[i]
        var seen = ({})
        var ids = page.modelIds || []
        for (var j = 0; j < ids.length; ++j) {
            var id = ids[j]
            if (byId[id] !== undefined && !seen[id]) {
                appendRow(byId[id], true)
                seen[id] = true
            }
        }
        for (var k = 0; k < entries.length; ++k) {
            if (!seen[entries[k].id])
                appendRow(entries[k], false)
        }
    }

    function appendRow(info, checked) {
        var subtitle = info.shortcut !== undefined && info.shortcut !== ""
                       ? info.shortcut : info.id
        rows.append({
            "tid": info.id,
            "label": info.label,
            "subtitle": subtitle,
            "checked": checked
        })
    }

    function moveRow(index, delta) {
        var target = index + delta
        if (target < 0 || target >= rows.count)
            return
        rows.move(index, target, 1)
        page.edited()
    }

    function collect() {
        var out = []
        for (var i = 0; i < rows.count; ++i) {
            if (rows.get(i).checked)
                out.push(rows.get(i).tid)
        }
        return out
    }
}
