"""DualSense controller input wiring, kept separate from the core package
since it depends on the external `dualsensepy` library."""

from .dualsense import attach_controller, detach_controller, ControllerSubscriptions

__all__ = ["attach_controller", "detach_controller", "ControllerSubscriptions"]
