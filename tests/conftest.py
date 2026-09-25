"""Shared fakes for the tests. None of them needs a console, a network or a GPU: the pybind11 types that
would talk to one (ChiakiPySession, DiscoveryManager, Backend) are replaced by the small fakes here."""

from __future__ import annotations

import threading
from typing import Any, Callable

import pytest

from chiaki_py.lib import Target
from chiaki_py.registration import HostRegistration


class FakeSubscription:
    def __init__(self, source: FakeEventSource, callback: Callable[[Any], None]):
        self.source = source
        self.callback = callback

    def unsubscribe(self) -> None:
        self.source.subscribers.remove(self)


class FakeEventSource:
    """Stands in for the pybind11 *EventSource types: subscribe() returns something to unsubscribe()."""

    def __init__(self) -> None:
        self.subscribers: list[FakeSubscription] = []

    def subscribe(self, callback: Callable[[Any], None]) -> FakeSubscription:
        subscription = FakeSubscription(self, callback)
        self.subscribers.append(subscription)
        return subscription

    def emit(self, value: Any) -> None:
        for subscription in list(self.subscribers):
            subscription.callback(value)


class FakeAudioHandler:
    def __init__(self, rate: int = 48000, channels: int = 2):
        self.rate = rate
        self.channels = channels
        self.queue: list = []

    def get_audio_rate(self) -> int:
        return self.rate

    def get_audio_channels(self) -> int:
        return self.channels

    def get_frame(self, max_frames: int = 0):
        import numpy as np

        frames = self.queue[:max_frames] if max_frames else self.queue
        self.queue = self.queue[len(frames):]
        return np.array(frames, dtype=np.int16).reshape(-1, self.channels)


class FakeChiakiPySession:
    """Stands in for ChiakiPySession. `on_start` decides what start() does: by default the session
    connects straight away, like a console that accepts."""

    def __init__(self, connect_info: Any = None, hardware_decoder: str = ""):
        self.connect_info = connect_info
        self.hardware_decoder = hardware_decoder
        self.session_quit = FakeEventSource()
        self.connected_changed = FakeEventSource()
        self.frame_available = FakeEventSource()
        self.audio_frame_available = FakeEventSource()
        self.audio_handler = FakeAudioHandler()
        self.connected = False
        self.connecting = False
        self.start_calls = 0
        self.stop_calls = 0
        self.on_start: Callable[[FakeChiakiPySession], None] = lambda s: s.connect()

    # What a console does
    def connect(self) -> None:
        self.connected = True
        self.connected_changed.emit(True)

    def quit(self, reason) -> None:
        self.connected = self.connecting = False
        self.session_quit.emit(reason)

    # The ChiakiPySession API Session uses
    def start(self) -> None:
        self.start_calls += 1
        self.connecting = True
        self.on_start(self)

    def stop(self) -> None:
        self.stop_calls += 1
        self.connected = self.connecting = False

    def is_connected(self) -> bool:
        return self.connected

    def is_connecting(self) -> bool:
        return self.connecting

    def hardware_decoder_type(self) -> str:
        return self.hardware_decoder

    def has_hardware_decoder(self) -> bool:
        return bool(self.hardware_decoder)

    def on_session_quit(self) -> FakeEventSource:
        return self.session_quit

    def on_connected_changed(self) -> FakeEventSource:
        return self.connected_changed

    def on_frame_available(self) -> FakeEventSource:
        return self.frame_available

    def on_audio_frame_available(self) -> FakeEventSource:
        return self.audio_frame_available

    def get_audio_handler(self) -> FakeAudioHandler:
        return self.audio_handler


def emit_from_thread(delay: float, fn: Callable[[], None]) -> threading.Thread:
    """Run `fn` after `delay` seconds on another thread, like chiaki's own threads deliver events."""
    import time

    def run() -> None:
        time.sleep(delay)
        fn()

    thread = threading.Thread(target=run, daemon=True)
    thread.start()
    return thread


@pytest.fixture
def registration() -> HostRegistration:
    return HostRegistration(
        host="192.168.1.20",
        target=Target.PS5_1,
        regist_key="0123abcd",
        nickname="PS5-083",
        morning=bytes(range(16)),
    )
