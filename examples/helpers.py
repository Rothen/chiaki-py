from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers
from dualsense_py import DualSenseController

from chiaki_py.controller import attach_controller, ControllerSubscriptions
from chiaki_py.lib import ChiakiPySession


def setup_controller(stream_session: ChiakiPySession) -> tuple[DualSenseController | None, ControllerSubscriptions | None]:
    SDL3Backend.init()
    controllers = get_available_controllers()
    if not controllers:
        print("No DualSense controllers found - streaming without input.")
        return None, None
    return controllers[0], attach_controller(controllers[0], stream_session)
