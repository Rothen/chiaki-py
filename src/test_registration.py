import os
import sys
from typing import Any
import threading
import signal
from chiaki_py import Backend, Settings
from chiaki_py.core.common import Target
from psn_account import PSNAccount
from psn_login_qt import PSNLoginQt
from platformdirs import user_data_dir

appname = "ChiakiPy"
author = "Banana-Bros"
app_dir: str = user_data_dir(appname, author)
if not os.path.exists(app_dir):
    os.makedirs(app_dir)

psn_account_path: str = os.path.join(app_dir, "psn_account.json")
psn_account: PSNAccount = PSNLoginQt.load_or_get(psn_account_path)

exit_event = threading.Event()

host: str = "192.168.42.32"
psn_id: str = psn_account.user_rpid # base64
pin: str = "64907664"
cpin: str = ""
broadcast: bool = False
target: Target = Target.PS5_1

settings: Settings = Settings()
settings.set_log_verbose(False)
backend: Backend = Backend(settings)


try:
    regist_event = backend.register_host(
        host=host,
        psn_id=psn_id,
        pin=pin,
        cpin=cpin,
        broadcast=broadcast,
        target=target
    )
    
    print(regist_event)
except Exception as e:
    print(f"Error: {e}")

def signal_handler(sig: int, frame: Any) -> None:
    """Handles Ctrl+C to stop the session gracefully."""
    exit_event.set()

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    
    while not exit_event.is_set():
        exit_event.wait(0.1)  # Check every 100ms
