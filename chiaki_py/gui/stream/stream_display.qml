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

    // Push decoded frames into this sink from Python: root.videoSink.setVideoFrame(frame)
    readonly property var videoSink: videoOutput.videoSink

    // Where the picture is within the window: it is scaled to fit, keeping its aspect ratio, so
    // it follows the window's size, with bars at the sides or at the top and bottom if the
    // aspect ratios differ
    readonly property rect videoRect: videoOutput.contentRect

    // Emitted when the window is closed; stop the threads and the session in response
    signal closeRequested()
    onClosing: root.closeRequested()

    // The frame rate and time per frame in the top-right corner, set from Python
    // (root.statsText = "59.9 FPS\n16.7 ms/frame"); F shows or hides them. Nothing is drawn while the
    // text is empty, i.e. until the first measurement
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
