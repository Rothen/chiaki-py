from __future__ import annotations

import time
from typing import List

from .lib import DiscoveryHost, DiscoveryManager, Settings


def discover_hosts(settings: Settings, timeout: float = 2.0) -> List[DiscoveryHost]:
    """Broadcast-discover PS4/PS5 hosts on the local network and return what answered.

    Wraps the start/wait/collect/stop sequence `DiscoveryManager` otherwise
    requires callers to drive manually.
    """
    manager = DiscoveryManager()
    manager.set_settings(settings)
    manager.set_active(True)
    try:
        time.sleep(timeout)
        return manager.discovery_service_hosts()
    finally:
        manager.set_active(False)
