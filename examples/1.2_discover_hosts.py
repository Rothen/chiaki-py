"""Broadcast-scan the local network for PS4/PS5 consoles.

Usage:
    python examples/1.2_discover_hosts.py [--timeout SECONDS]

No pairing/PSN login required - this just listens for consoles announcing
themselves, the same way the PS Remote Play app's "device list" does.
"""
import argparse

from chiaki_py import discover_hosts


def main(timeout: float = 3.0) -> None:
    hosts = discover_hosts(timeout=timeout)
    if not hosts:
        print(f"No consoles found after {timeout:.1f}s. "
              "Make sure the console is on and on the same network/subnet.")
        return

    for host in hosts:
        kind = "PS5" if host.ps5 else "PS4"
        running = f" - playing {host.running_app_name}" if host.running_app_name else ""
        print(f"[{kind}] {host.host_name} ({host.host_addr}) state={host.state}{running}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Discover PS4/PS5 consoles on the local network.")
    parser.add_argument(
        "--timeout", type=float, default=3.0, help="Seconds to listen for consoles (default: 3.0)."
    )
    args = parser.parse_args()
    main(timeout=args.timeout)
