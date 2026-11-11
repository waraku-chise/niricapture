import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    property var colors
    property var bridge
    property var source
    property bool readOnly: false
    signal edited()
    signal colorRequested(color current)

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
            text: "调色板"
            font.pixelSize: 20
            font.weight: Font.Medium
            color: page.colors.onSurface
        }
        Label {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 4
            Layout.fillWidth: true
            text: "颜色面板里的色块，以及滚轮切换的线宽档位。"
            color: page.colors.onSurfaceVariant
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Card {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 16
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(360, 100 + colorModel.count * 44)
            colors: page.colors

            ListView {
                id: colorList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                model: ListModel { id: colorModel }
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 44
                    radius: 8
                    color: colorHover.hovered ? page.colors.hover : "transparent"

                    HoverHandler { id: colorHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 4
                        spacing: 12

                        Rectangle {
                            width: 24
                            height: 24
                            radius: 6
                            color: model.value
                            border.width: 1
                            border.color: page.colors.outline
                        }
                        Label {
                            Layout.fillWidth: true
                            text: String(model.value).toUpperCase()
                            color: page.colors.onSurface
                            font.pixelSize: 13
                        }
                        IconButton {
                            icon: "close"
                            colors: page.colors
                            tooltip: "删除"
                            onClicked: {
                                colorModel.remove(index)
                                page.edited()
                            }
                        }
                    }
                }
            }

            RowLayout {
                spacing: 8
                MdButton {
                    kind: "tonal"
                    text: "添加颜色"
                    colors: page.colors
                    onClicked: page.colorRequested("#FFFFFF")
                }
                Item { Layout.fillWidth: true }
            }
        }

        Card {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 12
            Layout.fillWidth: true
            colors: page.colors

            TextField {
                id: widthsField
                Layout.fillWidth: true
                placeholderText: "线宽（逗号分隔）"
                color: page.colors.onSurface
                placeholderTextColor: page.colors.onSurfaceVariant
                Material.accent: page.colors.primary
                onTextEdited: page.edited()
            }
            Label {
                text: "面板里显示为圆点，滚轮在这些档位之间切换。"
                color: page.colors.onSurfaceVariant
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Item { Layout.preferredHeight: 24 }
        Item { Layout.fillHeight: true }
    }

    Component.onCompleted: rebuild()
    onSourceChanged: rebuild()

    function rebuild() {
        if (!page.source || !page.source.palette) return
                colorModel.clear()
        var entries = page.source.palette.colors || []
        for (var i = 0; i < entries.length; ++i)
            colorModel.append({ "value": String(entries[i]).toUpperCase() })
        widthsField.text = (page.source.palette.widths || []).join(", ")
    }

    function applyColor(color) {
        colorModel.append({ "value": String(color).toUpperCase() })
        page.edited()
    }

    function collect() {
        var colors = []
        for (var i = 0; i < colorModel.count; ++i)
            colors.push(colorModel.get(i).value)
        return { "colors": colors, "widths": widthsField.text }
    }
}
