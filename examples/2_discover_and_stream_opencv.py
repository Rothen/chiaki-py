"""End-to-end: discover a console on the network, pair with it, and start
streaming into an OpenCV window - the whole path from nothing to video in one
script, instead of the 1.1_login.py / 1.2_discover_hosts.py /
1.3_register_console.py / 1.4.x split.

Usage:
    python examples/2_discover_and_stream_opencv.py [--force-pair] [--headless] [--dir ./cache]

You'll be asked to pick a console (if more than one answers) and to type in
the registration PIN shown on its screen (PS5: Settings > System > Remote
Play > Link Device; PS4: Settings > Remote Play Connection Settings > Add
Device). That PIN is fresh each time you open that screen, so pairing can't
be fully scripted - everything else here is automatic.

The regist_key/morning pairing credentials from a successful registration
are cached per-console as <console name>.json in --dir, so re-running this
against an already-paired console skips PSN login and registration entirely.
Pass --force-pair to ignore the cache and pair again (e.g. after un-registering
the app on the console). --headless logs in to PSN from the terminal instead of
a Qt window. Press 'q' in the video window, or Ctrl+C in the terminal, to quit.
"""

import sys
from pathlib import Path
from typing import Any

import cv2
import typer
import numpy as np

from chiaki_py import Session, discover_hosts, Serializer, HostRegistration, register_host
from chiaki_py.lib import Settings, DiscoveryHost, CpuFrameHandler
from chiaki_py.psn import PSNLoginQt, PSNAccount, LoginError, PSNLoginTerminal, PSNLogin
from chiaki_py.controller import detach_controller
from fps_overlay import FpsCounter, draw_text_top_right
from helpers import setup_controller


def _login(headless: bool):
    LoginClass: type[PSNLogin] = PSNLoginQt
    if headless:
        LoginClass = PSNLoginTerminal
    try:
        return LoginClass.login()
    except LoginError as e:
        print(e.message)
        sys.exit()


def _register(host: DiscoveryHost, dir: Path, headless: bool):
    psn_account = Serializer.load_or(PSNAccount, Path(
        dir, "psn_account.json"), lambda: _login(headless))
    psn_id = psn_account.user_rpid if host.ps5 else psn_account.online_id
    pin = input(
        "Enter the registration PIN shown on the console's Link Device screen: ").strip()
    print("Registering...")
    registration = register_host(
        host=host.host_addr, psn_id=psn_id, pin=pin, target=host.target)
    return registration


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
    hosts_dir.mkdir(exist_ok=True)
    return Path(dir, f"{host.host_name}.json")


def get_registration(host: DiscoveryHost, dir: Path, force_pair: bool, headless: bool) -> HostRegistration:
    cache_path = pairing_cache_path(dir, host)

    if force_pair:
        registration = _register(host, dir, headless)
        Serializer.save(registration, cache_path)
        print(f"Saved pairing for next time at {cache_path}")
    else:
        print(f"Reusing saved pairing for '{host.host_name}'.")
        registration = Serializer.load_or(HostRegistration, cache_path, lambda: _register(host, dir, headless))

    return registration


def main(force_pair: bool = False, headless: bool = False, dir: Path = Path('./cache')) -> None:
    print("Scanning network for consoles (3s)...")
    hosts = discover_hosts(timeout=3.0)
    if not hosts:
        print("No consoles found. Make sure it's powered on and on the same network.")
        sys.exit(1)

    host = pick_host(hosts)
    print(f"Using {host.host_name} ({host.host_addr})")

    dir.mkdir(exist_ok=True)

    registration = get_registration(host, dir, force_pair, headless)
    print(f"Connecting to '{registration.nickname}'.")

    session = Session(registration, CpuFrameHandler)
    session.cp_session.on_session_quit().subscribe(lambda reason: print("Session quit:", reason))
    session.cp_session.on_login_pin_requested().subscribe(lambda incorrect: print("Login PIN requested, incorrect:", incorrect))

    try:
        with session:
            controller, subscriptions = setup_controller(session.cp_session)
            
            print("Streaming - press 'q' in the video window, or Ctrl+C in the terminal, to quit.")
            try:
                profile = session.cp_session.get_video_profile()
                shape = (profile.height, profile.width, 3)
                frame_np: np.ndarray = np.empty(shape, dtype=np.uint8)
                frame_out: Any = frame_np
                fps = FpsCounter()
                for _ in session.frames(max_fps=60, out=frame_out):
                    image = cv2.cvtColor(frame_np, cv2.COLOR_RGB2BGR)
                    measured = fps.tick()
                    if measured is not None:
                        draw_text_top_right(image, f"{measured:.1f} FPS")
                    cv2.imshow("chiaki-py", image)
                    if cv2.waitKey(1) & 0xFF == ord("q"):
                        break
            finally:
                if controller is not None and subscriptions is not None:
                    detach_controller(controller, session.cp_session, subscriptions)
    except KeyboardInterrupt:
        print("\nInterrupted, shutting down.")
    finally:
        cv2.destroyAllWindows()


if __name__ == "__main__":
    typer.run(main)
