import QtQuick
import QtQuick.Window
import QtMultimedia

Window {
    id: root
    visible: true
    x: 100
    y: 100
    width: 640
    height: 480
    title: "Live Image Stream"

    readonly property var videoSink: videoOutput.videoSink

    readonly property rect videoRect: videoOutput.contentRect

    signal closeRequested()
    onClosing: root.closeRequested()

    property string statsText: ""
    property bool showStats: false

    Shortcut {
        sequence: "F"
        onActivated: root.showStats = !root.showStats
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    Rectangle {
        visible: root.showStats && root.statsText !== ""
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        width: statsLabel.width + 16
        height: statsLabel.height + 12
        color: "#96000000" // black at 150/255 opacity, so the text reads on any picture

        Text {
            id: statsLabel
            anchors.centerIn: parent
            text: root.statsText
            color: "white"
            horizontalAlignment: Text.AlignRight
            font.pixelSize: 16
            font.bold: true
        }
    }
}
