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

import sys
from pathlib import Path

import cv2
import typer

from chiaki_py import Session, discover_hosts, Serializer
from chiaki_py.controller import attach_controller
from chiaki_py.lib import Settings, DiscoveryHost, StreamSession
from chiaki_py.psn import PSNLoginQt, PSNAccount, LoginError, PSNLoginTerminal, PSNLogin
from chiaki_py.registration import register, Registration
from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers


def _login(headless: bool):
    LoginClass: type[PSNLogin] = PSNLoginQt
    if headless:
        LoginClass = PSNLoginTerminal
    try:
        return LoginClass.login()
    except LoginError as e:
        print(e.message)
        sys.exit()


def _register(settings: Settings, host: DiscoveryHost, dir: Path, headless: bool):
    psn_account = Serializer.load_or(PSNAccount, Path(
        dir, "psn_account.json"), lambda: _login(headless))
    psn_id = psn_account.user_rpid if host.ps5 else psn_account.online_id
    pin = input(
        "Enter the registration PIN shown on the console's Link Device screen: ").strip()
    print("Registering...")
    registration = register(
        settings, host=host.host_addr, psn_id=psn_id, pin=pin, target=host.target)
    return registration


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


def pairing_cache_path(dir: Path, host: DiscoveryHost) -> Path:
    hosts_dir = Path(dir, "hosts")
    hosts_dir.touch(exist_ok=True)
    return Path(dir, f"{host.host_name}.json")


def get_registration(settings: Settings, host: DiscoveryHost, dir: Path, force_pair: bool, headless: bool) -> Registration:
    cache_path = pairing_cache_path(dir, host)

    if force_pair:
        registration = _register(settings, host, dir, headless)
        Serializer.save(registration, cache_path)
        print(f"Saved pairing for next time at {cache_path}")
    else:
        print(f"Reusing saved pairing for '{host.host_name}'.")
        registration = Serializer.load_or(Registration, cache_path, lambda: _register(settings, host, dir, headless))

    return registration


def main(force_pair: bool = False, headless: bool = False, dir: Path = Path('./cache')) -> None:
    settings = Settings()
    settings.set_log_verbose(False)

    print("Scanning network for consoles (3s)...")
    hosts = discover_hosts(settings, timeout=3.0)
    if not hosts:
        print("No consoles found. Make sure it's powered on and on the same network.")
        sys.exit(1)

    host = pick_host(hosts)
    print(f"Using {host.host_name} ({host.host_addr})")

    dir.touch(exist_ok=True)

    registration = get_registration(settings, host, dir, force_pair, headless)
    print(f"Connecting to '{registration.nickname}'.")

    session = Session.connect(settings, registration)
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
    typer.run(main)
