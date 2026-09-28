"""Generates the pybind11 bindings of the chiaki_py extension module, and their type stubs, from the
headers in pybind/include with litgen (https://pthom.github.io/litgen):

    python pybind/bindings/generate_bindings.py

Run it after changing a bound header and commit what it writes. Each entry of BINDINGS turns some
headers into

  - the code between the <litgen_pydef> markers of pybind/bindings/pydef_<name>.cpp, and
  - the code between the <litgen_stub_<name>> markers of chiaki_py/lib/chiaki_py/__init__.pyi.

Everything outside the markers is written by hand and kept as it is, including all of
pydef_chiaki.cpp and pydef_event_source.cpp, which bind chiaki's C types and the EventSource<T>
instances (litgen only handles the headers of this project).
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

import litgen
from codemanip import code_utils

ROOT = Path(__file__).resolve().parents[2]
INCLUDE_DIR = ROOT / "pybind" / "include"
BINDINGS_DIR = ROOT / "pybind" / "bindings"
STUB_FILE = ROOT / "chiaki_py" / "lib" / "chiaki_py" / "__init__.pyi"

def event_source(argument: str) -> str:
    """A regex for EventSource<`argument`>, whose argument litgen may have translated to Python already."""
    return rf"\bEventSource\s*[<\[]\s*(?:const\s+)?(?:{argument})\s*[&*]?\s*[>\]]"


# The Python names of the types bound by hand in pydef_chiaki.cpp and pydef_event_source.cpp, for the stubs.
PYTHON_TYPE_NAMES = {
    event_source("bool"): "BoolEventSource",
    event_source("double|float"): "DoubleEventSource",
    event_source("std::string|str"): "StringEventSource",
    event_source("ChiakiQuitReason|QuitReason"): "ChiakiQuitReasonEventSource",
    event_source("ChiakiRegisteredHost"): "RegisteredHostEventSource",
    event_source("ChiakiRegistEvent"): "RegistEventSource",
    r"\bChiakiTarget\b": "Target",
    r"\bChiakiCodec\b": "Codec",
    r"\bChiakiLogLevel\b": "LogLevel",
    r"\bChiakiDisableAudioVideo\b": "DisableAudioVideo",
    r"\bChiakiVideoResolutionPreset\b": "VideoResolutionPreset",
    r"\bChiakiVideoFPSPreset\b": "VideoFPSPreset",
    r"\bChiakiQuitReason\b": "QuitReason",
    r"\bChiakiDiscoveryHostState\b": "DiscoveryHostState",
    r"\bChiakiRegistEventType\b": "RegistEventType",
    r"\bChiakiFfmpegDecoder\b": "FfmpegDecoder",
    r"\bpy::object\b": "Any",
    r"\bpy::bytes\b": "bytes",
    r"\bpy::array_t<.*>": "np.ndarray",
    r"\buintptr_t\b": "int",
}


def remove_friend_declarations(code: str) -> str:
    # srcml loses the constructor of a class that starts with one, and they never matter to Python.
    return re.sub(r"^\s*friend\s+(?:class|struct)\s+\w+\s*;", "", code, flags=re.M)


def base_options() -> litgen.LitgenOptions:
    options = litgen.LitgenOptions()
    options.srcmlcpp_options.code_preprocess_function = remove_friend_declarations
    options.fn_exclude_non_api = True
    # The comments are written as Python docstrings already: no turning `3d` into `3` as if it were a C number.
    options.comments_replacements = litgen.RegexReplacementList()

    # Everything a bound function returns by reference or pointer is owned by C++ (a session's
    # AudioHandler, its event sources, its decoder, ...): never let Python copy or delete it.
    options.fn_return_force_policy_reference_for_pointers__regex = r".*"
    options.fn_return_force_policy_reference_for_references__regex = r".*"

    options.struct_create_default_named_ctor__regex = r""
    options.enum_export_values = True

    for cpp_type, python_type in PYTHON_TYPE_NAMES.items():
        options.type_replacements.add_last_replacement(cpp_type, python_type)
    options.value_replacements.add_first_replacement(r"\bpy::none\(\)", "None")
    return options


def keep_enum_value_names(options: litgen.LitgenOptions, header: str) -> None:
    """Keep the names of the values of `header`'s own `enum class`es as they are in C++ (they are
    CamelCase in Python too), instead of litgen's snake_case."""
    code = (INCLUDE_DIR / header).read_text(encoding="utf-8")
    for body in re.findall(r"enum class \w+\s*\{(.*?)\}", code, flags=re.S):
        for value in re.findall(r"^\s*(\w+)\s*(?:=[^,\n]*)?,?\s*(?://.*)?$", body, flags=re.M):
            options.var_names_replacements.add_last_replacement(f"^{code_utils.to_snake_case(value)}$", value)


def add_keep_alive_constructor(
    options: litgen.LitgenOptions, cls: str, cpp_params: str, params: list[tuple[str, ...]], doc: str = ""
) -> None:
    """Bind the constructor of `cls` taking `cpp_params`, named and typed in Python as `params`, such that
    the first argument (the ChiakiPySession the new object works on) lives at least as long as the object.
    A parameter with a default is (name, python_type, python_default, cpp_default)."""
    stub_params = ", ".join(f"{p[0]}: {p[1]}" + (f" = {p[2]}" if len(p) > 2 else "") for p in params)
    py_args = ", ".join(f'py::arg("{p[0]}")' + (f" = {p[3]}" if len(p) > 2 else "") for p in params)
    options.custom_bindings.add_custom_bindings_to_class(
        cls,
        stub_code=f"def __init__(self, {stub_params}) -> None:\n" + (f'    """{doc}"""\n' if doc else "") + "    pass",
        pydef_code=f"LG_CLASS.def(py::init<{cpp_params}>(), {py_args}, py::keep_alive<1, 2>()"
        + (f', "{doc}"' if doc else "") + ");",
    )


def configure_settings(options: litgen.LitgenOptions) -> None:
    keep_enum_value_names(options, "settings.h")
    options.enum_exclude_by_name__regex = r"^ControllerButtonExt$"
    # get_fpslocal_ps4() & co. rather than get_fps_local_ps4(), as they have always been called.
    options.function_names_replacements.add_last_replacement(r"FPSLocal", "Fpslocal")
    options.function_names_replacements.add_last_replacement(r"FPSRemote", "Fpsremote")
    options.custom_bindings.add_custom_bindings_to_class(
        "Settings",
        stub_code="""
            def __repr__(self) -> str:
                pass
        """,
        pydef_code="""
            LG_CLASS.def("__repr__", [](const Settings &s)
            {
                std::ostringstream repr;
                repr << "<Settings("
                     << "logVerbose=" << (s.GetLogVerbose() ? "True" : "False") << ", "
                     << "logLevelMask=" << s.GetLogLevelMask() << ", "
                     << "rumbleHapticsIntensity=" << static_cast<int>(s.GetRumbleHapticsIntensity()) << ", "
                     << "buttonsByPosition=" << (s.GetButtonsByPosition() ? "True" : "False") << ", "
                     << "startMicUnmuted=" << (s.GetStartMicUnmuted() ? "True" : "False") << ", "
                     << "hapticOverride=" << s.GetHapticOverride() << ", "
                     << "resolutionLocalPS4=" << static_cast<int>(s.GetResolutionLocalPS4()) << ", "
                     << "resolutionRemotePS4=" << static_cast<int>(s.GetResolutionRemotePS4()) << ", "
                     << "fpsLocalPS4=" << static_cast<int>(s.GetFPSLocalPS4()) << ", "
                     << "fpsRemotePS4=" << static_cast<int>(s.GetFPSRemotePS4()) << ", "
                     << "bitrateLocalPS4=" << s.GetBitrateLocalPS4() << ", "
                     << "bitrateRemotePS4=" << s.GetBitrateRemotePS4() << ", "
                     << "codecPS4=" << static_cast<int>(s.GetCodecPS4()) << ", "
                     << "decoder=" << static_cast<int>(s.GetDecoder()) << ", "
                     << "hardwareDecoder='" << s.GetHardwareDecoder() << "', "
                     << "packetLossMax=" << s.GetPacketLossMax() << ", "
                     << "audioVolume=" << s.GetAudioVolume() << ", "
                     << "audioBufferSize=" << s.GetAudioBufferSize() << ", "
                     << "audioOutDevice='" << s.GetAudioOutDevice() << "', "
                     << "audioInDevice='" << s.GetAudioInDevice() << "', "
                     << "psnAccountId='" << s.GetPsnAccountId() << "'"
                     << ")>";
                return repr.str();
            });
        """,
    )


def configure_discovery(options: litgen.LitgenOptions) -> None:
    options.class_exclude_by_name__regex = r"^(HiddenHost|RegisteredHost|ManualHost|PsnHost)$"
    options.fn_exclude_by_name__regex = r"^HostMAC$"  # its constructors
    options.custom_bindings.add_custom_bindings_to_class(
        "HostMAC",
        stub_code="""
            def __str__(self) -> str:
                pass
        """,
        pydef_code="""
            LG_CLASS.def("__str__", &HostMAC::ToString);
        """,
    )


def configure_audio(options: litgen.LitgenOptions) -> None:
    options.fn_exclude_by_name__regex = r"^(AudioHandler|AudioOutput)$"  # their constructors
    # (These also exclude methods of the same names.)
    options.member_exclude_by_name_and_class__regex = {"AudioHandler": r"^audio_"}
    options.fn_add_gil_scoped_release_guard__regex = r"^(Open|Close)$"
    # An AudioOutput plays a session's AudioHandler, which lives as long as the session.
    options.custom_bindings.add_custom_bindings_to_class(
        "AudioOutput",
        stub_code="""
            def __init__(self, session: ChiakiPySession) -> None:
                pass
        """,
        pydef_code="""
            LG_CLASS.def(py::init([](ChiakiPySession &session) { return new AudioOutput(session.GetAudioHandler()); }),
                         py::arg("session"), py::keep_alive<1, 2>());
        """,
    )


def configure_session(options: litgen.LitgenOptions) -> None:
    options.class_exclude_by_name__regex = r"^(KeyEvent|ChiakiException)$"
    # Only the constructors of ChiakiPySessionConnectInfo, none of its fields. (These regexes also
    # exclude methods of the same names.)
    options.member_exclude_by_name_and_class__regex = {
        "ChiakiPySessionConnectInfo": r"^(?!ChiakiPySessionConnectInfo$)",
        "ChiakiPySession": r"^controller_state$",
    }


VULKAN_FRAME_PROPERTIES = [
    # (name, C++ getter, Python type, docstring)
    ("hw_type", "&VulkanFrame::hw_type", "str", "Hardware decoder type, e.g. 'vulkan', 'cuda', 'd3d11va'."),
    ("format", "&VulkanFrame::format", "str", "Pixel format of the frame itself, e.g. 'vulkan', 'cuda', 'd3d11'."),
    ("sw_format", "&VulkanFrame::sw_format", "str", "Underlying pixel layout on the GPU, e.g. 'nv12' or 'p010le'."),
    ("width", "[](const VulkanFrame &f) { f.require_frame(); return f.frame->width; }", "int",
     "Width in pixels. Raises RuntimeError while the frame holds no decoded frame yet."),
    ("height", "[](const VulkanFrame &f) { f.require_frame(); return f.frame->height; }", "int",
     "Height in pixels. Raises RuntimeError while the frame holds no decoded frame yet."),
    ("data", "&VulkanFrame::data", "List[int]", "Raw per-plane pointers / handles (integers)."),
    ("linesize", "&VulkanFrame::linesize", "List[int]", "Row stride in bytes for each entry of `data`."),
    ("device_hwctx", "&VulkanFrame::device_hwctx", "int",
     "Address of the hardware device context struct (AVVulkanDeviceContext*, AVCUDADeviceContext*, ...) the frame lives on."),
]


def configure_frame_handler(options: litgen.LitgenOptions) -> None:
    options.class_exclude_by_name__regex = r"^(ThreadSwsContext|CudaFrameLayout|CudaArrayDestination)$"
    # Their constructors: none for VulkanFrame and the abstract FrameHandler, the others are bound below.
    # yuv_to_rgb_params() takes an AVFrame*, which Python has no way to make.
    options.fn_exclude_by_name__regex = (
        r"^(VulkanFrame|FrameHandler|CpuFrameHandler|CudaFrameHandler|VulkanFrameHandler|yuv_to_rgb_params)$"
    )
    options.member_exclude_by_name_and_class__regex = {"VulkanFrame": r"^frame$"}
    options.member_readonly_by_name__regex = r"^(pts|duration)$"

    options.custom_bindings.add_custom_bindings_to_class(
        "VulkanFrame",
        stub_code="\n".join(
            f'@property\ndef {name}(self) -> {python_type}:\n    """{doc}"""\n    pass'
            for name, _, python_type, doc in VULKAN_FRAME_PROPERTIES
        ),
        pydef_code="\n".join(
            f'LG_CLASS.def_property_readonly("{name}", {getter}, "{doc}");'
            for name, getter, _, doc in VULKAN_FRAME_PROPERTIES
        ),
    )
    for cls in ("CpuFrameHandler", "CudaFrameHandler", "VulkanFrameHandler"):
        add_keep_alive_constructor(options, cls, "ChiakiPySession *", [("stream_session", "ChiakiPySession")])


def configure_vulkan_renderer(options: litgen.LitgenOptions) -> None:
    options.fn_exclude_by_name__regex = r"^VulkanRenderer$"  # its constructor, bound below
    keep_enum_value_names(options, "vulkan_renderer.h")
    options.fn_add_gil_scoped_release_guard__regex = r"^(render|close)$"
    add_keep_alive_constructor(
        options, "VulkanRenderer", "ChiakiPySession &, uintptr_t, uintptr_t, VulkanWindowSystem",
        [
            ("stream_session", "ChiakiPySession"),
            ("window", "int"),
            ("display", "int", "0", "0"),
            ("window_system", "VulkanWindowSystem", "VulkanWindowSystem.Default", "VulkanWindowSystem::Default"),
        ],
        doc="Draw into the native window `window`, of the connection `display`, as `window_system` says: an HWND on "
            "Windows; an X11 Window id (e.g. Qt's winId()) and an Xlib Display* or 0 on X11; a wl_surface* and its "
            "wl_display* on Wayland. The session must use the Vulkan hardware decoder (Settings.set_hardware_decoder('vulkan')); "
            "raises RuntimeError otherwise, or if the window can't be drawn into.",
    )
    options.custom_bindings.add_custom_bindings_to_class(
        "VulkanRenderer",
        stub_code='''
            def set_overlay(self, rgba: np.ndarray, margin: int = 12) -> None:
                """Show `rgba`, a uint8 array of shape (height, width, 4) with premultiplied alpha, on top of the video
                in the top-right corner of the window, `margin` pixels from its edges, until it is replaced or cleared.
                The pixels are copied. libplacebo composites it in the same pass that draws the video, so it is not
                part of the frames, and it stays where it is while the window is resized."""
                pass
        ''',
        pydef_code="""
            LG_CLASS.def("set_overlay",
                [](VulkanRenderer &renderer, const py::array_t<uint8_t, py::array::c_style | py::array::forcecast> &rgba, int margin)
                {
                    if (rgba.ndim() != 3 || rgba.shape(2) != 4)
                        throw py::value_error("The overlay must be an array of shape (height, width, 4)");
                    renderer.set_overlay(rgba.data(), static_cast<int>(rgba.shape(1)), static_cast<int>(rgba.shape(0)), margin);
                },
                py::arg("rgba"), py::arg("margin") = 12,
                "Show `rgba`, a uint8 array of shape (height, width, 4) with premultiplied alpha, on top of the video "
                "in the top-right corner of the window, `margin` pixels from its edges, until it is replaced or cleared. "
                "The pixels are copied. libplacebo composites it in the same pass that draws the video, so it is not "
                "part of the frames, and it stays where it is while the window is resized.");
        """,
    )


def configure_backend(options: litgen.LitgenOptions) -> None:
    keep_enum_value_names(options, "backend.h")
    options.class_exclude_by_name__regex = r"^Regist$"
    options.fn_exclude_by_name__regex = r"^RegistResult$"  # its constructors
    options.macro_define_include_by_name__regex = r"^(PSN_|MAX_PSN_|WAKEUP_)"
    options.member_exclude_by_name_and_class__regex = {"RegistResult": r"^rp_key$"}
    options.member_readonly_by_name__regex = r".*"
    options.custom_bindings.add_custom_bindings_to_class(
        "RegistResult",
        stub_code='''
            @property
            def rp_key(self) -> bytes:
                """The RP key, which ChiakiPySessionConnectInfo takes as `morning`."""
                pass
            def __repr__(self) -> str:
                pass
        ''',
        pydef_code="""
            LG_CLASS.def_property_readonly("rp_key", [](const RegistResult &r)
            {
                return py::bytes(reinterpret_cast<const char *>(r.rp_key), sizeof(r.rp_key));
            }, "The RP key, which ChiakiPySessionConnectInfo takes as `morning`.");
            LG_CLASS.def("__repr__", [](const RegistResult &r)
            {
                std::ostringstream rp_key;
                for (uint8_t byte : r.rp_key)
                    rp_key << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                std::ostringstream repr;
                repr << "<RegistResult"
                     << " type=" << py::str(py::cast(r.type)).cast<std::string>()
                     << " target=" << py::str(py::cast(r.target)).cast<std::string>()
                     << " ap_ssid='" << r.ap_ssid << "'"
                     << " ap_bssid='" << r.ap_bssid << "'"
                     << " ap_key='" << r.ap_key << "'"
                     << " ap_name='" << r.ap_name << "'"
                     << " server_mac=" << r.server_mac
                     << " server_nickname='" << r.server_nickname << "'"
                     << " rp_regist_key='" << r.rp_regist_key << "'"
                     << " rp_key='" << rp_key.str() << "'"
                     << " rp_key_type=" << r.rp_key_type
                     << " console_pin=" << r.console_pin
                     << ">";
                return repr.str();
            });
        """,
    )


@dataclass
class Binding:
    name: str  # Of the pydef_<name>.cpp file, and of the <litgen_stub_<name>> section of the stub.
    headers: list[str]
    configure: Callable[[litgen.LitgenOptions], None]


BINDINGS = [
    Binding("settings", ["settings.h"], configure_settings),
    Binding("discovery", ["host.h", "discovery_manager.h"], configure_discovery),
    Binding("audio", ["audio_handler.h", "audio_output.h"], configure_audio),
    Binding("session", ["chiakipysession.h"], configure_session),
    Binding("frame_handler", ["frame_handler.h"], configure_frame_handler),
    Binding("vulkan_renderer", ["vulkan_renderer.h"], configure_vulkan_renderer),
    Binding("backend", ["backend.h"], configure_backend),
]


def generate(binding: Binding) -> None:
    options = base_options()
    binding.configure(options)
    generator = litgen.LitgenGenerator(options)
    for header in binding.headers:
        generator.process_cpp_file(str(INCLUDE_DIR / header))
    if generator.has_glue_code():
        raise RuntimeError(f"{binding.name}: litgen needs glue code, which pydef_{binding.name}.cpp has no place for")

    pydef_file = BINDINGS_DIR / f"pydef_{binding.name}.cpp"
    code_utils.write_generated_code_between_markers(str(pydef_file), "litgen_pydef", generator.pydef_code())
    code_utils.write_generated_code_between_markers(str(STUB_FILE), f"litgen_stub_{binding.name}", generator.stub_code())
    print(f"{pydef_file.relative_to(ROOT)} <- {', '.join(binding.headers)}")


def main() -> None:
    for binding in BINDINGS:
        generate(binding)


if __name__ == "__main__":
    main()
