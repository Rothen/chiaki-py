from __future__ import annotations

from pydantic import BaseModel


class PSNAccount(BaseModel):
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
