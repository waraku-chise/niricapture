import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: popup
    property var colors
    property Item anchorItem
    property int red: 0
    property int green: 0
    property int blue: 0
    readonly property color value: Qt.rgba(red / 255, green / 255, blue / 255, 1)
    property bool syncing: false
    signal accepted(color result)

    width: 380
    padding: 24
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    x: anchorItem ? (anchorItem.width - width) / 2 : 0
    y: anchorItem ? (anchorItem.height - height) / 2 : 0

    background: Rectangle {
        radius: 24
        color: popup.colors.surfaceContainerHigh
    }

    contentItem: ColumnLayout {
        spacing: 14

        Label {
            text: "选择颜色"
            font.pixelSize: 20
            font.weight: Font.Medium
            color: popup.colors.onSurface
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                width: 56
                height: 56
                radius: 12
                color: popup.value
                border.width: 1
                border.color: popup.colors.outline
            }

            TextField {
                id: hexField
                Layout.fillWidth: true
                placeholderText: "#RRGGBB"
                color: popup.colors.onSurface
                Material.accent: popup.colors.primary
                onTextEdited: popup.fromHex(text)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: "R"; color: popup.colors.onSurfaceVariant; font.pixelSize: 12; Layout.preferredWidth: 14 }
            Slider {
                id: redSlider
                Layout.fillWidth: true
                from: 0
                to: 255
                stepSize: 1
                Material.accent: popup.colors.primary
                onValueChanged: popup.syncFromSliders()
            }
            Label { text: popup.red; color: popup.colors.onSurface; font.pixelSize: 12; Layout.preferredWidth: 30; horizontalAlignment: Text.AlignRight }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: "G"; color: popup.colors.onSurfaceVariant; font.pixelSize: 12; Layout.preferredWidth: 14 }
            Slider {
                id: greenSlider
                Layout.fillWidth: true
                from: 0
                to: 255
                stepSize: 1
                Material.accent: popup.colors.primary
                onValueChanged: popup.syncFromSliders()
            }
            Label { text: popup.green; color: popup.colors.onSurface; font.pixelSize: 12; Layout.preferredWidth: 30; horizontalAlignment: Text.AlignRight }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { text: "B"; color: popup.colors.onSurfaceVariant; font.pixelSize: 12; Layout.preferredWidth: 14 }
            Slider {
                id: blueSlider
                Layout.fillWidth: true
                from: 0
                to: 255
                stepSize: 1
                Material.accent: popup.colors.primary
                onValueChanged: popup.syncFromSliders()
            }
            Label { text: popup.blue; color: popup.colors.onSurface; font.pixelSize: 12; Layout.preferredWidth: 30; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            MdButton {
                kind: "text"
                text: "取消"
                colors: popup.colors
                onClicked: popup.close()
            }
            MdButton {
                kind: "filled"
                text: "应用"
                colors: popup.colors
                onClicked: {
                    popup.accepted(popup.value)
                    popup.close()
                }
            }
        }
    }

    function openWith(currentColor) {
        syncing = true
        red = Math.round(currentColor.r * 255)
        green = Math.round(currentColor.g * 255)
        blue = Math.round(currentColor.b * 255)
        redSlider.value = red
        greenSlider.value = green
        blueSlider.value = blue
        hexField.text = toHex()
        syncing = false
        open()
    }

    function syncFromSliders() {
        if (syncing)
            return
        red = Math.round(redSlider.value)
        green = Math.round(greenSlider.value)
        blue = Math.round(blueSlider.value)
        hexField.text = toHex()
    }

    function fromHex(text) {
        if (!/^#[0-9a-fA-F]{6}$/.test(text))
            return
        syncing = true
        red = parseInt(text.substring(1, 3), 16)
        green = parseInt(text.substring(3, 5), 16)
        blue = parseInt(text.substring(5, 7), 16)
        redSlider.value = red
        greenSlider.value = green
        blueSlider.value = blue
        syncing = false
    }

    function toHex() {
        var r = red.toString(16)
        var g = green.toString(16)
        var b = blue.toString(16)
        if (r.length < 2) r = "0" + r
        if (g.length < 2) g = "0" + g
        if (b.length < 2) b = "0" + b
        return ("#" + r + g + b).toUpperCase()
    }
}
