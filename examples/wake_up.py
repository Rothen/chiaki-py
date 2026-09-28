import sys
from pathlib import Path

from chiaki_py import Serializer, HostRegistration, discover_hosts
from chiaki_py.lib import DiscoveryManager

if __name__ == "__main__":
    cache_dir = Path("./cache")
    registration_file = Path(cache_dir, "host_registration.json")
    reg = Serializer.load(HostRegistration, registration_file)
    
    hosts = discover_hosts(timeout=5.0)
    if not hosts:
        print(f"No consoles found after {5.0:.1f}s. "
                "Make sure the console is on and on the same network/subnet.")
        sys.exit(-1)

    for host in hosts:
        kind = "PS5" if host.ps5 else "PS4"
        running = f" - playing {host.running_app_name}" if host.running_app_name else ""
        print(f"[{kind}] {host.host_name} ({host.host_addr}) state={host.state}{running}")
    
    # print(reg.host, reg.regist_key)
    
    # DiscoveryManager().send_wakeup(host=reg.host, regist_key=reg.regist_key, ps5=True)