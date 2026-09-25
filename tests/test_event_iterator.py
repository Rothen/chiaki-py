import itertools
import time
from types import SimpleNamespace

import pytest

from chiaki_py.event_iterator import FrameEventIterator
from conftest import FakeEventSource, emit_from_thread


def always_active():
    return True


def test_yields_a_pulled_frame_per_event():
    source = FakeEventSource()
    counter = itertools.count()
    frames = FrameEventIterator(source, lambda: next(counter), always_active)(0)

    emit_from_thread(0.01, lambda: source.emit(True))
    assert next(frames) == 0
    emit_from_thread(0.01, lambda: source.emit(True))
    assert next(frames) == 1


def test_skips_none_and_unreadable_frames():
    source = FakeEventSource()
    results = iter([None, RuntimeError("corrupt"), "frame"])

    def pull():
        result = next(results)
        if isinstance(result, Exception):
            raise result
        return result

    frames = FrameEventIterator(source, pull, always_active)(0)

    def emit_three():
        for _ in range(3):
            source.emit(True)
            time.sleep(0.02)

    emit_from_thread(0.01, emit_three)
    assert next(frames) == "frame"


def test_other_exceptions_propagate():
    source = FakeEventSource()

    def pull():
        raise ValueError("bug")

    frames = FrameEventIterator(source, pull, always_active)(0)
    emit_from_thread(0.01, lambda: source.emit(True))

    with pytest.raises(ValueError):
        next(frames)


def test_ends_once_inactive_even_without_events():
    source = FakeEventSource()
    active = [True]
    frames = FrameEventIterator(source, lambda: "frame", lambda: active[0])(0)

    emit_from_thread(0.05, lambda: active.__setitem__(0, False))
    start = time.perf_counter()

    assert list(frames) == []
    assert time.perf_counter() - start < 2.0


def test_does_not_start_when_already_inactive():
    frames = FrameEventIterator(FakeEventSource(), lambda: "frame", lambda: False)(0)

    assert list(frames) == []


def test_unsubscribes_when_the_loop_is_left():
    source = FakeEventSource()
    frames = FrameEventIterator(source, lambda: "frame", always_active)(0)

    emit_from_thread(0.01, lambda: source.emit(True))
    next(frames)
    assert len(source.subscribers) == 1

    frames.close()  # what `break` out of a for loop does
    assert source.subscribers == []


def test_unsubscribes_when_it_ends_by_itself():
    source = FakeEventSource()
    frames = FrameEventIterator(source, lambda: "frame", lambda: False)(0)

    list(frames)

    assert source.subscribers == []


def fake_clock_stream(monkeypatch, event_interval: float, duration: float, max_fps: float) -> list[float]:
    """The times at which frames are yielded, for events arriving every `event_interval` seconds of a fake
    clock. Each pull advances the clock and delivers the next event, so no threads or sleeps are involved."""
    import chiaki_py.event_iterator as event_iterator_module

    clock = [0.0]
    monkeypatch.setattr(event_iterator_module, "time", SimpleNamespace(perf_counter=lambda: clock[0]))

    class FrameWaitingSource(FakeEventSource):
        def subscribe(self, callback):
            subscription = super().subscribe(callback)
            callback(True)  # a frame is already waiting (the iterator only subscribes once iterated)
            return subscription

    source = FrameWaitingSource()

    def pull():
        clock[0] += event_interval
        source.emit(True)  # the next frame is ready right away
        return clock[0]

    frames = FrameEventIterator(source, pull, always_active)(max_fps)
    times = []
    for t in frames:
        if t > duration:
            break
        times.append(t)
    return times


def test_max_fps_limits_the_rate(monkeypatch):
    times = fake_clock_stream(monkeypatch, event_interval=0.01, duration=2.0, max_fps=10)

    # 100 frames/s arrive, about 10/s get through (one extra is allowed to catch up at the start)
    assert 19 <= len(times) <= 22


def test_max_fps_zero_yields_every_frame(monkeypatch):
    times = fake_clock_stream(monkeypatch, event_interval=0.01, duration=1.0, max_fps=0)

    assert len(times) == pytest.approx(100, abs=1)


def test_max_fps_above_the_frame_rate_drops_nothing(monkeypatch):
    times = fake_clock_stream(monkeypatch, event_interval=1 / 30, duration=1.0, max_fps=60)

    assert len(times) == pytest.approx(30, abs=1)
