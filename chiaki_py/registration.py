from __future__ import annotations

from typing import Annotated

from pydantic import BaseModel, BeforeValidator, PlainSerializer, PlainValidator

from .lib import Backend, Settings
from .lib.core.common import Target


HexBytes = Annotated[
    bytes,
    BeforeValidator(lambda v: bytes.fromhex(v) if isinstance(v, str) else v),
    PlainSerializer(lambda b: b.hex(), return_type=str),
]


def _parse_target(v: Target | str | int) -> Target:
    if isinstance(v, Target):
        return v
    if isinstance(v, str):
        return Target.__members__[v]
    return Target(v)


TargetField = Annotated[
    Target,
    PlainValidator(_parse_target),
    PlainSerializer(lambda t: t.name, return_type=str),
]


class Registration(BaseModel):
    """Connection settings for a single console, loadable from a JSON file."""

    host: str
    target: TargetField
    regist_key: str
    nickname: str
    morning: HexBytes
    initial_login_pin: str = ""
    duid: str = ""
    auto_regist: bool = False
    fullscreen: bool = False
    zoom: bool = False
    stretch: bool = False
    ps5: bool = True
    discover_timeout: float = 2.0


def register(
    settings: Settings,
    host: str,
    psn_id: str,
    pin: str,
    console_pin: str = "",
    target: Target = Target.PS5_1,
    broadcast: bool = False,
) -> Registration:
    result = Backend(settings).register_host(
        host=host,
        psn_id=psn_id,
        pin=pin,
        cpin=console_pin,
        broadcast=broadcast,
        target=target,
    )
    return Registration(
        host=host,
        target=Target.PS5_1,
        regist_key=result.rp_regist_key,
        nickname=result.server_nickname,
        morning=result.rp_key,
    )
