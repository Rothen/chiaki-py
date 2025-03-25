import sys
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

print(psn_account)
