# type: ignore
from __future__ import annotations
from typing import Type

from sdl3 import *

from .backend import Backend
from .sdl3_device_info import SDL3DeviceInfo


class SDL3Backend(Backend[SDL3DeviceInfo]):
    @staticmethod
    def _get_available_devices() -> list[SDL3DeviceInfo]:
        available_devices: list[SDL3DeviceInfo] = []

        ids_ptr = SDL_GetGamepads(None)
        if ids_ptr:
            i = 0
            while True:
                id_value = ids_ptr[i]
                if id_value == 0:
                    break
                available_devices.append(SDL3DeviceInfo(id_value))
                i += 1

            SDL_free(ids_ptr)

        return available_devices

    @staticmethod
    def _init() -> Type[SDL3Backend]:
        SDL_Init(SDL_INIT_GAMEPAD)
        return SDL3Backend

    @staticmethod
    def _quit() -> None:
        SDL_Quit()
