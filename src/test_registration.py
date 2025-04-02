import os
import gc
from typing import Any
import threading
import signal
from chiaki_py import Backend, Settings
from chiaki_py.core.common import Target
from psn_account import PSNAccount
import time
from psn_login_qt import PSNLoginQt
from platformdirs import user_data_dir
appname = "ChiakiPy"
author = "Banana-Bros"
app_dir: str = user_data_dir(appname, author)
if not os.path.exists(app_dir):
    os.makedirs(app_dir)

gc.disable()

psn_account_path: str = os.path.join(app_dir, "psn_account.json")
psn_account: PSNAccount = PSNLoginQt.load_or_get(psn_account_path)

exit_event = threading.Event()

host: str = "192.168.42.32"
psn_id: str = psn_account.user_rpid # base64
pin: str = "11572734"
cpin: str = ""
broadcast: bool = False
target: Target = Target.PS5_1

settings: Settings = Settings()
settings.set_log_verbose(False)
backend: Backend = Backend(
    settings,
    log_callback=lambda x, y: print('PyLog:', x, y),
    failed_callback=lambda: print('failed'),
    success_callback=lambda x: print(x)
)


backend.register_host(
    host=host,
    psn_id=psn_id,
    pin=pin,
    cpin=cpin,
    broadcast=broadcast,
    target=target
)

def signal_handler(sig: int, frame: Any) -> None:
    """Handles Ctrl+C to stop the session gracefully."""
    print("\nCtrl+C detected! Stopping stream session...")
    # stream_session.stop()
    exit_event.set()

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    
    # discovery_manager.send_wakeup(chiaki_py_settings.host, chiaki_py_settings.regist_key, True)

    print("Starting stream session...")

    print("Started")
    
    while True:
        time.sleep(1)

    print("Session closed. Exiting...")
