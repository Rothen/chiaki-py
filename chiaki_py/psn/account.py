from __future__ import annotations

from pydantic import BaseModel


class PSNAccount(BaseModel):
    """A signed-in PSN account, as returned by `PSNLogin.login()`. `user_rpid` is the base64
    account-ID `register_host()`/`Backend.register_host()` expects as `psn_id` for a PS5 (or a
    PS4 in "PS4 8.0" mode); `online_id` is what an older PS4 expects there instead."""

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
