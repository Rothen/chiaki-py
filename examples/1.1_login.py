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
from pathlib import Path

import typer

from chiaki_py import Serializer
from chiaki_py.psn import PSNLoginQt, PSNLoginTerminal, PSNLogin, LoginError


def main(headless: bool = False) -> None:
    cache_dir = Path("./cache")
    cache_dir.mkdir(exist_ok=True)
    psn_account_file = Path(cache_dir, "psn_account.json")

    LoginClass: type[PSNLogin] = PSNLoginQt
    if headless:
        LoginClass = PSNLoginTerminal
    try:
        psn_account = LoginClass.login()
    except LoginError as e:
        print(e.message)
        sys.exit()

    Serializer.save(psn_account, Path(cache_dir, "psn_account.json"))
    print(f"PSN Account '{psn_account.online_id}'. Wrote {psn_account_file}")


if __name__ == "__main__":
    typer.run(main)
