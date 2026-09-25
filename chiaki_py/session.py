from __future__ import annotations

import logging
import threading
import time
from typing import Any, Callable, Iterator, TypeVar
from types import TracebackType

import numpy as np
import numpy.typing as npt

from .lib import VulkanFrame, Settings, ChiakiPySession, ChiakiPySessionConnectInfo, CpuFrameHandler, CudaFrameHandler, VulkanFrameHandler, QuitReason, quit_reason_is_error, quit_reason_string
from .registration import HostRegistration

_logger = logging.getLogger(__name__)

_T = TypeVar("_T")

FrameHandler = CpuFrameHandler | CudaFrameHandler | VulkanFrameHandler

SESSION_RETRY_SECONDS = 10.0
"""How long after start() a failed connection attempt is still retried."""
RETRY_DELAY_SECONDS = 1.0
"""Pause between a failed connection attempt and the next one."""

class Session:
    """A Remote Play connection to a console, and the frame handler that pulls its decoded video.

    Wraps the pybind11 `ChiakiPySession` (the actual connection, input and event source) together
    with a `FrameHandler` (how decoded frames are retrieved: to system memory, to a CUDA buffer or
    kept on the Vulkan device - see `frames()`). Build one with `connect()`, which takes a
    `HostRegistration` instead of a raw `ChiakiPySessionConnectInfo`; use it as a context manager, or
    call `stop()` directly, to make sure the connection is torn down.

    A connection attempt that fails before ever connecting is retried every
    `RETRY_DELAY_SECONDS` until `retry_seconds` have passed since the start (0 disables retries).
    A failed retry can't raise into the caller - it runs later, on its own thread - so it is
    logged and kept in `error` instead.
    """

    def __init__(
        self,
        connect_info: ChiakiPySessionConnectInfo,
        frame_handler_cls: type[FrameHandler] = CpuFrameHandler,
        retry_seconds: float = SESSION_RETRY_SECONDS,
    ):
        self.__stream_session = ChiakiPySession(connect_info)
        self.__frame_handler: FrameHandler = frame_handler_cls(self.__stream_session)
        self.__pull_time = 0.0

        self.__retry_seconds = retry_seconds
        self.__connect_deadline = 0.0
        self.__ever_connected = False
        self.__stopping = False
        self.__active = False  # connected, connecting or waiting to retry
        self.__retry_lock = threading.Lock()
        self.__retry_timer: threading.Timer | None = None
        self.error: BaseException | None = None
        """The exception from the last failed retry, or None."""

        self.__subscriptions = [
            self.__stream_session.on_connected_changed().subscribe(self.__on_connected_changed),
            self.__stream_session.on_session_quit().subscribe(self.__on_session_quit),
        ]

    @property
    def stream_session(self) -> ChiakiPySession:
        """The underlying pybind11 connection: input, low-level events, connection state."""
        return self.__stream_session

    @property
    def pull_time(self) -> float:
        """Seconds the frame handler took for the latest frame delivered by frames() (0.0 before the first)."""
        return self.__pull_time

    @property
    def frame_handler(self) -> FrameHandler:
        """The handler frames() pulls decoded frames through; matches `frame_handler_cls`."""
        return self.__frame_handler

    @classmethod
    def connect(
        cls,
        settings: Settings,
        registration: HostRegistration,
        frame_handler_cls: type[FrameHandler] = CpuFrameHandler,
        retry_seconds: float = SESSION_RETRY_SECONDS,
    ) -> "Session":
        """Build a Session for the console `registration` describes. Does not connect yet -
        use the returned Session as a context manager, or call `__enter__`/`stop()` directly,
        to actually start and stop the stream."""
        connect_info = ChiakiPySessionConnectInfo(
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
        return cls(connect_info, frame_handler_cls, retry_seconds)

    @property
    def is_active(self) -> bool:
        """True from start until the session ends for good: stopped, disconnected, or out of retries."""
        return self.__active

    def __on_connected_changed(self, connected: bool) -> None:
        if connected:
            self.__ever_connected = True

    def __on_session_quit(self, reason: QuitReason) -> None:
        # Called on chiaki's session thread, which is the one quitting: only decide here and hand
        # the actual start() over to another thread.
        should_retry = (
            not self.__stopping
            and not self.__ever_connected
            and quit_reason_is_error(reason)
            and time.monotonic() < self.__connect_deadline
        )
        if not should_retry:
            self.__active = False
            return

        _logger.info("Connection failed (%s), retrying in %.0fs", quit_reason_string(reason), RETRY_DELAY_SECONDS)
        with self.__retry_lock:
            self.__retry_timer = threading.Timer(RETRY_DELAY_SECONDS, self.__retry)
            self.__retry_timer.daemon = True
            self.__retry_timer.start()

    def __retry(self) -> None:
        with self.__retry_lock:
            if self.__stopping:
                return
            try:
                self.__stream_session.start()
            except RuntimeError as e:  # ChiakiException
                _logger.error("Retrying the connection failed: %s", e)
                self.error = e
                self.__active = False

    def __enter__(self) -> "Session":
        """Start the connection: raises RuntimeError up front if `frame_handler` needs a hardware
        decoder that `settings` was not set up for, rather than failing once frames start arriving."""
        hw_type = self.__stream_session.hardware_decoder_type()
        
        if isinstance(self.__frame_handler, VulkanFrameHandler) and not self.__stream_session.has_hardware_decoder():
            raise RuntimeError(
                "VulkanFrameHandler needs a hardware decoder; set one with "
                "Settings.set_hardware_decoder() (e.g. 'vulkan', 'cuda', 'd3d11va') before connecting"
            )
        elif isinstance(self.__frame_handler, CudaFrameHandler) and hw_type != "cuda":
            raise RuntimeError(
                "CudaFrameHandler needs the CUDA hardware decoder; set it with "
                f"Settings.set_hardware_decoder('cuda') before connecting (currently: {hw_type or 'none'})"
            )

        self.__stopping = False
        self.__ever_connected = False
        self.error = None
        self.__connect_deadline = time.monotonic() + self.__retry_seconds
        self.__active = True
        try:
            self.__stream_session.start()
        except BaseException:
            self.__active = False
            raise

        return self

    def __exit__(self, exc_type: type[BaseException] | None, exc_value: BaseException | None, traceback: TracebackType | None) -> None:
        self.stop()

    def stop(self) -> None:
        """Disconnect, if a connection is up or in progress, and cancel a pending retry.
        Safe to call more than once."""
        with self.__retry_lock:
            self.__stopping = True
            self.__active = False
            if self.__retry_timer is not None:
                self.__retry_timer.cancel()
                self.__retry_timer = None
        if self.__stream_session.is_connected() or self.__stream_session.is_connecting():
            self.__stream_session.stop()

    def frames(
        self,
        max_fps: float = 60.0,
        out: npt.NDArray[np.uint8] | VulkanFrame | Any | None = None,
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
            next_yield = 0.0
            while self.__active:
                if not ready.wait(timeout=0.5):
                    continue
                ready.clear()
                try:
                    started = time.perf_counter()
                    frame = pull()
                    pull_time = time.perf_counter() - started
                except RuntimeError:
                    _logger.warning("Dropping unreadable frame", exc_info=True)
                    continue
                if frame is None:
                    continue
                self.__pull_time = pull_time
                now = time.perf_counter()
                if now < next_yield:
                    continue
                next_yield = max(next_yield + min_interval, now - min_interval)
                yield frame
        finally:
            subscription.unsubscribe()
