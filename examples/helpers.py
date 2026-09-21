from chiaki_py.controller import attach_controller
from chiaki_py.lib import StreamSession
from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers

def setup_controller(stream_session: StreamSession) -> bool:
    SDL3Backend.init()
    controllers = get_available_controllers()
    if not controllers:
        print("No DualSense controllers found - streaming without input.")
        return False
    attach_controller(controllers[0], stream_session)
    print("Controller attached.")
    return True
