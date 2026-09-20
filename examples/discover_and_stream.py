"""End-to-end: discover a console on the network, pair with it, and start
streaming - the whole path from nothing to video in one script, instead of
the discover_hosts.py / register_console.py / gui_stream.py split.

Usage:
    python examples/discover_and_stream.py

You'll be asked to pick a console (if more than one answers) and to type in
the registration PIN shown on its screen (PS5: Settings > System > Remote
Play > Link Device; PS4: Settings > Remote Play Connection Settings > Add
Device). That PIN is fresh each time you open that screen, so pairing can't
be fully scripted - everything else here is automatic.

The regist_key/morning pairing credentials from a successful registration
are cached per-console (keyed by its MAC/host_id, not its IP, since that can
change), so re-running this against an already-paired console skips PSN
login and registration entirely. Pass --force-pair to ignore the cache and
pair again (e.g. after un-registering the app on the console).

Needs: pip install -e .[psn,cv,controller]
"""

import os
import sys

import cv2

from platformdirs import user_data_dir

from chiaki_py import Session, discover_hosts
from chiaki_py.config import ChiakiPySettings
from chiaki_py.controller import attach_controller
from chiaki_py.lib import Settings, DiscoveryHost, StreamSession
from chiaki_py.psn.login import PSNLoginQt
from chiaki_py.registration import ConnectKwargs, connect_info_kwargs, register
from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers


def setup_controller(stream_session: StreamSession) -> bool:
    SDL3Backend.init()
    controllers = get_available_controllers()
    if not controllers:
        print("No DualSense controllers found - streaming without input.")
        return False
    attach_controller(controllers[0], stream_session)
    print("Controller attached.")
    return True


def pick_host(hosts: list[DiscoveryHost]) -> DiscoveryHost:
    if len(hosts) == 1:
        return hosts[0]
    print("Found multiple consoles:")
    for i, host in enumerate(hosts):
        kind = "PS5" if host.ps5 else "PS4"
        print(f"  [{i}] {host.host_name} ({host.host_addr}) [{kind}] state={host.state}")
    index = int(input(f"Pick one [0-{len(hosts) - 1}]: "))
    return hosts[index]


def pairing_cache_path(app_dir: str, host: DiscoveryHost) -> str:
    hosts_dir = os.path.join(app_dir, "hosts")
    os.makedirs(hosts_dir, exist_ok=True)
    return os.path.join(hosts_dir, f"{host.host_id}.json")


def get_connect_kwargs(settings: Settings, host: DiscoveryHost, app_dir: str, force_pair: bool) -> ConnectKwargs:
    cache_path = pairing_cache_path(app_dir, host)

    if not force_pair and os.path.exists(cache_path):
        print(f"Reusing saved pairing for '{host.host_name}'.")
        cached = ChiakiPySettings.from_file(cache_path)
        return {
            "host": host.host_addr,
            "nickname": cached.nickname,
            "regist_key": cached.regist_key,
            "morning": bytes.fromhex(cached.morning),
            "target": host.target,
        }

    psn_account = PSNLoginQt.load_or_get(os.path.join(app_dir, "psn_account.json"))
    psn_id = psn_account.user_rpid if host.ps5 else psn_account.online_id

    pin = input("Enter the registration PIN shown on the console's Link Device screen: ").strip()

    print("Registering...")
    result = register(settings, host=host.host_addr, psn_id=psn_id, pin=pin, target=host.target)
    kwargs = connect_info_kwargs(result, host=host.host_addr)

    ChiakiPySettings(
        host=kwargs["host"],
        nickname=kwargs["nickname"],
        regist_key=result.rp_regist_key,
        morning=result.rp_key,
        ps5=host.ps5,
    ).to_file(cache_path)
    print(f"Saved pairing for next time at {cache_path}")

    return kwargs


def main() -> None:
    force_pair = "--force-pair" in sys.argv

    settings = Settings()
    settings.set_log_verbose(False)

    print("Scanning network for consoles (3s)...")
    hosts = discover_hosts(settings, timeout=3.0)
    if not hosts:
        print("No consoles found. Make sure it's powered on and on the same network.")
        sys.exit(1)

    host = pick_host(hosts)
    print(f"Using {host.host_name} ({host.host_addr})")

    app_dir = user_data_dir("ChiakiPyClient", "chiaki-py")
    os.makedirs(app_dir, exist_ok=True)

    kwargs = get_connect_kwargs(settings, host, app_dir, force_pair)
    print(f"Connecting to '{kwargs['nickname']}'.")

    session = Session.connect(settings, **kwargs)
    session.stream_session.on_session_quit().subscribe(lambda reason: print("Session quit:", reason))
    session.stream_session.on_login_pin_requested().subscribe(lambda incorrect: print("Login PIN requested, incorrect:", incorrect))

    try:
        with session:
            controller_attached = setup_controller(session.stream_session)
            
            print("Streaming - press 'q' in the video window, or Ctrl+C in the terminal, to quit.")
            try:
                for frame in session.frames(max_fps=60):
                    cv2.imshow("chiaki-py", cv2.cvtColor(frame, cv2.COLOR_RGB2BGR))
                    if cv2.waitKey(1) & 0xFF == ord("q"):
                        break
            finally:
                if controller_attached:
                    session.stream_session.release_right()
                    session.stream_session.release_left()
                    session.stream_session.send_feedback_state()
    except KeyboardInterrupt:
        print("\nInterrupted, shutting down.")
    finally:
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
