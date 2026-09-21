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

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
    }
}
