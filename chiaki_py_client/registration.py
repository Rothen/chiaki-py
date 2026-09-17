from __future__ import annotations

from chiaki_py import Backend, RegistResult, Settings
from chiaki_py.core.common import Target


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
    """Pair with a console and return its `RegistResult`.

    This is a thin, blocking wrapper around `chiaki_py.Backend.register_host`.
    Use `connect_info_kwargs()` to turn the result into the keyword arguments
    `Session.connect()`/`StreamSessionConnectInfo` need - nothing in the raw
    bindings does that translation for you.
    """
    backend = Backend(settings)
    return backend.register_host(
        host=host,
        psn_id=psn_id,
        pin=pin,
        cpin=console_pin,
        broadcast=broadcast,
        target=target,
    )


def connect_info_kwargs(result: RegistResult, *, host: str) -> dict:
    """Turn a `RegistResult` into kwargs for `Session.connect()`/`StreamSessionConnectInfo`.

    `result.rp_regist_key` and `result.rp_key` are exactly `regist_key` and
    `morning` (as a hex string) - chiaki-py just never wired the two
    together, so every caller ended up hand-copying these fields themselves.
    """
    return {
        "host": host,
        "nickname": result.server_nickname,
        "regist_key": result.rp_regist_key,
        "morning": bytes.fromhex(result.rp_key),
        "target": result.target,
    }
