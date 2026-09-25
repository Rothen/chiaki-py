"""Pairs with a PS4/PS5 over the local network and writes
cache/host_registration.json, which the 1.4.x streaming examples (or any
chiaki_py.Session instance) can use directly.

Usage:
    python examples/1.3_register_console.py <host> <pin> [--ps4] [--console-pin PIN]

Needs the PSN account saved by 1.1_login.py. `pin` is the 8-digit code shown on
the console's Link Device screen (PS5: Settings > System > Remote Play > Link
Device; PS4: Settings > Remote Play Connection Settings > Add Device).
"""

import argparse
import sys
from pathlib import Path

from chiaki_py import Serializer, register_host
from chiaki_py.lib import Settings, Target
from chiaki_py.psn import PSNLoginQt, PSNAccount


def main(host: str, pin: str, ps4: bool = False, console_pin: str = "") -> None:
    cache_dir = Path("./cache")
    psn_account_file = Path(cache_dir, "psn_account.json")
    registration_file = Path(cache_dir, "host_registration.json")
    
    if not cache_dir.exists() or not psn_account_file.exists():
        print(f"PSN Account not found under {psn_account_file}. Run examples/1.1_login.py first")
        sys.exit(1)

    psn_account = Serializer.load_or(PSNAccount, psn_account_file, PSNLoginQt.login)

    target = Target.PS4_8 if ps4 else Target.PS5_1
    settings = Settings()
    settings.set_log_verbose(False)

    registration = register_host(
        settings,
        host=host,
        psn_id=psn_account.user_rpid,
        pin=pin,
        console_pin=console_pin,
        target=target,
    )

    Serializer.save(registration, registration_file)
    print(f"Registered '{registration.nickname}'. Wrote {registration_file}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pair with a PS4/PS5 over the local network.")
    parser.add_argument("host", help="IP address or hostname of the console.")
    parser.add_argument("pin", help="8-digit code from the console's Link Device screen.")
    parser.add_argument("--ps4", action="store_true", help="Register a PS4 instead of a PS5.")
    parser.add_argument("--console-pin", default="", help="Console PIN, if one is set.")
    args = parser.parse_args()
    main(host=args.host, pin=args.pin, ps4=args.ps4, console_pin=args.console_pin)
