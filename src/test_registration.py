import sys
from typing import Any
import threading
import signal
from chiaki_py import Backend, Settings
from chiaki_py.core.common import Target
from psn_login_qt import PSNLoginQt
from psn_account import PSNAccount

psn_account: PSNAccount

try:
    psn_account = PSNAccount.load("psn_account.json")
except FileNotFoundError:
    print("PSN Account not found")
    try:
        psn_account = PSNLoginQt.get_psn_account()
        psn_account.save("psn_account.json")
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

exit_event = threading.Event()

import base64

encoded_bytes = psn_account.user_rpid.encode("utf-8")  # Equivalent to `toUtf8()`
decoded_bytes = base64.b64decode(encoded_bytes)  # Equivalent to `QByteArray::fromBase64()`

print(decoded_bytes)  # This is the decoded output
sys.exit()

host: str = "192.168.42.32"
psn_id: str = psn_account.user_rpid # base64
pin: str = ""
cpin: str = ""
broadcast: bool = False
target: Target = Target.PS5_1
callback: lambda: None = lambda: exit_event.set()

settings: Settings = Settings()
backend: Backend = Backend(settings)


backend.register_host(
    host=host,
    psn_id=psn_id,
    pin=pin,
    cpin=cpin,
    broadcast=broadcast,
    target=target,
    callback=callback
)


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
