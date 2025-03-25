import requests
import base64
import webview
from typing import Any
from urllib.parse import parse_qs, urlparse
from Cryptodome.Hash import SHA256
from typing import Optional
from psn_account import PSNAccount

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
TOKEN_BODY = (
    "grant_type=authorization_code" "&code={}" f"&redirect_uri={REDIRECT_URL}&"
)
HEADERS = {"Content-Type": "application/x-www-form-urlencoded"}

class PSNLoginFlow:
    def __init__(self):
        self.result: Optional[PSNAccount] = None
        self.window = webview.create_window("PSN Login", LOGIN_URL)

    def get_psn_account(self) -> Optional[PSNAccount]:
        self.window.events.loaded += self._on_loaded
        webview.start()
        return self.result

    def get_user_account(self, redirect_url: str) -> PSNAccount | None:
        """Return user account.

        Account should be formatted with \
            :meth:`format_user_account() <pyremoteplay.profile.format_user_account>` before use.

        :param redirect_url: Redirect url found after logging in
        """
        code: str | None = self._parse_redirect_url(redirect_url)
        if code is None:
            return None
        token = self._get_token(code)
        if token is None:
            return None
        account = self._fetch_account_info(token)
        return account

    def _get_token(self, code: str) -> str | None:
        print("Sending POST request")
        resp = requests.post(
            TOKEN_URL,
            headers=HEADERS,
            data=TOKEN_BODY.format(code).encode("ascii"),
            auth=(CLIENT_ID, CLIENT_SECRET),
            timeout=3,
        )
        if resp.status_code == 200:
            content = resp.json()
            token = content.get("access_token")
            return token
        print("Error getting token. Got response: %s", resp.status_code)
        return None

    def _fetch_account_info(self, token: str) -> PSNAccount | None:
        print('Fetching account info')
        resp = requests.get(
            f"{TOKEN_URL}/{token}",
            headers=HEADERS,
            auth=(CLIENT_ID, CLIENT_SECRET),
            timeout=3,
        )
        if resp.status_code == 200:
            account_info = resp.json()
            user_b64, user_creds = self._format_psn_account_info(account_info)
            psn_account: PSNAccount = PSNAccount(
                scopes=account_info['scopes'],
                expiration=account_info['expiration'],
                client_id=account_info['client_id'],
                dcim_id=account_info['dcim_id'],
                grant_type=account_info['grant_type'],
                user_id=account_info['user_id'],
                user_uuid=account_info['user_uuid'],
                online_id=account_info['online_id'],
                country_code=account_info['country_code'],
                language_code=account_info['language_code'],
                community_domain=account_info['community_domain'],
                is_sub_account=account_info['is_sub_account'],
                user_rpid=user_b64,
                credentials=user_creds
            )
            return psn_account
        print("Error getting account. Got response: %s", resp.status_code)
        return None

    def _parse_redirect_url(self, redirect_url: str) -> str | None:
        if not redirect_url.startswith(REDIRECT_URL):
            print("Redirect URL does not start with %s", REDIRECT_URL)
            return None
        code_url = urlparse(redirect_url)
        query = parse_qs(code_url.query)
        code = query.get("code")
        if code is None:
            print("Code not in query")
            return None
        code = code[0]
        if len(code) <= 1:
            print("Code is too short")
            return None
        print("Got Auth Code: %s" % (code, ))
        return code

    def _format_psn_account_info(self, account_info: dict[str, Any]) -> tuple[str, str]:
        if 'user_id' not in account_info:
            raise ValueError("User ID not in account info")

        user_id = account_info['user_id']
        user_b64 = self._format_user_id(user_id, "base64")
        user_creds = self._format_user_id(user_id, "sha256")
        if user_b64 is None or user_creds is None:
            raise ValueError("User ID is None")
        return user_b64, user_creds

    def _format_user_id(self, user_id: str | None, encoding: str = "base64") -> str | None:
        """Format user id into useable encoding."""
        valid_encodings = {"base64", "sha256"}
        if encoding not in valid_encodings:
            raise TypeError(
                f"{encoding} encoding is not valid. Use {valid_encodings}")

        if user_id is not None:
            if encoding == "sha256":
                user_id = SHA256.new(user_id.encode()).digest().hex()
            elif encoding == "base64":
                user_id = base64.b64encode(
                    int(user_id).to_bytes(8, "little")).decode()
        return user_id

    def _on_loaded(self):
        try:
            current_url = self.window.get_current_url()
            if current_url is not None and current_url.startswith(REDIRECT_URL):
                self.result = self.get_user_account(current_url)
                self.window.destroy()
        except Exception:
            pass
