import webview
from psn_login_base import PSNLoginParser, REDIRECT_URL, LOGIN_URL
from psn_account import PSNAccount

class PSNLoginFlow:
    @classmethod
    def get_psn_account(cls) -> PSNAccount:
        cls.window = webview.create_window("PSN Login", LOGIN_URL)
        cls.window.events.loaded += cls._on_loaded
        webview.start()
        return cls.psn_account

    @classmethod
    def _on_loaded(cls):
        try:
            current_url = cls.window.get_current_url()
            if current_url is not None and current_url.startswith(REDIRECT_URL):
                cls.psn_account = PSNLoginParser.parse_psn_account(current_url)
                cls.window.destroy()
        except Exception:
            pass