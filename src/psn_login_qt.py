import sys
from typing import cast
from PyQt6.QtWidgets import QApplication, QMainWindow
from PyQt6.QtWebEngineWidgets import QWebEngineView
from PyQt6.QtCore import QUrl
from psn_login_base import PSNLoginParser, LOGIN_URL, REDIRECT_URL
from psn_account import PSNAccount


class PSNLoginQt(QMainWindow):
    def __init__(self):
        super().__init__()
        self.web_view = QWebEngineView()
        self.setCentralWidget(self.web_view)
        self.web_view.load(QUrl(LOGIN_URL))
        self.web_view.urlChanged.connect(self._on_url_changed)
        self.psn_account: PSNAccount | None = None

    def _on_url_changed(self, url: QUrl) -> None:
        current_url = url.toString()
        if current_url.startswith(REDIRECT_URL):
            self.psn_account = PSNLoginParser.parse_psn_account(current_url)
            self.close()

    @classmethod
    def get_psn_account(cls) -> PSNAccount:
        app = QApplication(sys.argv)
        window = cls()
        window.show()
        app.exec()
        return cast(PSNAccount, window.psn_account)
