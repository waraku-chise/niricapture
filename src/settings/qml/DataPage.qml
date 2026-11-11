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
            text: "截图 · 数据"
            font.pixelSize: 20
            font.weight: Font.Medium
            color: page.colors.onSurface
        }
        Label {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 4
            Layout.fillWidth: true
            text: "OCR、保存目录与登录启动；这些值不会改动 Niri 或 DMS 配置。"
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

            TextField {
                id: ocrField
                Layout.fillWidth: true
                placeholderText: "OCR 语言（如 eng+chi_sim）"
                placeholderTextColor: page.colors.onSurfaceVariant
                color: page.colors.onSurface
                Material.accent: page.colors.primary
                onTextEdited: page.edited()
            }
            Label {
                text: "留空则由 Tesseract 自动检测；NIRICAPTURE_OCR_LANGS 优先于本项。"
                color: page.colors.onSurfaceVariant
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Card {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 12
            Layout.fillWidth: true
            colors: page.colors

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: directoryField
                    Layout.fillWidth: true
                    placeholderText: "截图目录"
                    placeholderTextColor: page.colors.onSurfaceVariant
                    color: page.colors.onSurface
                    Material.accent: page.colors.primary
                    onTextEdited: page.edited()
                }
                MdButton {
                    kind: "outlined"
                    text: "选择"
                    colors: page.colors
                    onClicked: page.bridge.chooseDirectory(directoryField.text)
                }
            }
        }

        Card {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 12
            Layout.fillWidth: true
            colors: page.colors

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: "登录时启动托盘"; color: page.colors.onSurface; font.pixelSize: 14 }
                    Label {
                        text: "写入 XDG autostart；若 Niri 不读取，请改用 spawn-at-startup。"
                        color: page.colors.onSurfaceVariant
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                Switch {
                    id: autostartSwitch
                    checked: page.source.autostart
                    Material.accent: page.colors.primary
                    onToggled: page.edited()
                }
            }
        }

        Card {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 12
            Layout.fillWidth: true
            colors: page.colors

            Label { text: "配置文件"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
            Label {
                text: page.bridge.configPath
                color: page.colors.onSurface
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
                textFormat: Text.PlainText
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                MdButton {
                    kind: "text"
                    text: "打开配置文件"
                    colors: page.colors
                    onClicked: page.bridge.openConfigFile()
                }
                Item { Layout.fillWidth: true }
            }
        }

        Item { Layout.preferredHeight: 24 }
        Item { Layout.fillHeight: true }
    }

    Connections {
        target: page.bridge
        function onDirectoryChosen(path) {
            directoryField.text = path
            page.edited()
        }
    }

    Component.onCompleted: rebuild()
    onSourceChanged: rebuild()

    function rebuild() {
        if (!page.source) return
                ocrField.text = page.source.ocr ? page.source.ocr.languages : ""
        directoryField.text = page.source.paths ? page.source.paths.screenshots : ""
        autostartSwitch.checked = page.source.autostart === true
    }

    function collect() {
        return {
            "ocrText": ocrField.text,
            "directory": directoryField.text,
            "autostart": autostartSwitch.checked
        }
    }
}
