from __future__ import annotations

import logging
import threading
import time
from typing import Any, Iterator
from types import TracebackType

import numpy as np
import numpy.typing as npt

from .lib import VulkanFrame, Settings, ChiakiPySession, ChiakiPySessionConnectInfo, CpuFrameHandler, CudaFrameHandler, VulkanFrameHandler, QuitReason, quit_reason_is_error, quit_reason_string
from .registration import HostRegistration
from .event_iterator import AudioFrameEventIterator, FrameEventIterator

_logger = logging.getLogger(__name__)

FrameHandler = CpuFrameHandler | CudaFrameHandler | VulkanFrameHandler


class SessionConnectError(ConnectionError):
    """The console refused the session or the connection failed; `reason` says why
    (e.g. QuitReason.SessionRequestRpInUse when another Remote Play session is running)."""

    def __init__(self, reason: QuitReason):
        super().__init__(f"Session could not be established: {quit_reason_string(reason)}")
        self.reason = reason


class Session:
    """A Remote Play connection to a console, and the frame handler that pulls its decoded video.

    Wraps the pybind11 `ChiakiPySession` (the actual connection, input and event source) together
    with a `FrameHandler` (how decoded frames are retrieved: to system memory, to a CUDA buffer or
    kept on the Vulkan device - see `frames()`). It is built from a `HostRegistration` instead of a
    raw `ChiakiPySessionConnectInfo`, and `frame_handler_cls` picks the handler. Use it as a context
    manager, or call `connect()` and `disconnect()` yourself, so the connection is always torn down.
    """

    def __init__(
        self,
        registration: HostRegistration,
        frame_handler_cls: type[FrameHandler] = CpuFrameHandler,
        settings: Settings | None = None,
    ):
        settings = settings if settings is not None else Settings()
        
        if frame_handler_cls == VulkanFrameHandler:
            settings.set_hardware_decoder("vulkan")
        elif frame_handler_cls == CudaFrameHandler:
            settings.set_hardware_decoder("cuda")
        
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

        self.__cp_session = ChiakiPySession(connect_info)
        self.__frame_handler: FrameHandler = frame_handler_cls(self.__cp_session)
        self.__active = False
        self.__connected = threading.Event()
        self.__quit = threading.Event()
        self.__quit_reason: QuitReason | None = None

        self.__subscriptions = [
            self.__cp_session.on_session_quit().subscribe(self.__on_session_quit),
            self.__cp_session.on_connected_changed().subscribe(self.__on_connected_changed),
        ]

    @property
    def cp_session(self) -> ChiakiPySession:
        """The underlying pybind11 connection: input, low-level events, connection state."""
        return self.__cp_session

    @property
    def frame_handler(self) -> FrameHandler:
        """The handler frames() pulls decoded frames through; matches `frame_handler_cls`."""
        return self.__frame_handler

    @property
    def is_active(self) -> bool:
        """True from start until the session ends for good: stopped, disconnected, or failed to connect."""
        return self.__active

    def __on_session_quit(self, reason: QuitReason) -> None:
        if quit_reason_is_error(reason):
            _logger.error("Session quit: %s", quit_reason_string(reason))
        self.__active = False
        self.__quit_reason = reason
        self.__quit.set()

    def __on_connected_changed(self, connected: bool) -> None:
        if connected:
            self.__connected.set()

    def __enter__(self) -> "Session":
        """Connect and wait until the session is established (see connect()). Raises RuntimeError up
        front if `frame_handler` needs a hardware decoder that `settings` was not set up for, rather
        than failing once frames start arriving."""
        self.connect()
        return self

    def __exit__(self, exc_type: type[BaseException] | None, exc_value: BaseException | None, traceback: TracebackType | None) -> None:
        self.disconnect()

    def connect(self) -> None:
        """Connect to the console and block until the session is established.

        Raises SessionConnectError if the console refuses or the connection fails (see its `reason`),
        and RuntimeError up front if the frame handler needs a hardware decoder that `settings` was not
        set up for; either way the attempt is torn down before raising. There is no timeout. A login PIN
        request does not end the wait: answer it from an on_login_pin_requested() subscription.
        """
        self.__connected.clear()
        self.__quit.clear()
        self.__quit_reason = None
        self.__active = True
        try:
            self.__cp_session.start()
            self.__wait_connected()
        except BaseException:
            self.disconnect()
            raise

    def __wait_connected(self) -> None:
        while not self.__connected.wait(timeout=0.2):
            if self.__quit.is_set():
                reason = self.__quit_reason
                assert reason is not None
                raise SessionConnectError(reason)

    def disconnect(self) -> None:
        """Disconnect, if a connection is up or in progress. Safe to call more than once."""
        self.__active = False
        if self.__cp_session.is_connected() or self.__cp_session.is_connecting():
            self.__cp_session.stop()

    def audio_frames(
        self,
    ) -> Iterator[npt.NDArray[np.int16]]:
        """Yield the queued audio as int16 arrays of shape (frames, channels), each holding every frame
        queued since the last one. The rate and channel count are on `cp_session.get_audio_handler()`.

        Yields nothing unless the session is connected. Don't combine this with an `AudioSink`: both take
        frames off the same queue, so each would only get part of the audio.
        """
        self._iter_audio_frames = AudioFrameEventIterator(
            self.__cp_session.on_audio_frame_available(),
            lambda: self.__cp_session.get_audio_handler().get_frame(),
            lambda: self.__active
        )
        if not self.__connected.is_set():
            return []
        return self._iter_audio_frames()

    def frames(
        self,
        max_fps: float = 60.0,
        out: npt.NDArray[np.uint8] | VulkanFrame | Any | None = None,
    ) -> Iterator[npt.NDArray[np.uint8] | VulkanFrame | Any]:
        """Yield decoded frames, at most `max_fps` per second (0 = every frame). Yields nothing unless
        the session is connected.

        What a frame is depends on the frame handler: a (H, W, 3) uint8 RGB numpy array for
        CpuFrameHandler, a (H, W, 3) uint8 CuPy array for CudaFrameHandler, a VulkanFrame for
        VulkanFrameHandler. By default every frame is a new object. Pass `out` (an array of the
        stream's shape, see the handler's `empty_frame()`) to have every frame written into it
        instead: the same object is then yielded each time and is overwritten by the next frame, so
        copy it if you keep it or hand it to another thread.
        """
        if not self.__connected.is_set():
            return []
        self._iter_frames = FrameEventIterator(
            self.__cp_session.on_frame_available(),
            lambda: self.__frame_handler.get_frame(out),
            lambda: self.__active
        )
        return self._iter_frames(max_fps)
