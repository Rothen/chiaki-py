"""Broadcast-scan the local network for PS4/PS5 consoles.

Usage:
    python examples/1.2_discover_hosts.py [--timeout SECONDS]

No pairing/PSN login required - this just listens for consoles announcing
themselves, the same way the PS Remote Play app's "device list" does.
"""
import typer

from chiaki_py import discover_hosts
from chiaki_py.lib import Settings


def main(timeout: float = 3.0) -> None:
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
    typer.run(main)
