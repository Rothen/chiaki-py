import time

from PyQt6.QtCore import QThread, pyqtSignal

from .frame_thread import FrameThread

class FpsThread(QThread):
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
    new_fps = pyqtSignal(float, float, float)

    def __init__(self, frame_thread: FrameThread, update_time: float = 0.5, render_async: bool = False):
        super().__init__()
        self.update_time = update_time
        self._frame_count = 0
        self._last_time: float = 0.0
        self._running = False
        self._sleep_time = int(self.update_time * 1000)
        self._frame_thread = frame_thread
        self._frame_thread.new_frame.connect(self._on_frame)
        self._render_async = render_async
        
        self.__tick_render_start: float = 0.0
        self.__tick_total_start: float = 0.0

        self.__total_render_time: float = 0.0
        self.__total_time: float = 0.0
        

    def tick_render(self) -> None:
        self.__tick_render_start = time.perf_counter()

    def tock_render(self) -> None:
        self.__total_render_time += time.perf_counter() - self.__tick_render_start

    def tick_total(self) -> None:
        self.__tick_total_start = time.perf_counter()

    def tock_total(self) -> None:
        self.__total_time += time.perf_counter() - self.__tick_total_start
    
    def _on_frame(self, _: object) -> None:
        self._frame_count += 1

    def run(self) -> None:
        self._running = True
        self._last_time = time.perf_counter()
        
        render_time: float = 0.0
        total_time: float = 0.0
        
        while self._running:
            self.msleep(self._sleep_time)
            dt = time.perf_counter() - self._last_time
            render_time = (self.__total_render_time / self._frame_count if self._frame_count > 0 else 0.0) * 1000
            total_time = (self.__total_time / self._frame_count if self._frame_count > 0 else 0.0) * 1000 + self._render_async * render_time

            self.new_fps.emit(
                self._frame_count / dt,
                render_time,
                total_time
            )

            self._frame_count = 0.0
            self.__total_render_time = 0.0
            self.__total_time = 0.0
            self._last_time = time.perf_counter()

    def stop(self) -> None:
        self._running = False
        self.quit()
        self.wait()