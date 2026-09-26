from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers
from dualsense_py import DualSenseController
import miniaudio

from chiaki_py import Session
from chiaki_py.controller import attach_controller, ControllerSubscriptions
from chiaki_py.lib import ChiakiPySession


def setup_controller(stream_session: ChiakiPySession) -> tuple[DualSenseController | None, ControllerSubscriptions | None]:
    SDL3Backend.init()
    controllers = get_available_controllers()
    if not controllers:
        print("No DualSense controllers found - streaming without input.")
        return None, None
    return controllers[0], attach_controller(controllers[0], stream_session)


def audio_stream(audio_handler, channels: int):
    """miniaudio's playback generator: each `yield` receives how many frames the device needs and
    hands back that many as interleaved int16 bytes, padded with silence if the queue runs short."""
    required_frames = yield b""
    while True:
        pcm = audio_handler.get_frame(required_frames)  # int16, shape (frames, channels)
        silence = bytes((required_frames - len(pcm)) * channels * 2)
        required_frames = yield pcm.tobytes() + silence


def start_audio(session: Session) -> miniaudio.PlaybackDevice | None:
    """Open the default output device, or return None while no audio has arrived yet: the rate and
    channel count are only known from the first audio frame on."""
    audio_handler = session.cp_session.get_audio_handler()
    rate, channels = audio_handler.get_audio_rate(), audio_handler.get_audio_channels()
    if not rate or not channels:
        return None
    device = miniaudio.PlaybackDevice(output_format=miniaudio.SampleFormat.SIGNED16, nchannels=channels, sample_rate=rate, buffersize_msec=20)
    stream = audio_stream(audio_handler, channels)
    next(stream)
    device.start(stream)
    return device
