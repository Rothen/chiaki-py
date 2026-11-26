from __future__ import annotations

import threading
from typing import Annotated, Any

from pydantic import BaseModel, PlainSerializer, PlainValidator

from chiaki_lib import (
    Backend,
    ChiakiPySession,
    ChiakiPySessionConnectInfo,
    PsnDevice,
    QuitReason,
    RegisteredHost,
    Settings,
    quit_reason_string,
)
from chiaki_lib.core.common import Target


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


class HostRegistration(BaseModel):
    """Connection settings for a single console, loadable from a JSON file."""

    host: str
    target: TargetField
    regist_key: str
    nickname: str
    morning: str
    initial_login_pin: str = ""
    duid: str = ""
    # The console's PSN device ID, set by auto_register_host(). Kept apart from `duid` because a
    # non-empty `duid` makes a session connect over PSN, while registrations stream on the local network.
    psn_duid: str = ""
    auto_regist: bool = False
    fullscreen: bool = False
    zoom: bool = False
    stretch: bool = False
    ps5: bool = True
    discover_timeout: float = 2.0


def register_host(
    host: str,
    psn_id: str,
    pin: str,
    console_pin: str = "",
    target: Target = Target.PS5_1,
    broadcast: bool = False,
) -> HostRegistration:
    """Register with a console (the one-time PIN pairing Remote Play needs before it can stream)
    and return the resulting `HostRegistration`, ready to `Serializer.save()` and later pass to
    `Session()`.

    `host` is the console's IP/hostname; `psn_id` is the account-ID (PS5, or a PS4 in "PS4 8.0"
    mode) or PSN online ID (older PS4) of the account being registered - see `PSNAccount`. `pin`
    is the one-time PIN shown on the console's Remote Play registration screen; `console_pin` is
    only needed for a console that has a login PIN set. Blocks until registration finishes;
    raises RuntimeError on failure (wrong PIN, console unreachable, ...).
    """
    result = Backend().register_host(
        host=host,
        psn_id=psn_id,
        pin=pin,
        cpin=console_pin,
        broadcast=broadcast,
        target=target,
    )
    return HostRegistration(
        host=host,
        target=target,
        regist_key=result.rp_regist_key,
        nickname=result.server_nickname,
        morning=result.rp_key,
        ps5=target.name.startswith("PS5"),
    )


class AutoRegistrationError(ConnectionError):
    """`auto_register_host()` failed: the PSN connection could not be set up, the console refused
    the registration (`reason` is then the session's QuitReason, e.g. PsnRegistFailed), or it did
    not answer in time."""

    def __init__(self, message: str, reason: QuitReason | None = None):
        super().__init__(message)
        self.reason = reason


def list_psn_devices(psn_token: str, remoteplay_only: bool = True) -> list[PsnDevice]:
    """The PS5s linked to the PSN account `psn_token` (an access token, see
    `chiaki_py.psn.fresh_access_token()`) belongs to; by default only those with Remote Play
    enabled, the others can't be reached. Each has a `name` and the `duid` `auto_register_host()`
    takes. PSN lists no PS4s. Raises RuntimeError if PSN can't be asked or refuses, e.g. for an
    expired token."""
    devices = Backend.list_psn_devices(psn_token)
    return [d for d in devices if d.remoteplay_enabled or not remoteplay_only]


def auto_register_host(
    psn_token: str,
    psn_account_id: str,
    console_name: str | None = None,
    duid: str = "",
    ps4: bool = False,
    host: str = "",
    timeout: float = 60.0,
    settings: Settings | None = None,
) -> HostRegistration:
    """Register with a console through PSN, without the PIN from its Link Device screen
    (chiaki-ng's "automatic registration"), and return the `HostRegistration`, like `register_host()`.

    `psn_token` is the account's OAuth access token and `psn_account_id` its base64 account-ID: pass
    `fresh_access_token(account)` and `account.user_rpid` of a `PSNAccount`. The console must be
    signed in to that account, have Remote Play enabled, and be on or in rest mode with "Stay
    connected to the internet" on; it need not be on the same network.

    A PS5 is picked by `duid` if given, else by `console_name` among `list_psn_devices()`, else the
    account's only one. With `ps4` the account's primary PS4 is registered instead, the only PS4
    PSN can reach; if `console_name` is given and that PS4 is called differently, the registration
    is discarded and AutoRegistrationError raised.

    PSN does not tell the console's local address: pass it as `host` if known, otherwise the
    registration's `host` stays empty and has to be filled in before streaming (e.g. from
    `find_host(registration.nickname)`). `timeout` bounds the wait for the console once the PSN
    connection is up, which itself can take up to a minute. Blocks until done. Raises ValueError if
    no PS5 matches, RuntimeError if PSN refuses (e.g. an expired token), and AutoRegistrationError
    if the connection or the registration fails.
    """
    if ps4:
        duid = Backend.psn_main_ps4_duid()
    elif not duid:
        duid = _pick_ps5(psn_token, console_name)

    settings = settings if settings is not None else Settings()
    settings.set_psn_auth_token(psn_token)
    settings.set_psn_account_id(psn_account_id)

    registered = _run_auto_regist(settings, duid, ps5=not ps4, timeout=timeout)

    if ps4 and console_name and registered["nickname"] != console_name:
        raise AutoRegistrationError(
            f"'{registered['nickname']}' is the account's primary PS4, not '{console_name}': "
            "only the primary PS4 can be registered through PSN"
        )

    return HostRegistration(
        host=host,
        target=registered["target"],
        regist_key=registered["regist_key"],
        nickname=registered["nickname"],
        morning=registered["morning"],
        psn_duid=duid,
        ps5=not ps4,
    )


def _pick_ps5(psn_token: str, console_name: str | None) -> str:
    devices = list_psn_devices(psn_token)
    matches = [d for d in devices if console_name is None or d.name == console_name]
    if len(matches) == 1:
        return matches[0].duid
    if not devices:
        raise ValueError("No PS5 with Remote Play enabled is linked to this PSN account")
    names = ", ".join(f"'{d.name}'" for d in devices)
    if console_name:
        raise ValueError(f"No PS5 called '{console_name}' is linked to this PSN account, only {names}")
    raise ValueError(f"Several PS5s are linked to this PSN account, pass console_name: {names}")


def _run_auto_regist(settings: Settings, duid: str, ps5: bool, timeout: float) -> dict[str, Any]:
    """Run a PSN session with `auto_regist`: it registers over the PSN connection, reports the
    result through on_auto_regist_succeeded() and quits without streaming."""
    session = ChiakiPySession(
        ChiakiPySessionConnectInfo(
            settings=settings,
            target=Target.PS5_1 if ps5 else Target.PS4_10,
            host="",
            nickname="",
            regist_key="",
            morning="",
            initial_login_pin="",
            duid=duid,
            auto_regist=True,
            fullscreen=False,
            zoom=False,
            stretch=False,
        )
    )

    done = threading.Event()
    registered: dict[str, Any] = {}
    quit_reasons: list[QuitReason] = []
    errors: list[BaseException] = []

    # Both run on chiaki's session thread: hand everything, failures included, over to this one.
    def on_registered(registered_host: RegisteredHost) -> None:
        try:
            # Copy out right away: the RegisteredHost only lives as long as this call.
            registered.update(
                target=registered_host.target,
                nickname=registered_host.server_nickname,
                regist_key=registered_host.rp_regist_key,
                morning=registered_host.rp_key,
            )
        except BaseException as e:
            errors.append(e)
        finally:
            done.set()

    def on_quit(reason: QuitReason) -> None:
        quit_reasons.append(reason)
        done.set()

    subscriptions = [
        session.on_auto_regist_succeeded().subscribe(on_registered),
        session.on_session_quit().subscribe(on_quit),
    ]
    try:
        try:
            session.connect_psn_connection(duid, ps5)
        except RuntimeError as e:
            raise AutoRegistrationError(f"Could not connect to the console through PSN: {e}") from e
        session.start()
        if not done.wait(timeout):
            raise AutoRegistrationError(f"The console did not answer within {timeout:.0f} seconds")
        if errors:
            raise AutoRegistrationError(f"The console registered, but reading its registration failed: {errors[0]!r}")
        if not registered:
            reason = quit_reasons[0] if quit_reasons else None
            detail = quit_reason_string(reason) if reason is not None else "unknown reason"
            raise AutoRegistrationError(f"Registration failed: {detail}", reason)
        return registered
    finally:
        for subscription in subscriptions:
            subscription.unsubscribe()
        session.stop()
