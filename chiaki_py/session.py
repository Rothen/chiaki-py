from __future__ import annotations

import logging
import threading
import time
from typing import Any, Callable, Iterator, TypeVar
from types import TracebackType

import numpy as np
import numpy.typing as npt

from .lib import GpuFrame, Settings, StreamSession, StreamSessionConnectInfo, CPUFrameHandler, CUDAFrameHandler, GPUFrameHandler
from .registration import Registration

_logger = logging.getLogger(__name__)

_T = TypeVar("_T")

FrameHandler = CPUFrameHandler | CUDAFrameHandler | GPUFrameHandler

class Session:
    def __init__(self, connect_info: StreamSessionConnectInfo, frame_handler_cls: type[FrameHandler] = CPUFrameHandler):
        self.__stream_session = StreamSession(connect_info)
        self.__frame_handler: FrameHandler = frame_handler_cls(self.__stream_session)
        self.__last_get_time = 0.0

    @property
    def stream_session(self) -> StreamSession:
        return self.__stream_session

    @property
    def last_get_time(self) -> float:
        """Seconds the frame handler took for the latest frame delivered by frames() (0.0 before the first)."""
        return self.__last_get_time

    @property
    def frame_handler(self) -> FrameHandler:
        return self.__frame_handler

    @classmethod
    def connect(
        cls,
        settings: Settings,
        registration: Registration,
        frame_handler_cls: type[FrameHandler] = CPUFrameHandler
    ) -> "Session":
        connect_info = StreamSessionConnectInfo(
            settings=settings,
            target=registration.target,
            host=registration.host,
            nickname=registration.nickname,
            regist_key=registration.regist_key,
            morning=registration.morning,
            initial_login_pin=registration.initial_login_pin,
            duid=registration.duid,
            auto_regist=registration.auto_regist,
            fullscreen=registration.fullscreen,
            zoom=registration.zoom,
            stretch=registration.stretch,
        )
        return cls(connect_info, frame_handler_cls)

    def __enter__(self) -> "Session":        
        hw_type = self.__stream_session.hardware_decoder_type()
        
        if isinstance(self.__frame_handler, GPUFrameHandler) and not self.__stream_session.has_hardware_decoder():
            raise RuntimeError(
                "GPUFrameHandler needs a hardware decoder; set one with "
                "Settings.set_hardware_decoder() (e.g. 'vulkan', 'cuda', 'd3d11va') before connecting"
            )
        elif isinstance(self.__frame_handler, CUDAFrameHandler) and hw_type != "cuda":
            raise RuntimeError(
                "CUDAFrameHandler needs the CUDA hardware decoder; set it with "
                f"Settings.set_hardware_decoder('cuda') before connecting (currently: {hw_type or 'none'})"
            )

        self.__stream_session.start()

        return self

    def __exit__(self, exc_type: type[BaseException] | None, exc_value: BaseException | None, traceback: TracebackType | None) -> None:
        self.stop()

    def stop(self) -> None:
        if self.__stream_session.is_connected() or self.__stream_session.is_connecting():
            self.__stream_session.stop()

    def frames(
        self,
        max_fps: float = 60.0,
        out: npt.NDArray[np.uint8] | GpuFrame | Any | None = None,
    ) -> Iterator[npt.NDArray[np.uint8]]:
        """Yield decoded frames as (H, W, 3) uint8 RGB arrays, downloaded to system memory.

        By default every frame is a new array. Pass `out` (C-contiguous, uint8,
        with exactly the stream's (H, W, 3) shape) to have every frame written
        into it instead: the same array is then yielded each time and is
        overwritten by the next frame, so copy it if you keep it or hand it to
        another thread.
        """
        return self._iter_frames(lambda: self.__frame_handler.get_frame(out), max_fps)

    def _iter_frames(self, pull: Callable[[], _T | None], max_fps: float) -> Iterator[_T]:
        min_interval = (1.0 / max_fps) if max_fps > 0 else 0.0
        ready = threading.Event()
        subscription = self.__stream_session.on_frame_available().subscribe(lambda _: ready.set())
        try:
            last_yield = 0.0
            while self.__stream_session.is_connecting() or self.__stream_session.is_connected():
                if not ready.wait(timeout=0.5):
                    continue
                ready.clear()
                try:
                    started = time.perf_counter()
                    frame = pull()
                    get_time = time.perf_counter() - started
                except RuntimeError:
                    _logger.warning("Dropping unreadable frame", exc_info=True)
                    continue
                if frame is None:
                    continue
                self.__last_get_time = get_time
                now = time.perf_counter()
                if now - last_yield < min_interval:
                    continue
                last_yield = now
                yield frame
        finally:
            subscription.unsubscribe()
