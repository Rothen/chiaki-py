from __future__ import annotations

import base64
import sys
from typing import Any, cast
from urllib.parse import parse_qs, urlparse

import requests
from Cryptodome.Hash import SHA256
from PyQt6.QtCore import QUrl
from PyQt6.QtWidgets import QApplication, QMainWindow
from PyQt6.QtWebEngineWidgets import QWebEngineView

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


class PSNLoginParser:
    """Drives the Sony PSN OAuth code exchange once a redirect URL is captured."""

    @classmethod
    def parse_psn_account(cls, redirect_url: str) -> PSNAccount:
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


class PSNLoginQt(QMainWindow):
    """Opens a PSN login page in an embedded browser and captures the resulting account."""

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
        app = QApplication.instance() or QApplication(sys.argv)
        window = cls()
        window.show()
        app.exec()
        return cast(PSNAccount, window.psn_account)

    @classmethod
    def load_or_get(cls, json_path: str) -> PSNAccount:
        try:
            return PSNAccount.load(json_path)
        except FileNotFoundError:
            psn_account = cls.get_psn_account()
            psn_account.save(json_path)
            return psn_account
