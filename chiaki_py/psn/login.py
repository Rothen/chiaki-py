from __future__ import annotations

import base64
import sys
from pathlib import Path
from typing import Any
from abc import ABC, abstractmethod
from urllib.parse import parse_qs, urlparse

import requests
from Cryptodome.Hash import SHA256
from PyQt6.QtCore import QObject, QUrl
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtQml import QQmlApplicationEngine
from PyQt6.QtWebEngineQuick import QtWebEngineQuick

from .._qt import check_qt_platform_libs
from .account import PSNAccount

CLIENT_ID = "ba495a24-818c-472b-b12d-ff231c1b5745"
CLIENT_SECRET = "mvaiZkRsAsI1IBkY"
REDIRECT_URL = "https://remoteplay.dl.playstation.net/remoteplay/redirect"
LOGIN_URL = (
    "https://auth.api.sonyentertainmentnetwork.com/"
    "2.0/oauth/authorize?service_entity=urn:service-entity:psn"
    f"&response_type=code&client_id={CLIENT_ID}"
    f"&redirect_uri={REDIRECT_URL}"
    "&scope=psn:clientapp referenceDataService:countryConfig.read pushNotification:webSocket.desktop.connect sessionManager:remotePlaySession.system.update"
    "&request_locale=en_US"
    "&ui=pr"
    "&service_logo=ps"
    "&layout_type=popup"
    "&smcid=remoteplay"
    "&prompt=always"
    "&PlatformPrivacyWs1=minimal&"
)
TOKEN_URL = "https://auth.api.sonyentertainmentnetwork.com/2.0/oauth/token"
TOKEN_BODY = "grant_type=authorization_code" "&code={}" f"&redirect_uri={REDIRECT_URL}&"
HEADERS = {"Content-Type": "application/x-www-form-urlencoded"}
QML_PATH = Path(__file__).with_name("login.qml")


class PSNLoginParser:
    """Drives the Sony PSN OAuth code exchange once a redirect URL is captured."""

    @classmethod
    def parse_psn_account(cls, redirect_url: str) -> PSNAccount:
        """Turn the redirect URL PSN's OAuth login page ends on (starting with `REDIRECT_URL`,
        carrying a `code` query parameter) into a `PSNAccount`: exchanges the code for a token
        and fetches the account info with it. Raises ValueError if the URL or the exchange is
        invalid, or if either HTTP call fails."""
        code = cls.__parse_redirect_url(redirect_url)
        if code is None:
            raise ValueError("Code is None")
        token = cls.__get_token(code)
        if token is None:
            raise ValueError("Token is None")
        return cls.__fetch_account_info(token)

    @classmethod
    def __get_token(cls, code: str) -> str | None:
        resp = requests.post(
            TOKEN_URL,
            headers=HEADERS,
            data=TOKEN_BODY.format(code).encode("ascii"),
            auth=(CLIENT_ID, CLIENT_SECRET),
            timeout=3,
        )
        if resp.status_code == 200:
            return resp.json().get("access_token")
        raise ValueError(f"Error getting token: HTTP {resp.status_code}")

    @classmethod
    def __fetch_account_info(cls, token: str) -> PSNAccount:
        resp = requests.get(
            f"{TOKEN_URL}/{token}",
            headers=HEADERS,
            auth=(CLIENT_ID, CLIENT_SECRET),
            timeout=3,
        )
        if resp.status_code != 200:
            raise ValueError(f"Error getting account: HTTP {resp.status_code}")

        account_info = resp.json()
        user_b64, user_creds = cls.__format_psn_account_info(account_info)
        return PSNAccount(
            scopes=account_info["scopes"],
            expiration=account_info["expiration"],
            client_id=account_info["client_id"],
            dcim_id=account_info["dcim_id"],
            grant_type=account_info["grant_type"],
            user_id=account_info["user_id"],
            user_uuid=account_info["user_uuid"],
            online_id=account_info["online_id"],
            country_code=account_info["country_code"],
            language_code=account_info["language_code"],
            community_domain=account_info["community_domain"],
            is_sub_account=account_info["is_sub_account"],
            user_rpid=user_b64,
            credentials=user_creds,
        )

    @classmethod
    def __parse_redirect_url(cls, redirect_url: str) -> str | None:
        if not redirect_url.startswith(REDIRECT_URL):
            return None
        query = parse_qs(urlparse(redirect_url).query)
        code = query.get("code")
        if not code or len(code[0]) <= 1:
            return None
        return code[0]

    @classmethod
    def __format_psn_account_info(cls, account_info: dict[str, Any]) -> tuple[str, str]:
        if "user_id" not in account_info:
            raise ValueError("User ID not in account info")
        user_id = account_info["user_id"]
        return (
            cls.__format_user_id(user_id, "base64"),
            cls.__format_user_id(user_id, "sha256"),
        )

    @classmethod
    def __format_user_id(cls, user_id: str, encoding: str) -> str:
        if encoding == "sha256":
            return SHA256.new(user_id.encode()).digest().hex()
        if encoding == "base64":
            return base64.b64encode(int(user_id).to_bytes(8, "little")).decode()
        raise TypeError(f"{encoding} encoding is not valid")


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

    def _on_redirect_captured(self, url: str) -> None:
        self.psn_account = PSNLoginParser.parse_psn_account(url)

    @classmethod
    def login(cls) -> PSNAccount:
        if QGuiApplication.instance() is None:
            check_qt_platform_libs()
            QtWebEngineQuick.initialize()  # pyright: ignore[reportCallIssue]

        app = QGuiApplication.instance() or QGuiApplication(sys.argv)

        login = PSNLoginQt()
        engine = QQmlApplicationEngine()
        engine.load(QUrl.fromLocalFile(str(QML_PATH)))
        if not engine.rootObjects():
            raise RuntimeError(f"Failed to load {QML_PATH}")

        root: QObject = engine.rootObjects()[0]
        root.redirectCaptured.connect(login._on_redirect_captured)  # type: ignore[attr-defined]

        app.exec()
        if login.psn_account is None:
            raise LoginError("Unable to retrieve login information")

        return login.psn_account


class PSNLoginTerminal(PSNLogin):
    """Prints the PSN login URL and reads the resulting redirect URL back from the terminal."""

    @classmethod
    def login(cls) -> PSNAccount:
        print("Open this URL in a browser and sign in to your PlayStation account:\n")
        print(LOGIN_URL)
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
