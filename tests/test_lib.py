"""Tests of the compiled pybind11 module: the parts that work without a console."""

import ctypes
import logging
import sys
from pathlib import Path

import numpy as np
import pytest

import chiaki_py.lib as lib
from chiaki_py.lib import CpuFrameHandler, LogLevel, QuitReason, Settings, Target, quit_reason_is_error, quit_reason_string


class TestSettings:
    def test_hardware_decoder_round_trips(self):
        settings = Settings()
        settings.set_hardware_decoder("cuda")

        assert settings.get_hardware_decoder() == "cuda"

    def test_log_level_round_trips(self):
        settings = Settings()
        settings.set_log_level(LogLevel.WARNING)

        assert settings.get_log_level() == LogLevel.WARNING

    def test_log_verbose_round_trips(self):
        settings = Settings()
        settings.set_log_verbose(True)

        assert settings.get_log_verbose() is True

    def test_audio_buffer_size_zero_means_the_default(self):
        settings = Settings()
        settings.set_audio_buffer_size(0)

        assert settings.get_audio_buffer_size() == settings.get_audio_buffer_size_default() > 0

    def test_instances_are_independent(self):
        first, second = Settings(), Settings()
        first.set_hardware_decoder("cuda")

        assert second.get_hardware_decoder() == "vulkan"  # the default


class TestQuitReason:
    def test_stopped_is_not_an_error(self):
        assert not quit_reason_is_error(QuitReason.Stopped)

    def test_rp_in_use_is_an_error(self):
        assert quit_reason_is_error(QuitReason.SessionRequestRpInUse)

    @pytest.mark.parametrize("reason", list(QuitReason.__members__.values()))
    def test_every_reason_has_a_description(self, reason):
        assert quit_reason_string(reason)


def test_target_names():
    assert {"PS4_8", "PS4_9", "PS4_10", "PS5_1"} <= set(Target.__members__)


class TestCpuEmptyFrame:
    def test_shape_and_dtype(self):
        frame = CpuFrameHandler.empty_frame(1920, 1080)

        assert frame.shape == (1080, 1920, 3)
        assert frame.dtype == np.uint8
        assert frame.flags.c_contiguous

    def test_frames_are_separate_arrays(self):
        first = CpuFrameHandler.empty_frame(4, 2)
        second = CpuFrameHandler.empty_frame(4, 2)
        first[:] = 1

        assert not np.shares_memory(first, second)


def _av_log():
    """FFmpeg's av_log from the avutil the extension loaded, or None where it can't be found by name."""
    lib_dir = Path(lib.__file__).parent
    candidates = sorted(lib_dir.glob("avutil*.dll")) if sys.platform == "win32" else sorted(lib_dir.glob("libavutil*"))
    for candidate in candidates:
        try:
            return ctypes.CDLL(str(candidate)).av_log
        except OSError:
            continue
    return None


AV_LOG_ERROR, AV_LOG_WARNING, AV_LOG_DEBUG = 16, 24, 48


@pytest.mark.skipif(_av_log() is None, reason="FFmpeg's avutil is not bundled next to the extension")
class TestFfmpegLogging:
    @pytest.fixture
    def av_log(self):
        return _av_log()

    def test_messages_reach_the_ffmpeg_logger(self, av_log, caplog):
        with caplog.at_level(logging.DEBUG, logger="chiaki_py.lib.ffmpeg"):
            av_log(None, AV_LOG_ERROR, b"Could not find ref with POC %d\n", ctypes.c_int(39))

        record, = [r for r in caplog.records if r.name == "chiaki_py.lib.ffmpeg"]
        assert record.levelno == logging.ERROR
        assert record.getMessage() == "Could not find ref with POC 39"

    def test_a_line_sent_in_pieces_is_logged_once(self, av_log, caplog):
        with caplog.at_level(logging.DEBUG, logger="chiaki_py.lib.ffmpeg"):
            av_log(None, AV_LOG_WARNING, b"first half, ")
            av_log(None, AV_LOG_WARNING, b"second half\n")

        messages = [r.getMessage() for r in caplog.records if r.name == "chiaki_py.lib.ffmpeg"]
        assert messages == ["first half, second half"]

    def test_messages_below_ffmpegs_level_are_dropped(self, av_log, caplog):
        with caplog.at_level(logging.DEBUG, logger="chiaki_py.lib.ffmpeg"):
            av_log(None, AV_LOG_DEBUG, b"too detailed\n")

        assert not [r for r in caplog.records if r.name == "chiaki_py.lib.ffmpeg"]

    def test_the_logger_level_silences_it(self, av_log, caplog):
        logger = logging.getLogger("chiaki_py.lib.ffmpeg")
        with caplog.at_level(logging.CRITICAL, logger="chiaki_py.lib.ffmpeg"):
            av_log(None, AV_LOG_ERROR, b"hidden\n")

        assert not [r for r in caplog.records if r.name == "chiaki_py.lib.ffmpeg"]
        assert logger.name == "chiaki_py.lib.ffmpeg"
