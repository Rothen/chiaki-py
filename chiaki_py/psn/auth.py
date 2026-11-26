"""PSN OAuth without any UI: the login URL, exchanging the `code` its redirect carries for tokens,
refreshing those, and fetching the account they belong to. The flow is chiaki-ng's
(gui/include/psnaccountid.h, gui/src/psntoken.cpp). `login.py` puts a browser or terminal on top.
"""

from __future__ import annotations

import base64
import hashlib
import time
from urllib.parse import parse_qs, quote, urlparse

import requests

from chiaki_lib import Backend

from .account import PSNAccount, PSNTokens

CLIENT_ID = "ba495a24-818c-472b-b12d-ff231c1b5745"
CLIENT_SECRET = "mvaiZkRsAsI1IBkY"
REDIRECT_URL = "https://remoteplay.dl.playstation.net/remoteplay/redirect"
SCOPE = (
    "psn:clientapp referenceDataService:countryConfig.read "
    "pushNotification:webSocket.desktop.connect sessionManager:remotePlaySession.system.update"
)
AUTHORIZE_URL = (
    "https://auth.api.sonyentertainmentnetwork.com/"
    "2.0/oauth/authorize?service_entity=urn:service-entity:psn"
    f"&response_type=code&client_id={CLIENT_ID}"
    f"&redirect_uri={REDIRECT_URL}"
    # Percent-encoded: with literal spaces, terminals and copy-paste cut the URL off after the
    # first scope, and PSN then grants only psn:clientapp, which PSN sessions get a 401 with.
    f"&scope={quote(SCOPE)}"
    "&request_locale=en_US"
    "&ui=pr"
    "&service_logo=ps"
    "&layout_type=popup"
    "&smcid=remoteplay"
    "&prompt=always"
    "&PlatformPrivacyWs1=minimal&"
)
TOKEN_URL = "https://auth.api.sonyentertainmentnetwork.com/2.0/oauth/token"
# Refresh the access token when it expires within this many seconds, like chiaki-ng does.
REFRESH_MARGIN_SECONDS = 60
_TIMEOUT = 10


def login_url() -> str:
    """The URL to sign in at. Like chiaki-ng, it carries a fresh device ID for this client."""
    return f"{AUTHORIZE_URL}duid={Backend.generate_client_device_uid()}&"


def parse_code(redirect_url: str) -> str:
    """The OAuth `code` from the URL PSN's login page redirects to. Raises ValueError if there is none."""
    if not redirect_url.startswith(REDIRECT_URL):
        raise ValueError(f"URL does not start with {REDIRECT_URL}")
    code = parse_qs(urlparse(redirect_url).query).get("code")
    if not code or len(code[0]) <= 1:
        raise ValueError("URL carries no code")
    return code[0]


def _request_tokens(form: dict[str, str], previous_refresh_token: str = "") -> PSNTokens:
    resp = requests.post(
        TOKEN_URL,
        data={**form, "scope": SCOPE, "redirect_uri": REDIRECT_URL},
        auth=(CLIENT_ID, CLIENT_SECRET),
        timeout=_TIMEOUT,
    )
    if resp.status_code != 200:
        raise ValueError(f"Error getting token: HTTP {resp.status_code}")
    info = resp.json()
    if "access_token" not in info:
        raise ValueError("Response carries no access token")
    return PSNTokens(
        access_token=info["access_token"],
        refresh_token=info.get("refresh_token", previous_refresh_token),
        expires_at=time.time() + int(info.get("expires_in", 0)),
    )


def get_tokens(code: str) -> PSNTokens:
    """Exchange the `code` from the login redirect for tokens. Raises ValueError if PSN refuses."""
    return _request_tokens({"grant_type": "authorization_code", "code": code})


def refresh_tokens(tokens: PSNTokens) -> PSNTokens:
    """New tokens from `tokens.refresh_token`. Raises ValueError if PSN refuses: log in again then."""
    return _request_tokens(
        {"grant_type": "refresh_token", "refresh_token": tokens.refresh_token}, tokens.refresh_token
    )


def fetch_account(tokens: PSNTokens) -> PSNAccount:
    """The account `tokens` belong to, with `tokens` attached. Raises ValueError if PSN refuses."""
    resp = requests.get(f"{TOKEN_URL}/{tokens.access_token}", auth=(CLIENT_ID, CLIENT_SECRET), timeout=_TIMEOUT)
    if resp.status_code != 200:
        raise ValueError(f"Error getting account: HTTP {resp.status_code}")

    info = resp.json()
    if "user_id" not in info:
        raise ValueError("User ID not in account info")
    user_id = info["user_id"]
    return PSNAccount(
        scopes=info["scopes"],
        expiration=info["expiration"],
        client_id=info["client_id"],
        dcim_id=info["dcim_id"],
        grant_type=info["grant_type"],
        user_id=user_id,
        user_uuid=info["user_uuid"],
        online_id=info["online_id"],
        country_code=info["country_code"],
        language_code=info["language_code"],
        community_domain=info["community_domain"],
        is_sub_account=info["is_sub_account"],
        user_rpid=base64.b64encode(int(user_id).to_bytes(8, "little")).decode(),
        credentials=hashlib.sha256(user_id.encode()).hexdigest(),
        tokens=tokens,
    )


def account_from_redirect(redirect_url: str) -> PSNAccount:
    """The signed-in account, tokens included, from the URL PSN's login page redirects to.
    Raises ValueError if the URL carries no code or PSN refuses it."""
    return fetch_account(get_tokens(parse_code(redirect_url)))


def missing_scopes(account: PSNAccount) -> list[str]:
    """The scopes PSN sessions (and so `auto_register_host()`) need that the account was not granted."""
    granted = set(account.scopes.split())
    return [scope for scope in SCOPE.split() if scope not in granted]


def fresh_access_token(account: PSNAccount) -> str:
    """The account's access token, refreshed in place first if it expires within a minute. Save the
    account afterwards so the refreshed tokens are kept (see `Serializer.save()`). Raises ValueError
    if the account has no tokens (saved by an older chiaki-py) or they can't be refreshed: log in
    again then."""
    if account.tokens is None:
        raise ValueError("The PSN account has no tokens; log in again")
    if account.tokens.expires_at - time.time() < REFRESH_MARGIN_SECONDS:
        account.tokens = refresh_tokens(account.tokens)
    return account.tokens.access_token
