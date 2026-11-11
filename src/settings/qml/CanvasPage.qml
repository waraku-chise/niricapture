import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    property var colors
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
            text: "画布"
            font.pixelSize: 20
            font.weight: Font.Medium
            color: page.colors.onSurface
        }
        Label {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 4
            Layout.fillWidth: true
            text: "新建画布和打开图片时使用的默认值。"
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

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    TextField {
                        id: widthField
                        Layout.fillWidth: true
                        placeholderText: "默认宽度"
                        color: page.colors.onSurface
                        Material.accent: page.colors.primary
                        validator: IntValidator { bottom: 1; top: 20000 }
                        onTextEdited: page.edited()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    TextField {
                        id: heightField
                        Layout.fillWidth: true
                        placeholderText: "默认高度"
                        color: page.colors.onSurface
                        Material.accent: page.colors.primary
                        validator: IntValidator { bottom: 1; top: 20000 }
                        onTextEdited: page.edited()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    TextField {
                        id: paddingField
                        Layout.fillWidth: true
                        placeholderText: "内边距"
                        color: page.colors.onSurface
                        Material.accent: page.colors.primary
                        validator: IntValidator { bottom: 0; top: 512 }
                        onTextEdited: page.edited()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "背景"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ComboBox {
                        id: backgroundBox
                        Layout.fillWidth: true
                        model: ["透明", "白色", "深色", "Aurora", "Sunset", "Lagoon", "Violet"]
                        Material.accent: page.colors.primary
                        onActivated: page.edited()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "背景色"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ColorField {
                        id: backgroundField
                        Layout.fillWidth: true
                        colors: page.colors
                        onActivated: page.colorRequested(backgroundField.value)
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }

        Item { Layout.preferredHeight: 24 }
        Item { Layout.fillHeight: true }
    }

    readonly property var backgroundKeys: ["transparent", "white", "dark", "aurora", "sunset", "lagoon", "violet"]

    Component.onCompleted: rebuild()
    onSourceChanged: rebuild()

    function applyColor(color) {
        backgroundField.value = color
        page.edited()
    }

    function rebuild() {
        if (!page.source || !page.source.canvas) return
                widthField.text = page.source.canvas.width
        heightField.text = page.source.canvas.height
        paddingField.text = page.source.canvas.padding
        backgroundBox.currentIndex = Math.max(0, backgroundKeys.indexOf(page.source.canvas.background))
        backgroundField.value = page.source.canvas.background_color
    }

    function collect() {
        return {
            "width": parseInt(widthField.text) || 1600,
            "height": parseInt(heightField.text) || 900,
            "padding": parseInt(paddingField.text) || 0,
            "background": backgroundKeys[backgroundBox.currentIndex],
            "background_color": String(backgroundField.value).toUpperCase(),
            "fit_on_open": true
        }
    }
}
