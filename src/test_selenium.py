import sys
import requests
from urllib.parse import parse_qs, urlparse
from typing import Any
import base64
import webview
from Cryptodome.Hash import SHA256
from psn_login import PSNLoginFlow

psn_login_flow: PSNLoginFlow = PSNLoginFlow()
print(psn_login_flow.get_psn_account())
sys.exit()




def generate_basic_auth_header(username: str, password: str):
    credentials = f"{username}:{password}"
    encoded_credentials = base64.b64encode(
        credentials.encode("utf-8")).decode("utf-8")
    return f"Basic {encoded_credentials}"

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


def get_user_account(redirect_url: str) -> dict[str, Any] | None:
    """Return user account.

    Account should be formatted with \
        :meth:`format_user_account() <pyremoteplay.profile.format_user_account>` before use.

    :param redirect_url: Redirect url found after logging in
    """
    code: str | None = _parse_redirect_url(redirect_url)
    if code is None:
        return None
    token = _get_token(code)
    if token is None:
        return None
    account = _fetch_account_info(token)
    return account


def _get_token(code: str) -> str | None:
    print("Sending POST request")
    print(TOKEN_URL)
    print(HEADERS)
    print(TOKEN_BODY.format(code).encode("ascii"))
    print((CLIENT_ID, CLIENT_SECRET))
    body = TOKEN_BODY.format(code).encode("ascii")
    resp = requests.post(
        TOKEN_URL,
        headers=HEADERS,
        data=body,
        auth=(CLIENT_ID, CLIENT_SECRET),
        timeout=3,
    )
    if resp.status_code == 200:
        content = resp.json()
        token = content.get("access_token")
        return token
    print("Error getting token. Got response: %s", resp.status_code)
    return None


def _fetch_account_info(token: str) -> dict[str, Any] | None:
    resp = requests.get(
        f"{TOKEN_URL}/{token}",
        headers=HEADERS,
        auth=(CLIENT_ID, CLIENT_SECRET),
        timeout=3,
    )
    if resp.status_code == 200:
        account_info = resp.json()
        account_info = _format_account_info(account_info)
        return account_info
    print("Error getting account. Got response: %s", resp.status_code)
    return None


def _parse_redirect_url(redirect_url: str) -> str | None:
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
    print("Got Auth Code: %s"%(code, ))
    return code


def _format_account_info(account_info: dict[str, Any]) -> dict[str, Any]:
    user_id = account_info["user_id"]
    user_b64 = _format_user_id(user_id, "base64")
    user_creds = _format_user_id(user_id, "sha256")
    account_info["user_rpid"] = user_b64
    account_info["credentials"] = user_creds
    return account_info


def _format_user_id(user_id: str | None, encoding: str = "base64"):
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


def on_loaded():
    try:
        current_url = window.get_current_url()
        print(f"Page loaded: {current_url}")
        if current_url is not None and  current_url.startswith(REDIRECT_URL):
            print("🎯 Target page has been reached!")
            print(get_user_account(current_url))
            window.destroy()
    except Exception as e:
        pass


# Create window
window = webview.create_window("Waiting for target page...", LOGIN_URL)

# Start webview with on_loaded callback
window.events.loaded += on_loaded
webview.start()
