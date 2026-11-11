import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    property var colors
    property var source
    property bool readOnly: false
    property var activeField: null
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
            text: "外观"
            font.pixelSize: 20
            font.weight: Font.Medium
            color: page.colors.onSurface
        }
        Label {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 4
            Layout.fillWidth: true
            text: "编辑器主题色与默认标注样式，保存后对新打开的编辑器生效。"
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

            Label { text: "颜色"; color: page.colors.onSurface; font.pixelSize: 15; font.weight: Font.Medium }

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "强调色"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ColorField {
                        id: accentField
                        Layout.fillWidth: true
                        colors: page.colors
                        onActivated: page.pick(this)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "编辑器背景"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ColorField {
                        id: backgroundField
                        Layout.fillWidth: true
                        colors: page.colors
                        onActivated: page.pick(this)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "面板"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ColorField {
                        id: panelField
                        Layout.fillWidth: true
                        colors: page.colors
                        onActivated: page.pick(this)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "棋盘格亮色"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ColorField {
                        id: checkerLightField
                        Layout.fillWidth: true
                        colors: page.colors
                        onActivated: page.pick(this)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "棋盘格暗色"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ColorField {
                        id: checkerDarkField
                        Layout.fillWidth: true
                        colors: page.colors
                        onActivated: page.pick(this)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "默认标注颜色"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ColorField {
                        id: defaultColorField
                        Layout.fillWidth: true
                        colors: page.colors
                        onActivated: page.pick(this)
                    }
                }
            }
        }

        Card {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 12
            Layout.fillWidth: true
            colors: page.colors

            Label { text: "标注"; color: page.colors.onSurface; font.pixelSize: 15; font.weight: Font.Medium }

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "默认线宽"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    RowLayout {
                        Layout.fillWidth: true
                        Slider {
                            id: widthSlider
                            Layout.fillWidth: true
                            from: 1
                            to: 96
                            stepSize: 1
                            Material.accent: page.colors.primary
                            onMoved: page.edited()
                        }
                        Label {
                            Layout.preferredWidth: 32
                            text: Math.round(widthSlider.value)
                            color: page.colors.onSurface
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "文字字体"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ComboBox {
                        id: fontBox
                        Layout.fillWidth: true
                        model: buildFontList()
                        Material.accent: page.colors.primary
                        onActivated: page.edited()
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 24 }
        Item { Layout.fillHeight: true }
    }

    Component.onCompleted: rebuild()
    onSourceChanged: rebuild()

    function pick(field) {
        activeField = field
        page.colorRequested(field.value)
    }

    function applyColor(color) {
        if (activeField) {
            activeField.value = color
            page.edited()
        }
    }

    function buildFontList() {
        var families = Qt.fontFamilies()
        var current = page.source && page.source.editor ? page.source.editor.font : ""
        if (current !== "" && families.indexOf(current) < 0)
            families.push(current)
        return families
    }

    function rebuild() {
        if (!page.source || !page.source.editor) return
                var editor = page.source.editor
        accentField.value = editor.accent
        backgroundField.value = editor.background
        panelField.value = editor.panel
        checkerLightField.value = editor.checker_light
        checkerDarkField.value = editor.checker_dark
        defaultColorField.value = editor.default_color
        widthSlider.value = editor.default_width
        var families = buildFontList()
        fontBox.model = families
        fontBox.currentIndex = Math.max(0, families.indexOf(editor.font))
    }

    function collect() {
        return {
            "accent": String(accentField.value).toUpperCase(),
            "background": String(backgroundField.value).toUpperCase(),
            "panel": String(panelField.value).toUpperCase(),
            "checker_light": String(checkerLightField.value).toUpperCase(),
            "checker_dark": String(checkerDarkField.value).toUpperCase(),
            "default_color": String(defaultColorField.value).toUpperCase(),
            "default_width": Math.round(widthSlider.value),
            "font": fontBox.currentText
        }
    }
}
