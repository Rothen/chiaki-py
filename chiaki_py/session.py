from __future__ import annotations

import logging
import threading
import time
from typing import Iterator

import numpy as np
import numpy.typing as npt

from .lib import Settings, StreamSession, StreamSessionConnectInfo, get_frame
from .registration import Registration

_MAX_FRAME_SHAPE = (1080, 1920, 3)

_logger = logging.getLogger(__name__)


class Session:
    def __init__(self, connect_info: StreamSessionConnectInfo):
        self.connect_info = connect_info
        self.stream_session = StreamSession(connect_info)

    @classmethod
    def connect(
        cls,
        settings: Settings,
        registration: Registration
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
        return cls(connect_info)

    def __enter__(self) -> "Session":
        self.stream_session.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.stop()

    def stop(self) -> None:
        if self.stream_session.is_connected() or self.stream_session.is_connecting():
            self.stream_session.stop()

    def frames(self, max_fps: float = 60.0, copy: bool = True) -> Iterator[npt.NDArray[np.uint8]]:
        min_interval = (1.0 / max_fps) if max_fps > 0 else 0.0
        buffer = np.zeros(_MAX_FRAME_SHAPE, dtype=np.uint8)
        ready = threading.Event()
        subscription = self.stream_session.on_frame_available().subscribe(lambda _: ready.set())
        try:
            last_yield = 0.0
            while True:
                if not ready.wait(timeout=0.5):
                    continue
                ready.clear()
                try:
                    dims = get_frame(self.stream_session, False, buffer)
                except RuntimeError:
                    _logger.warning("Dropping unreadable frame", exc_info=True)
                    continue
                if dims is None:
                    continue
                now = time.perf_counter()
                if now - last_yield < min_interval:
                    continue
                last_yield = now
                height, width = dims
                frame = buffer[:height, :width]
                yield frame.copy() if copy else frame
        finally:
            subscription.unsubscribe()
