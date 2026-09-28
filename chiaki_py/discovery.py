from __future__ import annotations

import time
from typing import List

from .lib import DiscoveryHost, DiscoveryManager


def find_host(host_name: str, timeout: float = 2.0) -> DiscoveryHost | None:
    """Broadcast-discover PS4/PS5 hosts on the local network and return what answered.

    Wraps the start/wait/collect/stop sequence `DiscoveryManager` otherwise
    requires callers to drive manually.
    """
    found_host: DiscoveryHost | None = None
    manager = DiscoveryManager()
    manager.set_active(True)
    try:
        time.sleep(timeout)

        if hosts := manager.get_hosts():
            for host in hosts:
                if host.host_name == host_name:
                    found_host = host
                    break
    finally:
        manager.set_active(False)
        return found_host


def discover_hosts(timeout: float = 2.0) -> List[DiscoveryHost]:
    """Broadcast-discover PS4/PS5 hosts on the local network and return what answered.

    Wraps the start/wait/collect/stop sequence `DiscoveryManager` otherwise
    requires callers to drive manually.
    """
    manager = DiscoveryManager()
    manager.set_active(True)
    try:
        time.sleep(timeout)
        return manager.get_hosts()
    finally:
        manager.set_active(False)
