"""Qt widgets that display a Session's frames and forward DualSense input to it, kept separate
from the core package since it depends on PyQt6 (and, for controller input, `dualsense-py`).

`StreamDisplay` is the entry point: it opens a window sized and shaped for the session's frame
handler (CPU, CUDA or Vulkan - see the individual video widgets) and drives it until closed.
"""

from typing import TYPE_CHECKING

from .stream.stream_display import StreamDisplay, ControllerThread
from .stream.views.base_view import BaseView, VideoMixin
from .stream.views.cpu_view import CpuVideoWidget
from .stream.views.vulkan_view import VulkanVideoWidget, VulkanRenderThread
from .stream.stats_overlay import StatsOverlay
from .stream.aspect_ratio import AspectRatioLock
from .stream.threads.frame_thread import FrameThread
from .stream.threads.fps_thread import FpsThread

if TYPE_CHECKING:
    from .stream.views.cuda_view import CudaVideoWidget

__all__ = [
    "StreamDisplay",
    "ControllerThread",
    "BaseView",
    "VideoMixin",
    "CpuVideoWidget",
    "VulkanVideoWidget",
    "VulkanRenderThread",
    "StatsOverlay",
    "AspectRatioLock",
    "FrameThread",
    "FpsThread",
]


def __getattr__(name: str):
    # CudaVideoWidget needs cuda-python, CuPy and PyOpenGL (NVIDIA only, not installed on macOS), so it
    # is imported on first use rather than with the package; it is left out of __all__ for the same reason.
    if name == "CudaVideoWidget":
        from .stream.views.cuda_view import CudaVideoWidget
        return CudaVideoWidget
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
