from typing import Any

import numpy as np
import cupy as cp
import numpy.typing as npt
from PyQt6.QtCore import QCoreApplication, QObject, QThread, pyqtSignal
from PyQt6.QtWidgets import QApplication
from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers

from chiaki_py import Session
from chiaki_py.controller import attach_controller
from chiaki_py.gui.stream.aspect_ratio import AspectRatioLock
from chiaki_py.lib import CpuFrameHandler, CudaFrameHandler, VulkanFrameHandler, VulkanFrame
from chiaki_py.gui.stream.views.base_view import BaseView, VideoMixin
from chiaki_py.gui.stream.views.cuda_view import _CupyArray


class FrameProducer(QThread):
    """Bridges Session.frames() (a plain generator) into a Qt signal, along with how long getting the frame took."""
    new_frame = pyqtSignal(np.ndarray)

    def __init__(self, session: Session, max_fps: float = 60.0):
        super().__init__()
        self.session = session
        self.max_fps = max_fps
        self._running = True

    def run(self) -> None:
        vw = self.session.stream_session.get_video_profile()

        frame: Any | npt.NDArray[np.uint8] | VulkanFrame | None = self.session.frame_handler.empty_frame(vw.height, vw.width)

        for _ in self.session.frames(max_fps=self.max_fps, out=frame):
            if not self._running:
                break
            self.new_frame.emit(frame)

    def stop(self) -> None:
        self._running = False
        self.quit()
        self.wait()


class ControllerThread(QThread):
    def __init__(self, session: Session):
        super().__init__()
        self.session = session

    def run(self) -> None:
        SDL3Backend.init()
        available_controllers = get_available_controllers()
        if not available_controllers:
            print("No DualSense controllers found.")
            return
        attach_controller(
            available_controllers[0], self.session.stream_session)

    def stop(self) -> None:
        self.quit()
        self.wait()
        self.session.stream_session.release_right()
        self.session.stream_session.release_left()
        self.session.stream_session.send_feedback_state()


class StreamDisplay(QObject):
    """Shows the session's frames in a window, scaled to fit it as it is resized.

    How the frames are rendered follows the session's frame handler:
      - CpuFrameHandler: frames are decoded to system memory and shown by a plain QMainWindow
        (cpu_view.py).
      - CudaFrameHandler: frames are converted on the GPU and drawn straight from GPU
        memory by an OpenGL widget (gpu_view.py), never touching the CPU. This needs an
        NVIDIA GPU, a QApplication, and pip install cupy-cuda12x cuda-python PyOpenGL.
      - VulkanFrameHandler: frames stay where the Vulkan hardware decoder put them and are drawn
        on that same Vulkan device by libplacebo, which converts them to RGB (vulkan_view.py),
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
        self.frame_thread: FrameProducer | None = None
        self.aspect_lock: AspectRatioLock | None = None

        handler = session.frame_handler
        if isinstance(handler, CudaFrameHandler):
            VideoMixinClass = self._init_cuda()
        elif isinstance(handler, VulkanFrameHandler):
            VideoMixinClass = self._init_vulkan()
        elif isinstance(handler, CpuFrameHandler):
            VideoMixinClass = self._init_cpu()
        else:
            raise TypeError(f"StreamDisplay can't show frames from a {type(handler).__name__}: use a CpuFrameHandler "
                            "(rendered on the CPU), a CudaFrameHandler (rendered on the GPU with OpenGL) "
                            "or a VulkanFrameHandler (rendered on the GPU with Vulkan)")
            
        try:
            self.window = BaseView(self.session, VideoMixinClass, show_stats, keep_aspect_ratio)
            self.window.closeRequested.connect(self.close)  # type: ignore[attr-defined]
            self.window.show()
        except Exception as e:
            print(e)
            raise e

        self.controller_thread = ControllerThread(session)
        self.controller_thread.start()

    def _init_cpu(self) -> type[VideoMixin]:
        from chiaki_py.gui.stream.views.cpu_view import CpuVideoWidget

        return CpuVideoWidget

    def _init_cuda(self) -> type[VideoMixin]:
        if not isinstance(QCoreApplication.instance(), QApplication):
            raise RuntimeError("Rendering on the GPU needs a QApplication (a QGuiApplication is not enough) to exist first; StreamDisplay.start() creates one")

        from chiaki_py.gui.stream.views.cuda_view import CudaVideoWidget
        
        return CudaVideoWidget

    def _init_vulkan(self) -> type[VideoMixin]:
        if not isinstance(QCoreApplication.instance(), QApplication):
            raise RuntimeError("Rendering with Vulkan needs a QApplication (a QGuiApplication is not enough) to exist first; StreamDisplay.start() creates one")

        from chiaki_py.gui.stream.views.vulkan_view import VulkanVideoWidget
        
        return VulkanVideoWidget

    def close(self) -> None:
        if self.aspect_lock is not None:
            self.aspect_lock.remove()
        self.controller_thread.stop()
        if self.frame_thread is not None:
            self.frame_thread.stop()
        self.session.stop()

    @classmethod
    def start(cls, session: Session, argv: list[str], show_stats: bool = False, keep_aspect_ratio: bool = False):
        app = QCoreApplication.instance() or QApplication(argv)
        _ = StreamDisplay(session, show_stats, keep_aspect_ratio)
        return app.exec()
