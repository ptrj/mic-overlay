import QtQuick
import QtQuick.Window

Window {
    id: root
    required property QtObject micState
    required property var overlayConfig
    property bool overlayReady: false
    property bool screenReady: false
    property bool editing: false
    property int monitorWidth: 1
    property int monitorHeight: 1
    property int overlayX: 0
    property int overlayY: 0
    readonly property real indicatorWidth: Math.max(overlayConfig.iconOnly ? 42 : 150, content.implicitWidth + 28)
    readonly property real indicatorHeight: Math.max(42, content.implicitHeight + 20)
    signal indicatorSizeChanged()
    onIndicatorWidthChanged: indicatorSizeChanged()
    onIndicatorHeightChanged: indicatorSizeChanged()
    signal repositionRequested(real dx, real dy)
    signal repositionFinished()
    readonly property bool isMuted: micState.available && micState.muted
    readonly property color stateColor: !micState.available ? "#26303c"
        : isMuted ? overlayConfig.mutedColor : overlayConfig.unmutedColor
    visible: overlayReady && screenReady && (editing || (micState.available
        ? (micState.muted || overlayConfig.showWhenUnmuted)
        : !overlayConfig.hideWhenUnavailable))
    width: editing ? monitorWidth : indicatorWidth
    height: editing ? monitorHeight : indicatorHeight
    color: "transparent"
    title: "mic-overlay"

    Rectangle {
        id: indicator
        x: editing ? overlayX : 0
        y: editing ? overlayY : 0
        width: indicatorWidth
        height: indicatorHeight
        radius: 10
        color: Qt.rgba(stateColor.r, stateColor.g, stateColor.b, overlayConfig.opacity)
        border.color: Qt.lighter(stateColor, 1.8)
        border.width: editing ? 2 : 1

        Row {
            id: content
            anchors.centerIn: parent
            spacing: 8

            Image {
                width: Math.max(22, overlayConfig.fontSize + 4)
                height: width
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/assets/" + (isMuted
                    ? overlayConfig.mutedIcon : overlayConfig.unmutedIcon) + ".svg"
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
                fillMode: Image.PreserveAspectFit
            }

            Text {
                id: label
                visible: !overlayConfig.iconOnly
                anchors.verticalCenter: parent.verticalCenter
                text: !micState.available ? "MIC N/A" : isMuted
                    ? overlayConfig.mutedText : overlayConfig.unmutedText
                textFormat: Text.PlainText
                color: "#f2f6fa"
                font.pixelSize: overlayConfig.fontSize
                font.bold: overlayConfig.bold
            }
        }
    }
    MouseArea {
        id: pointer
        x: indicator.x
        y: indicator.y
        width: indicator.width
        height: indicator.height
        enabled: editing
        cursorShape: Qt.SizeAllCursor
        property point previousPointer
        // The editing surface stays fixed. Measure incremental movement in that
        // surface, independently of the indicator moving under the grabbed pointer.
        onPressed: mouse => { previousPointer = pointer.mapToItem(root.contentItem, mouse.x, mouse.y) }
        onPositionChanged: mouse => {
            if (pressed) {
                const current = pointer.mapToItem(root.contentItem, mouse.x, mouse.y)
                const dx = current.x - previousPointer.x
                const dy = current.y - previousPointer.y
                previousPointer = current
                repositionRequested(dx, dy)
            }
        }
        onReleased: repositionFinished()
        onCanceled: repositionFinished()
    }
}
