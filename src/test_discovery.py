from typing import Any
import threading
import signal
from chiaki_py import DiscoveryManager

from .chiaki_py_settings import ChiakiPySettings

chiaki_py_settings: ChiakiPySettings = ChiakiPySettings.from_file("settings.json")

exit_event = threading.Event()

discovery_manager: DiscoveryManager = DiscoveryManager()


def signal_handler(sig: int, frame: Any) -> None:
    """Handles Ctrl+C to stop the session gracefully."""
    print("\nCtrl+C detected! Stopping stream session...")
    # stream_session.stop()
    exit_event.set()


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    
    discovery_manager.send_wakeup(chiaki_py_settings.host, chiaki_py_settings.regist_key, True)

    print("Starting stream session...")

    print("Started")

    print("Session closed. Exiting...")
