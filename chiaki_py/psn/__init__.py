"""PSN OAuth account login, kept separate from the core package since it
pulls in requests/pycryptodome (and login.py additionally needs PyQt6)."""

from .account import PSNAccount

__all__ = ["PSNAccount"]
