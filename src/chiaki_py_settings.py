"""This is a simple example of a Typer application."""
from __future__ import annotations
from pydantic import BaseModel


class ChiakiPySettings(BaseModel):
    """A class to represent a segment."""
    host: str = "192.168.42.43"
    regist_key: str = "b02d1ceb"
    nickname: str = "PS5-083"
    ps5Id: str = "78c881a8214a"
    morning: str = 'aa3f52ff47431d2f2cf0f14110f679b3'
    initial_login_pin = ""  # None
    duid: str = ""  # None
    auto_regist: bool = False
    fullscreen: bool = False
    zoom: bool = False
    stretch: bool = False
    ps5: bool = True
    discover_timout: int = 2000

    @classmethod
    def from_file(cls, file: str) -> ChiakiPySettings:
        with open(file, encoding='utf-8') as f:
            json_data = f.read()
        return ChiakiPySettings.model_validate_json(json_data)
