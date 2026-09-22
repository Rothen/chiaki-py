from typing import TypeVar, Generic, cast

from PyQt6.QtCore import QThread, pyqtSignal

from chiaki_py import Session

T = TypeVar("T")

class FrameThread(QThread, Generic[T]):
    """Pulls decoded frames off the GUI thread: Session.frames() blocks until the next one is ready and
    does the decoding/converting work, so running it here leaves the GUI thread free to just paint
    whatever it is handed.

    Every frame is a fresh object (Session.frames() with no `out`), never one buffer reused in place and
    shared across threads - the GUI thread might still be reading the previous emit from it when this
    thread would otherwise overwrite it (or, for a VulkanFrame, free it out from under the GUI thread by
    reset()ing it in place). A fresh object costs an allocation - cheap for a numpy/cupy array, and free
    for a VulkanFrame, which only wraps a frame the decoder already produced - and the GUI thread simply
    keeps whichever frame it is using alive by holding a reference to it for as long as it needs it.
    """
    new_frame = pyqtSignal(object)   # the frame, and how long getting/converting it took (seconds)

    def __init__(self, session: Session, max_fps: float = 60.0):
        super().__init__()
        self.session = session
        self.max_fps = max_fps
        self._running = True
        profile = session.stream_session.get_video_profile()
        self.width = profile.width
        self.height = profile.height
        self.size = (self.width, self.height)
        self.frame_init: T = cast(T, session.frame_handler.empty_frame(profile.width, profile.height))

    def run(self) -> None:
        for frame in self.session.frames(max_fps=self.max_fps):
            if not self._running:
                break
            self.new_frame.emit(frame)

    def stop(self) -> None:
        self._running = False
        self.quit()
        self.wait()
