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

    // Emitted when the window is closed; stop the threads and the session in response
    signal closeRequested()
    onClosing: root.closeRequested()

    // The frame rate shown in the top-right corner, set from Python (root.fpsText = "59.9 FPS");
    // nothing is drawn while it is empty, i.e. until the first measurement
    property string fpsText: ""

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
    }

    Rectangle {
        visible: root.fpsText !== ""
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        width: fpsLabel.width + 12
        height: fpsLabel.height + 12
        color: "#96000000" // black at 150/255 opacity, so the text reads on any picture

        Text {
            id: fpsLabel
            anchors.centerIn: parent
            text: root.fpsText
            color: "white"
            font.pixelSize: 24
            font.bold: true
        }
    }
}
