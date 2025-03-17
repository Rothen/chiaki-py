#ifndef CHIAKI_PY_AV_FRAME_H
#define CHIAKI_PY_AV_FRAME_H

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <stdexcept>

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

namespace py = pybind11;

class PyAVFrame
{
public:
    AVFrame *frame;

    PyAVFrame();

    ~PyAVFrame();

    int width() const { return frame->width; }
    void set_width(int w) { frame->width = w; }

    int height() const { return frame->height; }
    void set_height(int h) { frame->height = h; }

    int format() const { return frame->format; }
    void set_format(int fmt) { frame->format = fmt; }

    int64_t pts() const { return frame->pts; }
    void set_pts(int64_t pts) { frame->pts = pts; }

    py::bytes data(int index);

    py::array_t<uint8_t> to_numpy(int index);
};

#endif // CHIAKI_PY_AV_FRAME_H