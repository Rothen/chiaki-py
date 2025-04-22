from __future__ import annotations
from abc import ABC, abstractmethod
from typing import Generic, TypeVar, Any, Type


from .device_infos import DeviceInfo


_DeviceInfoType = TypeVar("DeviceInfoType", bound=DeviceInfo[Any, Any])


class Backend(ABC, Generic[_DeviceInfoType]):
    """
    Abstract base class for backend implementations.
    """

    ActiveBackend: Type[Backend[Any]] | None = None

    @staticmethod
    @abstractmethod
    def _get_available_devices() -> list[_DeviceInfoType]:
        """
        Open a connection to the device at the specified path.
        """

    @staticmethod
    @abstractmethod
    def _init() -> Type[Backend[_DeviceInfoType]]:
        pass

    @staticmethod
    @abstractmethod
    def _quit() -> None:
        pass

    __slots__ = ( )

    @classmethod
    def init(cls) -> None:
        """
        Initialize the backend.
        """
        if cls == Backend:
            raise TypeError("Cannot initialize the base Backend class directly.")

        Backend.ActiveBackend = cls._init()

    @staticmethod
    def get_available_devices() -> list[_DeviceInfoType]:
        """
        Open a connection to the device at the specified path.
        """
        if Backend.ActiveBackend is None:
            raise TypeError("Cannot call get_available_devices() without initializing the backend.")

        return Backend.ActiveBackend._get_available_devices()

    @staticmethod
    def quit() -> None:
        """
        Close the backend.
        """
        if Backend.ActiveBackend is None:
            return

        Backend.ActiveBackend._quit()
        Backend.ActiveBackend = None
