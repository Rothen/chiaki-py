import QtQuick
import QtQuick.Window
import QtWebEngine

Window {
    id: root
    visible: true
    width: 800
    height: 600

    // Set by PSNLoginQt (chiaki_py.psn.auth.login_url() / REDIRECT_URL), so there is one source of truth.
    required property string loginUrl
    required property string redirectUrl

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
