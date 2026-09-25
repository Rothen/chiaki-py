from __future__ import annotations

import logging
import threading
import time
from typing import Generic, Callable, Iterator, TypeVar, Any, TypedDict
from typing_extensions import Unpack

import numpy as np
import numpy.typing as npt

from .lib import BoolEventSource, VulkanFrame

_logger = logging.getLogger(__name__)

_T = TypeVar("_T")


class _EventIterator(Generic[_T]):
    def __init__(self, event_source: BoolEventSource, pull_fn: Callable[[], _T | None]):
        """`is_active` is polled at least every 0.5s; iteration ends once it returns False (e.g. the session
        quit or failed to connect), instead of waiting forever for an event that will never come."""
        self.__event_source = event_source
        self.__pull_fn = pull_fn
        self.__active = False

    def __call__(self, max_fps: float = 0.0) -> Iterator[_T]:
        self.__active = True
        return self._iter(self.__pull_fn, max_fps)

    def _iter(self, pull: Callable[[], _T | None], max_fps: float) -> Iterator[_T]:
        min_interval = (1.0 / max_fps) if max_fps > 0 else 0.0
        ready = threading.Event()
        subscription = self.__event_source.subscribe(lambda _: ready.set())
        try:
            next_yield = 0.0
            while self.__active:
                if not ready.wait(timeout=0.5):
                    continue
                ready.clear()
                try:
                    frame = pull()
                except RuntimeError:
                    # _logger.warning("Dropping unreadable frame", exc_info=True)
                    continue
                if frame is None:
                    continue
                now = time.perf_counter()
                if now < next_yield:
                    continue
                next_yield = max(next_yield + min_interval, now - min_interval)
                yield frame
        finally:
            subscription.unsubscribe()
    

class FrameEventIterator(_EventIterator[npt.NDArray[np.uint8] | VulkanFrame | Any]):
    pass


class AudioFrameEventIterator(_EventIterator[npt.NDArray[np.int16]]):
    pass
