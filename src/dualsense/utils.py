from hidapi_py import get_all_device_infos

from .backends import Backend
from .dual_sense_controller import DualSenseController


SONY_VENDOR_ID: int = 0x054C
DS_PRODUCT_ID: int = 0x0CE6

MS_VENDOR_ID: int = 0x045E
XBOX_CONTROLLER_PRODUCT_ID: int = 0x028E


def get_all_controllers(vendor_id: int = 0, product_id: int = 0) -> list[DualSenseController]:
    return [DualSenseController(d) for d in get_all_device_infos(vendor_id, product_id)]

def get_all_dual_sense_controllers() -> list[DualSenseController]:
    return get_all_controllers(SONY_VENDOR_ID, DS_PRODUCT_ID)

def get_all_xbox_360_controllers() -> list[DualSenseController]:
    return get_all_controllers(MS_VENDOR_ID, XBOX_CONTROLLER_PRODUCT_ID)

def get_available_controllers() -> list[DualSenseController]:
    return [DualSenseController(d) for d in Backend.get_available_devices()]
