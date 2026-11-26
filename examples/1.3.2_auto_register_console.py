"""Registers with a PS5 or PS4 through PSN, without the PIN from its Link Device screen (chiaki-ng's
"automatic registration"), and writes cache/host_registration.json like 1.3_register_console.py.

Usage:
    python examples/1.3.2_auto_register_console.py [name] [--ps4] [--host IP] [--timeout SECONDS]

Needs the PSN account saved by 1.1_login.py, with tokens (log in again if you did so before
chiaki-py 0.1.1). Picks the PS5 called `name` among those linked to the account, or the only one
there is; lists them otherwise. With --ps4 it registers the account's primary PS4 instead, the only
PS4 PSN can reach.

The console must be signed in to the account, have Remote Play enabled, and be on or in rest mode
with "Stay connected to the internet" on. It need not be on the same network: everything goes
through PSN. The streaming examples then find it on the local network by its name.
"""

import argparse
import logging
import sys
from pathlib import Path

import requests

from chiaki_py import AutoRegistrationError, Serializer, auto_register_host, list_psn_devices
from chiaki_py.psn import PSNAccount, fresh_access_token, missing_scopes


def main(name: str | None, ps4: bool, host: str, timeout: float) -> None:
    cache_dir = Path("./cache")
    psn_account_file = Path(cache_dir, "psn_account.json")
    registration_file = Path(cache_dir, "host_registration.json")

    if not psn_account_file.exists():
        print(f"PSN Account not found under {psn_account_file}. Run examples/1.1_login.py first")
        sys.exit(1)
    account = Serializer.load(PSNAccount, psn_account_file)
    if missing := missing_scopes(account):
        print(f"The saved PSN login lacks {', '.join(missing)}. Run examples/1.1_login.py again")
        sys.exit(1)
    try:
        access_token = fresh_access_token(account)
    except (ValueError, requests.RequestException) as e:
        print(f"No usable PSN tokens ({e}). Run examples/1.1_login.py again")
        sys.exit(1)
    Serializer.save(account, psn_account_file)  # keeps the tokens if they were just refreshed

    try:
        registration = auto_register_host(
            psn_token=access_token,
            psn_account_id=account.user_rpid,
            console_name=name,
            ps4=ps4,
            host=host,
            timeout=timeout,
        )
    except ValueError as e:  # no or several matching PS5s
        print(e)
        if not ps4:
            print("The account's PS5s:", ", ".join(d.name for d in list_psn_devices(access_token)) or "none")
        sys.exit(1)
    except (RuntimeError, AutoRegistrationError) as e:
        print(e)
        sys.exit(1)

    Serializer.save(registration, registration_file)
    print(f"Registered '{registration.nickname}'. Wrote {registration_file}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Register with a PS5/PS4 through PSN, without a PIN.")
    parser.add_argument("name", nargs="?", help="The console's name; optional if the account has one PS5.")
    parser.add_argument("--ps4", action="store_true", help="Register the account's primary PS4 instead of a PS5.")
    parser.add_argument("--host", default="", help="The console's local IP, if known; else found by name when streaming.")
    parser.add_argument("--timeout", type=float, default=60.0, help="Seconds to wait for the console once connected.")
    parser.add_argument("--verbose", action="store_true", help="Show chiaki's log of the PSN connection and registration.")
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO if args.verbose else logging.WARNING, format="%(name)s %(levelname)s: %(message)s")
    main(name=args.name, ps4=args.ps4, host=args.host, timeout=args.timeout)
