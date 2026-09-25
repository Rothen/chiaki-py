import threading

import sounddevice as sd

from chiaki_py import Session


class AudioSink(threading.Thread):
    """Plays the session's audio on the default output device.

    Waits for the first audio frame (the rate/channel count are only known once audio arrives), then
    opens a sounddevice OutputStream whose callback pulls exactly as many frames as the device asks for
    straight from the AudioHandler queue, padding with silence on underrun. The callback is the queue's
    only consumer - pulling frames anywhere else as well would steal audio from it.

    The stream is opened and closed on this thread, so stop() never races the stream's setup. The thread
    ends by itself once the session quits (including when it never connects), and is a daemon, so a
    missing stop() can never keep the process from exiting.
    """

    def __init__(self, session: Session, auto: bool = True):
        super().__init__(daemon=True)
        self.session = session
        self._audio_handler = session.cp_session.get_audio_handler()
        self._stop_event = threading.Event()
        self._audio_ready = threading.Event()
        # Never unsubscribed: it only sets an Event, and unsubscribing while the quit event is being
        # delivered could deadlock on the EventSource's lock.
        self._quit_subscription = session.cp_session.on_session_quit().subscribe(
            lambda _: self._stop_event.set()
        )
        if auto:
            self.start()

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

        with sd.OutputStream(
            samplerate=self._audio_handler.get_audio_rate(),
            channels=self._audio_handler.get_audio_channels(),
            dtype='int16',
            callback=self._callback,
        ):
            while not self._stop_event.wait(timeout=0.5):
                if not self.session.is_active:
                    break

    def _callback(self, out, frames, t, status) -> None:
        pcm = self._audio_handler.get_frame(frames)
        n = len(pcm)
        if n:
            out[:n] = pcm
        out[n:] = 0

    def stop(self) -> None:
        self._stop_event.set()
        if self.is_alive():
            self.join()
