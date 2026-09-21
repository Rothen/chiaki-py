import QtQuick
import QtQuick.Window
import QtWebEngine

Window {
    id: root
    visible: true
    width: 800
    height: 600

    readonly property string clientId: "ba495a24-818c-472b-b12d-ff231c1b5745"
    readonly property string redirectUrl: "https://remoteplay.dl.playstation.net/remoteplay/redirect"
    readonly property string loginUrl: "https://auth.api.sonyentertainmentnetwork.com/" +
        "2.0/oauth/authorize?service_entity=urn:service-entity:psn" +
        "&response_type=code&client_id=" + clientId +
        "&redirect_uri=" + redirectUrl +
        "&scope=psn:clientapp referenceDataService:countryConfig.read pushNotification:webSocket.desktop.connect sessionManager:remotePlaySession.system.update" +
        "&request_locale=en_US" +
        "&ui=pr" +
        "&service_logo=ps" +
        "&layout_type=popup" +
        "&smcid=remoteplay" +
        "&prompt=always" +
        "&PlatformPrivacyWs1=minimal&"

    // Emitted with the captured redirect URL; hand it to PSNLoginParser.parse_psn_account
    signal redirectCaptured(string url)

    WebEngineView {
        id: webView
        anchors.fill: parent
        url: root.loginUrl

        onUrlChanged: {
            const currentUrl = webView.url.toString()
            if (currentUrl.startsWith(root.redirectUrl)) {
                root.redirectCaptured(currentUrl)
                root.close()
            }
        }
    }
}
