from __future__ import annotations

import threading
import time
from typing import Iterator

import numpy as np
import numpy.typing as npt

from chiaki_py import Settings, StreamSession, StreamSessionConnectInfo, get_frame
from chiaki_py.core.common import Target

# chiaki-ng negotiates up to 1080p; a buffer this size fits any stream
# resolution it can produce, regardless of what the console actually sends.
_MAX_FRAME_SHAPE = (1080, 1920, 3)


class Session:
    """A Pythonic wrapper around the raw `chiaki_py.StreamSession`.

    Handles the connect/disconnect lifecycle as a context manager and
    exposes decoded video frames as NumPy arrays through `frames()`,
    instead of requiring callers to drive `StreamSession`/`get_frame`
    and a frame-available subscription themselves.
    """

    def __init__(self, connect_info: StreamSessionConnectInfo):
        self.connect_info = connect_info
        self.stream_session = StreamSession(connect_info)

    @classmethod
    def connect(
        cls,
        settings: Settings,
        *,
        host: str,
        nickname: str,
        regist_key: str,
        morning: bytes,
        target: Target,
        initial_login_pin: str = "",
        duid: str = "",
        auto_regist: bool = False,
        fullscreen: bool = False,
        zoom: bool = False,
        stretch: bool = False,
    ) -> "Session":
        """Build a Session from already-known console pairing credentials.

        `regist_key`/`morning` normally come from `chiaki_py_client.registration.register()`.
        """
        connect_info = StreamSessionConnectInfo(
            settings=settings,
            target=target,
            host=host,
            nickname=nickname,
            regist_key=regist_key,
            morning=morning,
            initial_login_pin=initial_login_pin,
            duid=duid,
            auto_regist=auto_regist,
            fullscreen=fullscreen,
            zoom=zoom,
            stretch=stretch,
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
        """Yield decoded video frames as they become available.

        Waits on the native frame-available event instead of polling, and
        drops (rather than queues) frames arriving faster than `max_fps`.
        Each yielded array is cropped to the frame's actual (height, width);
        set `copy=False` to get a view into a buffer reused across calls
        (cheaper, but the data is only valid until the next frame arrives).
        """
        min_interval = (1.0 / max_fps) if max_fps > 0 else 0.0
        buffer = np.zeros(_MAX_FRAME_SHAPE, dtype=np.uint8)
        ready = threading.Event()
        subscription = self.stream_session.on_frame_available().subscribe(lambda _: ready.set())
        try:
            last_yield = 0.0
            while True:
                # A timeout instead of an unbounded wait means we return to
                # Python bytecode regularly even if no frame ever arrives -
                # without it, a stalled stream leaves this blocked in a C
                # call indefinitely, which also blocks KeyboardInterrupt
                # (Ctrl+C) from ever being delivered.
                if not ready.wait(timeout=0.5):
                    continue
                ready.clear()
                dims = get_frame(self.stream_session, False, buffer)
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
