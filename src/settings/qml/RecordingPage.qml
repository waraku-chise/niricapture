import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    property var colors
    property var bridge
    property var source
    property bool readOnly: false
    property bool loading: false
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
            text: "录屏"
            font.pixelSize: 20
            font.weight: Font.Medium
            color: page.colors.onSurface
        }
        Label {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 4
            Layout.fillWidth: true
            text: "区域/当前输出录制使用 wf-recorder。修改后新发起的录制生效。"
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

            Label {
                text: "保存"
                color: page.colors.onSurface
                font.pixelSize: 15
                font.weight: Font.Medium
            }
            Label {
                text: "录制目录（留空使用系统 Videos/NiriCapture）"
                color: page.colors.onSurfaceVariant
                font.pixelSize: 12
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: directoryField
                    Layout.fillWidth: true
                    placeholderText: "~/视频/NiriCapture"
                    placeholderTextColor: page.colors.onSurfaceVariant
                    color: page.colors.onSurface
                    Material.accent: page.colors.primary
                    onTextEdited: page.edited()
                }
                MdButton {
                    kind: "outlined"
                    text: "选择"
                    colors: page.colors
                    onClicked: page.bridge.chooseRecordingDirectory(directoryField.text)
                }
            }
        }

        Card {
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 12
            Layout.fillWidth: true
            colors: page.colors

            Label {
                text: "编码"
                color: page.colors.onSurface
                font.pixelSize: 15
                font.weight: Font.Medium
            }
            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 14
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "帧率（1–240）"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    TextField {
                        id: fpsField
                        Layout.fillWidth: true
                        color: page.colors.onSurface
                        Material.accent: page.colors.primary
                        validator: IntValidator { bottom: 1; top: 240 }
                        onTextEdited: page.edited()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "倒计时（0–10 秒）"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    TextField {
                        id: countdownField
                        Layout.fillWidth: true
                        color: page.colors.onSurface
                        Material.accent: page.colors.primary
                        validator: IntValidator { bottom: 0; top: 10 }
                        onTextEdited: page.edited()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    spacing: 4
                    Label { text: "编码器"; color: page.colors.onSurfaceVariant; font.pixelSize: 12 }
                    ComboBox {
                        id: codecBox
                        Layout.fillWidth: true
                        editable: true
                        model: ["libx264", "libx265", "h264_nvenc", "hevc_nvenc",
                                "h264_vaapi", "hevc_vaapi"]
                        Material.accent: page.colors.primary
                        onActivated: if (!page.loading) page.edited()
                        onEditTextChanged: if (!page.loading) page.edited()
                    }
                    Label {
                        text: "可填写 ffmpeg 支持的编码器名；硬件编码需对应驱动。"
                        color: page.colors.onSurfaceVariant
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
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

            Label {
                text: "声音与提示"
                color: page.colors.onSurface
                font.pixelSize: 15
                font.weight: Font.Medium
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                MdCheck {
                    id: audioCheck
                    colors: page.colors
                    onToggled: if (!page.loading) page.edited()
                }
                Label {
                    text: "录制桌面音频"
                    color: page.colors.onSurface
                    font.pixelSize: 13
                }
                Item { Layout.fillWidth: true }
            }
            Label {
                text: "使用 PipeWire/PulseAudio 默认监听源；麦克风选择将在后续版本加入。"
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

            Label {
                text: "快捷键"
                color: page.colors.onSurface
                font.pixelSize: 15
                font.weight: Font.Medium
            }
            Label {
                text: "把这些行加入 Niri 配置的 binds："
                color: page.colors.onSurfaceVariant
                font.pixelSize: 12
            }
            Label {
                text: "Mod+Shift+R → --record-region\nMod+Shift+F → --record-fullscreen\nMod+Shift+S → --stop-recording"
                color: page.colors.onSurface
                font.pixelSize: 12
                font.family: "monospace"
                lineHeight: 1.25
            }
        }

        Item { Layout.preferredHeight: 24 }
        Item { Layout.fillHeight: true }
    }

    Connections {
        target: page.bridge
        function onRecordingDirectoryChosen(path) {
            directoryField.text = path
            page.edited()
        }
    }

    Component.onCompleted: rebuild()
    onSourceChanged: rebuild()

    function rebuild() {
        if (!page.source || !page.source.recording) return
        page.loading = true
        var recording = page.source.recording
        directoryField.text = recording.directory || ""
        fpsField.text = recording.fps || 60
        countdownField.text = recording.countdown === undefined ? 3 : recording.countdown
        codecBox.editText = recording.codec || "libx264"
        audioCheck.checked = recording.audio === true
        page.loading = false
    }

    function collect() {
        return {
            "directory": directoryField.text,
            "fps": parseInt(fpsField.text) || 60,
            "codec": String(codecBox.editText || codecBox.currentText || "libx264"),
            "audio": audioCheck.checked,
            "countdown": isNaN(parseInt(countdownField.text)) ? 3 : parseInt(countdownField.text)
        }
    }
}
