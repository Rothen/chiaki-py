import logging

from PyQt6.QtCore import QCoreApplication, QObject, QThread, pyqtSignal
from PyQt6.QtWidgets import QApplication
from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers

from chiaki_py import Session
from chiaki_py.controller import attach_controller
from chiaki_py.gui.stream.aspect_ratio import AspectRatioLock
from chiaki_py.lib import CpuFrameHandler, CudaFrameHandler, VulkanFrameHandler
from chiaki_py.gui.stream.views.base_view import BaseView, VideoMixin
from .threads.frame_thread import FrameThread
from ...audio_sink import AudioSink
from .views.cpu_view import CpuVideoWidget
from .views.cuda_view import CudaVideoWidget
from .views.vulkan_view import VulkanVideoWidget
from chiaki_py.gui.stream.views.cpu_view import CpuVideoWidget
from chiaki_py.gui.stream.views.cuda_view import CudaVideoWidget
from chiaki_py.gui.stream.views.vulkan_view import VulkanVideoWidget
from .threads.fps_thread import FpsThread

_logger = logging.getLogger(__name__)


class ControllerThread(QThread):
    """Attaches the first available DualSense controller to the session's input, off the GUI
    thread (dualsense-py's SDL3 backend needs its own event loop). Does nothing but log if no
    controller is found; `stop()` releases the sticks and sends a final feedback state so the
    console doesn't see them stuck wherever they last were."""

    def __init__(self, session: Session):
        super().__init__()
        self.session = session

    def run(self) -> None:
        SDL3Backend.init()
        available_controllers = get_available_controllers()
        if not available_controllers:
            _logger.info("No DualSense controllers found.")
            return
        attach_controller(available_controllers[0], self.session.cp_session)

    def stop(self) -> None:
        self.quit()
        self.wait()
        self.session.cp_session.release_right()
        self.session.cp_session.release_left()
        self.session.cp_session.send_feedback_state()


class StreamDisplay(QObject):
    """Shows the session's frames in a window, scaled to fit it as it is resized.

    How the frames are rendered follows the session's frame handler:
      - CpuFrameHandler: frames are decoded to system memory and shown by a plain QMainWindow
        (views/cpu_view.py).
      - CudaFrameHandler: frames are converted on the GPU and drawn straight from GPU
        memory by an OpenGL widget (views/cuda_view.py), never touching the CPU. This needs an
        NVIDIA GPU, a QApplication, and pip install cupy-cuda12x cuda-python PyOpenGL.
      - VulkanFrameHandler: frames stay where the Vulkan hardware decoder put them and are drawn
        on that same Vulkan device by libplacebo, which converts them to RGB (views/vulkan_view.py),
        so they are not copied at all. Needs Settings.set_hardware_decoder("vulkan") and a
        QApplication; works on any GPU with Vulkan video decoding, Windows only so far.
    Other handlers cannot be shown.

    Press F to show or hide the frame rate and the time needed per frame in the top-right
    corner (with a VulkanFrameHandler libplacebo draws it over the video, as Vulkan draws over
    anything Qt puts on the window); `show_stats` says whether they start out shown. The time per frame is how
    long it takes to get a frame plus how long it takes to paint it, averaged over the last
    half second, e.g. "2.0 ms/frame (get 1.5 + paint 0.5)":
      - get: the frame handler fetching the frame, i.e. decoding it to system memory
        (CPU), converting it to RGB on the GPU (CUDA) or just taking hold of it (GPU).
      - paint: on the CPU path, preparing the frame and handing it to Qt (the drawing
        itself is done by Qt's render thread and not included); on the CUDA path, copying
        it into the texture and drawing it, up to the GPU having finished; with a
        VulkanFrameHandler, recording and submitting the drawing, not the GPU doing it.

    With `keep_aspect_ratio` (the default) the window keeps the video's aspect ratio while it is
    resized by dragging, so there are no bars; without it the window can take any shape and the
    picture is letterboxed. Only on Windows so far; elsewhere the window is never constrained.
    """

    def __init__(self, session: Session, show_stats: bool = False, keep_aspect_ratio: bool = False):
        super().__init__()
        self.session = session
        self.aspect_lock: AspectRatioLock | None = None
        self.frame_thread = FrameThread(session)
        self.audio_sink = AudioSink(session)
        self.fps_thread = FpsThread(self.frame_thread, render_async=isinstance(self.session.frame_handler, (CudaFrameHandler, VulkanFrameHandler)))
        self.controller_thread = ControllerThread(session)
 
        try:
            self.window = BaseView(self.__init_video_mixin(), self.frame_thread, self.fps_thread,
                                    show_stats, keep_aspect_ratio)
            self.window.closeRequested.connect(self.close)  # type: ignore[attr-defined]
            self.window.show()
        except Exception:
            _logger.exception("Failed to create the stream window")
            raise

        self.frame_thread.finished.connect(self.__on_frames_ended)
        self.frame_thread.start()
        self.fps_thread.start()
        self.controller_thread.start()

    def __init_video_mixin(self) -> VideoMixin:
        if isinstance(self.session.frame_handler, CpuFrameHandler):
            return CpuVideoWidget(self.frame_thread, self.fps_thread)
        elif isinstance(self.session.frame_handler, CudaFrameHandler):
            return CudaVideoWidget(self.frame_thread, self.fps_thread)
        elif isinstance(self.session.frame_handler, VulkanFrameHandler):
            return VulkanVideoWidget(self.session, self.frame_thread, self.fps_thread)
        else:
            raise TypeError(f"StreamDisplay can't show frames from a {type(self.session.frame_handler).__name__}: use a CpuFrameHandler (rendered on the CPU), a CudaFrameHandler (rendered on the GPU with OpenGL) or a VulkanFrameHandler (rendered on the GPU with Vulkan)")


    def __on_frames_ended(self) -> None:
        if self.window.isVisible():
            _logger.info("Session ended, closing the stream window")
            self.window.close()

    def close(self) -> None:
        if self.aspect_lock is not None:
            self.aspect_lock.remove()
        self.controller_thread.stop()
        self.frame_thread.stop()
        self.audio_sink.stop()
        self.session.disconnect()

    @classmethod
    def start(cls, session: Session, argv: list[str], show_stats: bool = False, keep_aspect_ratio: bool = False):
        """Open a StreamDisplay for `session` and run the Qt event loop until its window is closed.
        Reuses an existing QApplication if one is already running, otherwise creates one from `argv`.
        Returns the process exit code from `QApplication.exec()`."""
        app = QCoreApplication.instance() or QApplication(argv)
        _ = StreamDisplay(session, show_stats, keep_aspect_ratio)
        return app.exec()
