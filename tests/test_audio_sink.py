import threading

import numpy as np
import pytest

import chiaki_py.audio_sink as audio_sink_module
from chiaki_py import AudioSink
from chiaki_py.lib import QuitReason
from conftest import FakeChiakiPySession


class FakeSession:
    def __init__(self):
        self.cp_session = FakeChiakiPySession()
        self.is_active = True


class FakeOutputStream:
    """Records how the stream was opened, instead of opening a sound device."""

    opened: list["FakeOutputStream"] = []

    def __init__(self, samplerate, channels, dtype, callback):
        self.samplerate = samplerate
        self.channels = channels
        self.dtype = dtype
        self.callback = callback
        self.entered = threading.Event()
        self.exited = threading.Event()
        FakeOutputStream.opened.append(self)

    def __enter__(self):
        self.entered.set()
        return self

    def __exit__(self, *exc):
        self.exited.set()


@pytest.fixture(autouse=True)
def fake_sounddevice(monkeypatch):
    FakeOutputStream.opened = []
    monkeypatch.setattr(audio_sink_module.sd, "OutputStream", FakeOutputStream)


def test_opens_the_stream_once_audio_arrives():
    session = FakeSession()
    session.cp_session.audio_handler.rate = 44100
    session.cp_session.audio_handler.channels = 2
    sink = AudioSink(session)
    try:
        assert FakeOutputStream.opened == []

        session.cp_session.audio_frame_available.emit(True)
        stream = wait_for_stream()

        assert (stream.samplerate, stream.channels, stream.dtype) == (44100, 2, "int16")
    finally:
        sink.stop()
    assert stream.exited.is_set()


def test_ends_by_itself_when_the_session_quits_before_audio():
    session = FakeSession()
    sink = AudioSink(session)

    session.cp_session.quit(QuitReason.Stopped)
    sink.join(timeout=2)

    assert not sink.is_alive()
    assert FakeOutputStream.opened == []


def test_ends_by_itself_when_the_session_quits_while_playing():
    session = FakeSession()
    sink = AudioSink(session)
    session.cp_session.audio_frame_available.emit(True)
    stream = wait_for_stream()

    session.cp_session.quit(QuitReason.Stopped)
    sink.join(timeout=2)

    assert not sink.is_alive()
    assert stream.exited.is_set()


def test_stop_without_audio_ends_the_thread():
    sink = AudioSink(FakeSession())

    sink.stop()

    assert not sink.is_alive()


def test_auto_false_does_not_start():
    sink = AudioSink(FakeSession(), auto=False)

    assert not sink.is_alive()
    sink.stop()  # safe on a thread that never started


def test_unsubscribes_from_audio_events_once_playing():
    session = FakeSession()
    sink = AudioSink(session)
    try:
        session.cp_session.audio_frame_available.emit(True)
        wait_for_stream()

        assert session.cp_session.audio_frame_available.subscribers == []
    finally:
        sink.stop()


class TestCallback:
    def call(self, queue, frames):
        session = FakeSession()
        session.cp_session.audio_handler.queue = queue
        sink = AudioSink(session, auto=False)
        out = np.full((frames, 2), 99, dtype=np.int16)
        sink._callback(out, frames, None, None)
        return out, session.cp_session.audio_handler.queue

    def test_copies_queued_audio(self):
        out, left = self.call([[1, 2], [3, 4], [5, 6]], frames=2)

        assert out.tolist() == [[1, 2], [3, 4]]
        assert left == [[5, 6]]

    def test_pads_with_silence_on_underrun(self):
        out, _ = self.call([[1, 2]], frames=3)

        assert out.tolist() == [[1, 2], [0, 0], [0, 0]]

    def test_silence_when_nothing_is_queued(self):
        out, _ = self.call([], frames=2)

        assert out.tolist() == [[0, 0], [0, 0]]


def wait_for_stream() -> FakeOutputStream:
    for _ in range(200):
        if FakeOutputStream.opened:
            stream = FakeOutputStream.opened[0]
            assert stream.entered.wait(timeout=2)
            return stream
        threading.Event().wait(0.01)
    raise AssertionError("the output stream was never opened")
