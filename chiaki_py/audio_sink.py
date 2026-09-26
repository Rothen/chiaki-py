import threading

from chiaki_py import Session
from chiaki_py.lib import AudioOutput


class AudioSink(threading.Thread):
    """Plays the session's audio on an output device (the system default unless `device` names one of
    `AudioSink.devices()`).

    Waits for the first audio frame (the rate/channel count are only known once audio arrives), then
    opens an SDL audio device (`chiaki_py.lib.AudioOutput`) whose audio thread pulls exactly as many
    frames as the device asks for straight from the AudioHandler queue, in C++ and without the GIL,
    padding with silence on underrun. It is the queue's only consumer - pulling frames anywhere else as
    well would steal audio from it.

    The device is opened and closed on this thread, so stop() never races its setup. The thread ends by
    itself once the session quits (including when it never connects), and is a daemon, so a missing
    stop() can never keep the process from exiting.
    """

    def __init__(self, session: Session, auto: bool = True, device: str = ""):
        super().__init__(daemon=True)
        self.session = session
        self.device = device
        self._output = AudioOutput(session.cp_session)
        self._stop_event = threading.Event()
        self._audio_ready = threading.Event()
        self._quit_subscription = session.cp_session.on_session_quit().subscribe(
            lambda _: self._stop_event.set()
        )
        if auto:
            self.start()

    @staticmethod
    def devices() -> list[str]:
        """Names of the available audio output devices, for the `device` argument."""
        return AudioOutput.get_devices()

    def run(self) -> None:
        subscription = self.session.cp_session.on_audio_frame_available().subscribe(
            lambda _: self._audio_ready.set()
        )
        try:
            while not self._audio_ready.wait(timeout=0.5):
                if self._stop_event.is_set():
                    return
        finally:
            subscription.unsubscribe()

        self._output.open(self.device)
        try:
            while not self._stop_event.wait(timeout=0.5):
                if not self.session.is_active:
                    break
        finally:
            self._output.close()

    def stop(self) -> None:
        self._stop_event.set()
        if self.is_alive():
            self.join()
