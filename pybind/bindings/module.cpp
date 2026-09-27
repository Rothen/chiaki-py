#ifdef _WIN32
#include <winsock2.h>
#endif

#include "pydef.h"
#include "pylog.h"

PYBIND11_MODULE(chiaki_py, m)
{
    m.doc() = "Low-level pybind11 bindings around Chiaki, the PS4/PS5 Remote Play client library: "
              "discover consoles (DiscoveryManager), register with one (Backend), connect and stream "
              "one (ChiakiPySession) and pull its decoded frames (FrameHandler and subclasses). Most "
              "users want the pythonic wrapper in the `chiaki_py` package instead of this module directly.";

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    init_pylog();

    // Types before the functions taking or returning them, so that their signatures show the Python names.
    py_init_chiaki(m);
    py_init_event_source(m);
    py_init_settings(m);
    py_init_discovery(m);
    py_init_audio(m);
    py_init_session(m);
    py_init_frame_handler(m);
    py_init_vulkan_renderer(m);
    py_init_backend(m);
}
