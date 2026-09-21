"""Pairs with a PS4/PS5 over the local network and writes a config.json
that examples/gui_stream.py (or any chiaki_py.Session.connect call) can use
directly - the end-to-end path from "I have a PSN account" to "I have
connect credentials" that nothing in the raw bindings wires up.

Usage:
    python examples/register_console.py <host> <pin> [--ps4] [--console-pin PIN]

`pin` is the 8-digit code shown on the console's Settings > Remote Play
Connection Settings > Add Device screen.
"""

import sys
import argparse
from pathlib import Path

from chiaki_py import Serializer
from chiaki_py.psn import PSNLoginQt, PSNLoginTerminal, PSNLogin, LoginError


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--headless", action="store_true", help="Register a PS4 instead of a PS5")
    args = parser.parse_args()

    cache_dir = Path("./cache")
    cache_dir.touch(exist_ok=True)
    LoginClass: type[PSNLogin] = PSNLoginQt
    if args.headless:
        LoginClass = PSNLoginTerminal
    try:
        Serializer.save(LoginClass.login(), Path(cache_dir, "psn_account.json"))
    except LoginError as e:
        print(e.message)
        sys.exit()


if __name__ == "__main__":
    main()
