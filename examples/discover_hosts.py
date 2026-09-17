"""Broadcast-scan the local network for PS4/PS5 consoles.

Usage:
    python examples/discover_hosts.py [timeout_seconds]

No pairing/PSN login required - this just listens for consoles announcing
themselves, the same way the PS Remote Play app's "device list" does.
"""

import sys

from chiaki_py import Settings
from chiaki_py_client import discover_hosts


def main() -> None:
    timeout = float(sys.argv[1]) if len(sys.argv) > 1 else 2.0

    settings = Settings()
    settings.set_log_verbose(False)

    hosts = discover_hosts(settings, timeout=timeout)
    if not hosts:
        print(f"No consoles found after {timeout:.1f}s. "
              "Make sure the console is on and on the same network/subnet.")
        return

    for host in hosts:
        kind = "PS5" if host.ps5 else "PS4"
        running = f" - playing {host.running_app_name}" if host.running_app_name else ""
        print(f"[{kind}] {host.host_name} ({host.host_addr}) state={host.state}{running}")


if __name__ == "__main__":
    main()
