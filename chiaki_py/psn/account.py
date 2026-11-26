from __future__ import annotations

from pydantic import BaseModel


class PSNTokens(BaseModel):
    """The OAuth tokens of a signed-in account. `access_token` is what `auto_register_host()` and
    `list_psn_devices()` take; it expires at `expires_at` (Unix time), after which `refresh_token`
    gets a new one (see `chiaki_py.psn.fresh_access_token()`)."""

    access_token: str
    refresh_token: str
    expires_at: float


class PSNAccount(BaseModel):
    """A signed-in PSN account, as returned by `PSNLogin.login()`. `user_rpid` is the base64
    account-ID `register_host()`/`auto_register_host()` expect as `psn_id`/`psn_account_id` for a
    PS5 (or a PS4 in "PS4 8.0" mode); `online_id` is what an older PS4 expects there instead.
    `tokens` is None for an account saved by a chiaki-py older than 0.1.1: log in again to get
    them, PIN-less registration needs them."""

    scopes: str
    expiration: str
    client_id: str
    dcim_id: str
    grant_type: str
    user_id: str
    user_uuid: str
    online_id: str
    country_code: str
    language_code: str
    community_domain: str
    is_sub_account: bool
    user_rpid: str
    credentials: str
    tokens: PSNTokens | None = None
