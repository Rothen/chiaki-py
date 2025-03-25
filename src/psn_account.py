from __future__ import annotations
from dataclasses import dataclass
import json

@dataclass
class PSNAccount:
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
    
    def save(self, json_path: str) -> None:
        with open(json_path, 'w') as f:
            json.dump(self.__dict__, f)

    @classmethod
    def load(cls, json_path: str) -> PSNAccount:
        with open(json_path, 'r') as f:
            data = json.load(f)
        return cls(**data)
