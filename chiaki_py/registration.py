from __future__ import annotations

from typing import TypedDict
from .lib import Backend, RegistResult, Settings
from .lib.core.common import Target

class ConnectKwargs(TypedDict):
    host: str
    nickname: str
    regist_key: str
    morning: bytes
    target: Target


def register(
    settings: Settings,
    *,
    host: str,
    psn_id: str,
    pin: str,
    console_pin: str = "",
    target: Target,
    broadcast: bool = False,
) -> RegistResult:
    backend = Backend(settings)
    return backend.register_host(
        host=host,
        psn_id=psn_id,
        pin=pin,
        cpin=console_pin,
        broadcast=broadcast,
        target=target,
    )


def connect_info_kwargs(result: RegistResult, *, host: str) -> ConnectKwargs:
    return {
        "host": host,
        "nickname": result.server_nickname,
        "regist_key": result.rp_regist_key,
        "morning": bytes.fromhex(result.rp_key),
        "target": result.target,
    }
