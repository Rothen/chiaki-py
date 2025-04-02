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

    @classmethod
    def load_or_get(cls, json_path: str) -> PSNAccount:
        psn_account: PSNAccount
        try:
            psn_account = PSNAccount.load(json_path)
        except FileNotFoundError:
            print("PSN Account not found")
            try:
                psn_account = cls.get_psn_account()
                psn_account.save("psn_account.json")
            except Exception as e:
                print(f"Error: {e}")
                sys.exit(1)
        return psn_account
