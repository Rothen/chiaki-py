"""Logs in to a PSN account and saves it to cache/psn_account.json, which
1.3_register_console.py needs to pair with a console.

Usage:
    python examples/1.1_login.py [--headless]

By default a Qt window opens for you to sign in; with --headless the login
happens in the terminal instead.
"""

import argparse
import sys
from pathlib import Path

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
    parser = argparse.ArgumentParser(description="Log in to a PSN account.")
    parser.add_argument(
        "--headless", action="store_true", help="Log in via the terminal instead of a Qt window."
    )
    args = parser.parse_args()
    main(headless=args.headless)
