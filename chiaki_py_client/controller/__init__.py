"""DualSense controller input wiring, kept separate from the core package
since it depends on the external `ds_py` library."""

from .dualsense import attach_controller

__all__ = ["attach_controller"]
