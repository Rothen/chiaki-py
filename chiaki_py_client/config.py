from __future__ import annotations

from pydantic import BaseModel


class ChiakiPySettings(BaseModel):
    """Connection settings for a single console, loadable from a JSON file."""

    host: str
    regist_key: str
    nickname: str
    morning: str
    initial_login_pin: str = ""
    duid: str = ""
    auto_regist: bool = False
    fullscreen: bool = False
    zoom: bool = False
    stretch: bool = False
    ps5: bool = True
    discover_timeout: float = 2.0

    @classmethod
    def from_file(cls, file: str) -> "ChiakiPySettings":
        with open(file, encoding="utf-8") as f:
            return cls.model_validate_json(f.read())
