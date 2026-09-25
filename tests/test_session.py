import time

import pytest

import chiaki_py.session as session_module
from chiaki_py import Session, SessionConnectError
from chiaki_py.lib import QuitReason, Settings
from conftest import FakeChiakiPySession, emit_from_thread


class FakeCpuFrameHandler:
    def __init__(self, cp_session):
        self.cp_session = cp_session
        self.outs = []
        self.next_frame = "frame"

    def get_frame(self, out=None):
        self.outs.append(out)
        return self.next_frame


class FakeCudaFrameHandler(FakeCpuFrameHandler):
    pass


class FakeVulkanFrameHandler(FakeCpuFrameHandler):
    pass


@pytest.fixture
def fakes(monkeypatch):
    """Session built on fakes; `fakes.cp_session` is the fake ChiakiPySession of the last Session made."""

    class Fakes:
        cp_session: FakeChiakiPySession
        hardware_decoder = ""
        on_start = None

    def make_cp_session(connect_info):
        cp_session = FakeChiakiPySession(connect_info, hardware_decoder=Fakes.hardware_decoder)
        if Fakes.on_start is not None:
            cp_session.on_start = Fakes.on_start
        Fakes.cp_session = cp_session
        return cp_session

    monkeypatch.setattr(session_module, "ChiakiPySessionConnectInfo", lambda **kwargs: kwargs)
    monkeypatch.setattr(session_module, "ChiakiPySession", make_cp_session)
    monkeypatch.setattr(session_module, "CudaFrameHandler", FakeCudaFrameHandler)
    monkeypatch.setattr(session_module, "VulkanFrameHandler", FakeVulkanFrameHandler)
    return Fakes


def make_session(registration, handler=FakeCpuFrameHandler) -> Session:
    return Session(Settings(), registration, handler)


def test_connect_info_comes_from_the_registration(fakes, registration):
    settings = Settings()
    Session(settings, registration, FakeCpuFrameHandler)

    info = fakes.cp_session.connect_info
    assert info["settings"] is settings
    assert info["host"] == registration.host
    assert info["target"] == registration.target
    assert info["regist_key"] == registration.regist_key
    assert info["morning"] == registration.morning
    assert info["nickname"] == registration.nickname


def test_frame_handler_is_built_on_the_session(fakes, registration):
    session = make_session(registration)

    assert isinstance(session.frame_handler, FakeCpuFrameHandler)
    assert session.frame_handler.cp_session is fakes.cp_session
    assert session.cp_session is fakes.cp_session


def test_not_active_before_connecting(fakes, registration):
    assert not make_session(registration).is_active


def test_connect_waits_until_connected(fakes, registration):
    fakes.on_start = lambda cp: emit_from_thread(0.05, cp.connect)
    session = make_session(registration)

    session.connect()

    assert fakes.cp_session.connected
    assert session.is_active


def test_connect_raises_when_the_console_refuses(fakes, registration):
    fakes.on_start = lambda cp: emit_from_thread(0.05, lambda: cp.quit(QuitReason.SessionRequestRpInUse))
    session = make_session(registration)

    with pytest.raises(SessionConnectError) as info:
        session.connect()

    assert info.value.reason == QuitReason.SessionRequestRpInUse
    assert not session.is_active


def test_cuda_handler_needs_the_cuda_decoder(fakes, registration):
    fakes.hardware_decoder = "vulkan"
    session = make_session(registration, FakeCudaFrameHandler)

    with pytest.raises(RuntimeError, match="cuda"):
        session.connect()
    assert fakes.cp_session.start_calls == 0


def test_cuda_handler_connects_with_the_cuda_decoder(fakes, registration):
    fakes.hardware_decoder = "cuda"
    session = make_session(registration, FakeCudaFrameHandler)

    session.connect()

    assert session.is_active


def test_vulkan_handler_needs_a_hardware_decoder(fakes, registration):
    session = make_session(registration, FakeVulkanFrameHandler)

    with pytest.raises(RuntimeError, match="hardware decoder"):
        session.connect()
    assert fakes.cp_session.start_calls == 0


def test_context_manager_connects_and_disconnects(fakes, registration):
    with make_session(registration) as session:
        assert session.is_active
        cp_session = fakes.cp_session

    assert not session.is_active
    assert cp_session.stop_calls == 1


def test_context_manager_disconnects_on_error(fakes, registration):
    with pytest.raises(KeyError):
        with make_session(registration):
            raise KeyError

    assert fakes.cp_session.stop_calls == 1


def test_disconnect_is_safe_to_repeat(fakes, registration):
    session = make_session(registration)
    session.connect()

    session.disconnect()
    session.disconnect()

    assert fakes.cp_session.stop_calls == 1


def test_disconnect_before_connecting_does_nothing(fakes, registration):
    make_session(registration).disconnect()

    assert fakes.cp_session.stop_calls == 0


def test_session_quit_makes_it_inactive(fakes, registration):
    session = make_session(registration)
    session.connect()

    fakes.cp_session.quit(QuitReason.Stopped)

    assert not session.is_active


def test_can_reconnect_after_disconnecting(fakes, registration):
    session = make_session(registration)
    session.connect()
    session.disconnect()

    session.connect()

    assert session.is_active
    assert fakes.cp_session.start_calls == 2


class TestFrames:
    def test_yields_nothing_before_connecting(self, fakes, registration):
        assert list(make_session(registration).frames()) == []

    def test_yields_the_handlers_frames_and_passes_out(self, fakes, registration):
        session = make_session(registration)
        session.connect()
        out = object()

        frames = session.frames(max_fps=0, out=out)
        emit_from_thread(0.01, lambda: fakes.cp_session.frame_available.emit(True))

        assert next(frames) == "frame"
        assert session.frame_handler.outs == [out]

    def test_ends_when_the_session_quits(self, fakes, registration):
        session = make_session(registration)
        session.connect()

        emit_from_thread(0.05, lambda: fakes.cp_session.quit(QuitReason.Stopped))
        start = time.perf_counter()

        assert list(session.frames(max_fps=0)) == []
        assert time.perf_counter() - start < 2.0

    def test_ends_when_disconnected(self, fakes, registration):
        session = make_session(registration)
        session.connect()

        emit_from_thread(0.05, session.disconnect)

        assert list(session.frames(max_fps=0)) == []


class TestAudioFrames:
    def test_yields_nothing_before_connecting(self, fakes, registration):
        assert list(make_session(registration).audio_frames()) == []

    def test_yields_queued_audio(self, fakes, registration):
        session = make_session(registration)
        session.connect()
        fakes.cp_session.audio_handler.queue = [[1, 2], [3, 4]]

        frames = session.audio_frames()
        emit_from_thread(0.01, lambda: fakes.cp_session.audio_frame_available.emit(True))

        assert next(frames).tolist() == [[1, 2], [3, 4]]
