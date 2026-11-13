import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Md3.js" as Md3

Rectangle {
    id: root
    color: colors.surface

    property var configData: initialData
    property var colors: Md3.roles(configData.editor.accent, configData.editor.background)
    property bool readOnly: !settingsBridge.writable
    property bool dirty: false
    property int currentPage: 0
    readonly property int railWidth: width < 900 ? 80 : 96

    Material.theme: colors.dark ? Material.Dark : Material.Light
    Material.accent: colors.primary
    Material.primary: colors.primary
    Material.background: colors.surface
    Material.foreground: colors.onSurface

    function handleEdited() {
        if (readOnly)
            return
        dirty = true
        settingsBridge.dirty = true
    }

    function requestClose() {
        if (dirty)
            confirmClose.open()
        else
            settingsBridge.closeWindow()
    }

    function save() {
        var collected = dataPage.collect()
        var result = settingsBridge.save({
            "tools": toolsPage.collect(),
            "commands": commandsPage.collect(),
            "palette": palettePage.collect(),
            "canvas": canvasPage.collect(),
            "editor": themePage.collect(),
            "ocr": { "languages": collected.ocrText },
            "paths": { "screenshots": collected.directory }
        })
        if (result.ok) {
            dirty = false
            settingsBridge.dirty = false
            snack.show(result.warning && result.warning.length > 0
                       ? result.warning
                       : "已保存到 " + result.path)
        } else {
            errorDialog.message = result.error
            errorDialog.open()
        }
    }

    function resetDefaults() {
        configData = settingsBridge.defaults()
        dirty = false
        settingsBridge.dirty = false
        snack.show("已恢复默认值，保存后生效")
    }

    Shortcut {
        sequence: "Esc"
        onActivated: {
            if (confirmClose.opened)
                confirmClose.close()
            else if (errorDialog.opened)
                errorDialog.close()
            else
                root.requestClose()
        }
    }
    Shortcut {
        sequence: "Ctrl+S"
        onActivated: { if (root.dirty && !root.readOnly) root.save() }
    }

    function pickThemeColor(current) { themeColorPicker.openWith(current) }
    function pickCanvasColor(current) { canvasColorPicker.openWith(current) }
    function pickPaletteColor(current) { paletteColorPicker.openWith(current) }

    Connections {
        target: settingsBridge
        function onCloseRequested() { root.requestClose() }
    }

    // App bar.
    Rectangle {
        id: appBar
        width: parent.width
        height: 64
        color: root.colors.surface

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 12
            spacing: 12

            Label {
                text: "设置"
                font.pixelSize: 22
                font.weight: Font.Medium
                color: root.colors.onSurface
            }
            Item { Layout.fillWidth: true }
            Label {
                text: settingsBridge.version
                font.pixelSize: 12
                color: root.colors.onSurfaceVariant
            }
            IconButton {
                id: closeButton
                icon: "close"
                colors: root.colors
                tooltip: "关闭"
                onClicked: root.requestClose()
            }
        }
    }

    // Navigation rail and pages.
    RowLayout {
        id: body
        anchors.top: appBar.bottom
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 0

        Item {
            Layout.preferredWidth: root.railWidth
            Layout.fillHeight: true

            Column {
                anchors.top: parent.top
                anchors.topMargin: 8
                width: parent.width
                spacing: 4

                Repeater {
                    model: [
                        { "label": "工具栏", "icon": "tune" },
                        { "label": "命令", "icon": "terminal" },
                        { "label": "调色板", "icon": "palette" },
                        { "label": "画布", "icon": "crop_square" },
                        { "label": "外观", "icon": "brush" },
                        { "label": "截图 · 数据", "icon": "folder" }
                    ]
                    delegate: Item {
                        id: navItem
                        width: parent.width
                        height: root.railWidth > 80 ? 64 : 56

                        Rectangle {
                            id: indicator
                            width: 56
                            height: 32
                            radius: 16
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 4
                            color: index === root.currentPage
                                   ? root.colors.secondaryContainer
                                   : navHover.hovered ? root.colors.hover : "transparent"

                            Icon {
                                anchors.centerIn: parent
                                name: modelData.icon
                                size: 20
                                color: index === root.currentPage
                                       ? root.colors.onSecondaryContainer
                                       : root.colors.onSurfaceVariant
                            }
                        }
                        Label {
                            visible: root.railWidth > 80
                            text: modelData.label
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 40
                            font.pixelSize: 12
                            font.weight: index === root.currentPage ? Font.Medium : Font.Normal
                            color: index === root.currentPage
                                   ? root.colors.onSurface
                                   : root.colors.onSurfaceVariant
                        }
                        HoverHandler { id: navHover; cursorShape: Qt.PointingHandCursor }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.currentPage = index
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                visible: root.readOnly
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 4
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 8
                color: root.colors.errorContainer

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    text: "配置文件只读：版本过新或文件损坏，保存已禁用。"
                    color: root.colors.onErrorContainer
                    font.pixelSize: 12
                }
            }

            StackLayout {
                currentIndex: root.currentPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: !root.readOnly
                opacity: root.readOnly ? 0.55 : 1

                CheckListPage {
                    id: toolsPage
                    colors: root.colors
                    bridge: settingsBridge
                    catalog: bridge.toolCatalog
                    modelIds: root.configData.tools
                    title: "工具栏"
                    description: "勾选要显示的工具；用右侧箭头调整顺序。"
                    reorder: true
                    readOnly: root.readOnly
                    onEdited: root.handleEdited()
                }
                CheckListPage {
                    id: commandsPage
                    colors: root.colors
                    bridge: settingsBridge
                    catalog: bridge.commandCatalog
                    modelIds: root.configData.commands
                    title: "命令"
                    description: "标注工具栏上的命令；勾选后按这个顺序显示。"
                    reorder: true
                    readOnly: root.readOnly
                    onEdited: root.handleEdited()
                }
                PalettePage {
                    id: palettePage
                    colors: root.colors
                    bridge: settingsBridge
                    source: root.configData
                    readOnly: root.readOnly
                    onEdited: root.handleEdited()
                    onColorRequested: (current) => root.pickPaletteColor(current)
                }
                CanvasPage {
                    id: canvasPage
                    colors: root.colors
                    source: root.configData
                    readOnly: root.readOnly
                    onEdited: root.handleEdited()
                    onColorRequested: (current) => root.pickCanvasColor(current)
                }
                ThemePage {
                    id: themePage
                    colors: root.colors
                    source: root.configData
                    readOnly: root.readOnly
                    onEdited: root.handleEdited()
                    onColorRequested: (current) => root.pickThemeColor(current)
                }
                DataPage {
                    id: dataPage
                    colors: root.colors
                    bridge: settingsBridge
                    source: root.configData
                    readOnly: root.readOnly
                    onEdited: root.handleEdited()
                }
            }
        }
    }

    // Snackbar.
    Snackbar {
        id: snack
        colors: root.colors
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.height - footer.height - 60
        width: Math.min(implicitWidth, root.width - 48)
    }

    // Footer.
    Rectangle {
        id: footer
        anchors.bottom: parent.bottom
        width: parent.width
        height: 64
        color: root.colors.surface

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 16
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: "配置文件：" + settingsBridge.configPathShort
                color: root.colors.onSurfaceVariant
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }
            MdButton {
                kind: "text"
                text: "打开配置文件"
                colors: root.colors
                onClicked: settingsBridge.openConfigFile()
            }
            MdButton {
                kind: "text"
                text: "恢复默认"
                colors: root.colors
                onClicked: root.resetDefaults()
            }
            MdButton {
                kind: "outlined"
                text: "取消"
                colors: root.colors
                onClicked: root.requestClose()
            }
            MdButton {
                kind: "filled"
                text: "保存"
                colors: root.colors
                enabled: root.dirty && !root.readOnly
                onClicked: root.save()
            }
        }
    }

    ColorPopup {
        id: themeColorPicker
        anchorItem: root
        colors: root.colors
        onAccepted: (value) => themePage.applyColor(value)
    }
    ColorPopup {
        id: canvasColorPicker
        anchorItem: root
        colors: root.colors
        onAccepted: (value) => canvasPage.applyColor(value)
    }
    ColorPopup {
        id: paletteColorPicker
        anchorItem: root
        colors: root.colors
        onAccepted: (value) => palettePage.applyColor(value)
    }

    // Discard-changes dialog.
    Popup {
        id: confirmClose
        width: 380
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        padding: 24

        background: Rectangle {
            radius: 24
            color: root.colors.surfaceContainerHigh
        }

        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: "放弃未保存的修改？"
                font.pixelSize: 20
                font.weight: Font.Medium
                color: root.colors.onSurface
                Layout.fillWidth: true
            }
            Label {
                text: "关闭后，这次修改不会写入配置文件。"
                color: root.colors.onSurfaceVariant
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                MdButton {
                    kind: "text"
                    text: "继续编辑"
                    colors: root.colors
                    onClicked: confirmClose.close()
                }
                MdButton {
                    kind: "filled"
                    text: "放弃并关闭"
                    colors: root.colors
                    onClicked: {
                        confirmClose.close()
                        settingsBridge.closeWindow()
                    }
                }
            }
        }
    }

    // Save-error dialog.
    Popup {
        id: errorDialog
        property string message: ""
        width: 380
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        padding: 24

        background: Rectangle {
            radius: 24
            color: root.colors.surfaceContainerHigh
        }

        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: "保存失败"
                font.pixelSize: 20
                font.weight: Font.Medium
                color: root.colors.onSurface
                Layout.fillWidth: true
            }
            Label {
                text: errorDialog.message
                color: root.colors.onSurfaceVariant
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                MdButton {
                    kind: "filled"
                    text: "知道了"
                    colors: root.colors
                    onClicked: errorDialog.close()
                }
            }
        }
    }
}
