from __future__ import annotations

import sys
from pathlib import Path
from abc import ABC, abstractmethod

from PyQt6.QtCore import QObject, QUrl
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtQml import QQmlApplicationEngine
from PyQt6.QtWebEngineQuick import QtWebEngineQuick

from .._qt import check_qt_platform_libs
from .account import PSNAccount
from .auth import REDIRECT_URL, account_from_redirect, login_url

QML_PATH = Path(__file__).with_name("login.qml")


class PSNLoginParser:
    """Drives the Sony PSN OAuth code exchange once a redirect URL is captured."""

    @classmethod
    def parse_psn_account(cls, redirect_url: str) -> PSNAccount:
        """Turn the redirect URL PSN's OAuth login page ends on (starting with `REDIRECT_URL`,
        carrying a `code` query parameter) into a `PSNAccount`, tokens included: exchanges the code
        for tokens and fetches the account info with them. Raises ValueError if the URL or the
        exchange is invalid, or if either HTTP call fails."""
        return account_from_redirect(redirect_url)


class LoginError(Exception):
    """Raised by `PSNLogin.login()` when signing in did not produce a usable `PSNAccount`
    (the user closed the window, gave no redirect URL, or it could not be parsed)."""

    def __init__(self, message: str):
        self.message = message
        super().__init__(self.message)


class PSNLogin(ABC):
    """A way to sign in to a PSN account and get back the `PSNAccount` needed to register with
    a console: `PSNLoginQt` (an embedded browser) or `PSNLoginTerminal` (paste the URL by hand)."""

    @classmethod
    @abstractmethod
    def login(cls) -> PSNAccount:
        """Sign in interactively and return the resulting account. Raises LoginError on failure."""
        ...


class PSNLoginQt(PSNLogin):
    """Opens a PSN login page in an embedded QML WebEngineView and captures the resulting account."""

    def __init__(self) -> None:
        self.psn_account: PSNAccount | None = None
        self.error: Exception | None = None

    def _on_redirect_captured(self, url: str) -> None:
        try:
            self.psn_account = PSNLoginParser.parse_psn_account(url)
        except Exception as e:  # raised inside a Qt slot it would only be printed
            self.error = e

    @classmethod
    def login(cls) -> PSNAccount:
        if QGuiApplication.instance() is None:
            check_qt_platform_libs()
            QtWebEngineQuick.initialize()  # pyright: ignore[reportCallIssue]

        app = QGuiApplication.instance() or QGuiApplication(sys.argv)

        login = PSNLoginQt()
        engine = QQmlApplicationEngine()
        engine.setInitialProperties({"loginUrl": login_url(), "redirectUrl": REDIRECT_URL})
        engine.load(QUrl.fromLocalFile(str(QML_PATH)))
        if not engine.rootObjects():
            raise RuntimeError(f"Failed to load {QML_PATH}")

        root: QObject = engine.rootObjects()[0]
        root.redirectCaptured.connect(login._on_redirect_captured)  # type: ignore[attr-defined]

        app.exec()
        if login.psn_account is None:
            detail = f": {login.error}" if login.error else ""
            raise LoginError(f"Unable to retrieve login information{detail}")

        return login.psn_account


class PSNLoginTerminal(PSNLogin):
    """Prints the PSN login URL and reads the resulting redirect URL back from the terminal."""

    @classmethod
    def login(cls) -> PSNAccount:
        print("Open this URL in a browser and sign in to your PlayStation account:\n")
        print(login_url())
        print(
            "\nAfter signing in you land on a page that starts with "
            f"{REDIRECT_URL}\nCopy the full URL from the browser's address bar and paste it here."
        )
        try:
            redirect_url = input("\nRedirect URL: ").strip()
        except EOFError as e:
            raise LoginError("No redirect URL provided") from e

        try:
            return PSNLoginParser.parse_psn_account(redirect_url)
        except ValueError as e:
            raise LoginError(f"Unable to retrieve login information: {e}") from e
