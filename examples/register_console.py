"""Pairs with a PS4/PS5 over the local network and writes a config.json
that examples/gui_stream.py (or any chiaki_py.Session.connect call) can use
directly - the end-to-end path from "I have a PSN account" to "I have
connect credentials" that nothing in the raw bindings wires up.

Usage:
    python examples/register_console.py <host> <pin> [--ps4] [--console-pin PIN]

`pin` is the 8-digit code shown on the console's Settings > Remote Play
Connection Settings > Add Device screen.
"""

import argparse
from pathlib import Path

from chiaki_py import Serializer
from chiaki_py.lib import Settings
from chiaki_py.lib.core.common import Target
from chiaki_py.psn import PSNLoginQt, PSNAccount
from chiaki_py.registration import register


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="Console IP address or hostname")
    parser.add_argument("pin", help="8-digit registration PIN shown on the console")
    parser.add_argument("--ps4", action="store_true", help="Register a PS4 instead of a PS5")
    parser.add_argument("--console-pin", default="", help="Console parental-control PIN, if set")
    parser.add_argument("--out", default="chiaki_py_config.json", help="Where to write the resulting config")
    args = parser.parse_args()

    cache_dir = Path("./cache")
    cache_dir.touch(exist_ok=True)
    psn_account = Serializer.load_or(PSNAccount, Path(cache_dir, "psn_account.json"), PSNLoginQt.login)

    target = Target.PS4_8 if args.ps4 else Target.PS5_1
    settings = Settings()
    settings.set_log_verbose(False)

    registration = register(
        settings,
        host=args.host,
        psn_id=psn_account.user_rpid,
        pin=args.pin,
        console_pin=args.console_pin,
        target=target,
    )
    Serializer.save(registration, args.out)

    print(f"Registered '{registration.nickname}'. Wrote {args.out}")


if __name__ == "__main__":
    main()
