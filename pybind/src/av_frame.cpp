#include "av_frame.h"

PyAVFrame::PyAVFrame()
{
    frame = av_frame_alloc();
    if (!frame)
    {
        throw std::runtime_error("Failed to allocate AVFrame");
    }
}

PyAVFrame::~PyAVFrame()
{
    if (frame)
    {
        av_frame_free(&frame);
    }
}

py::bytes PyAVFrame::data(int index)
{
    if (index < 0 || index >= AV_NUM_DATA_POINTERS)
        throw std::out_of_range("Invalid data index");
    return py::bytes(reinterpret_cast<const char *>(frame->data[index]), frame->linesize[index]);
}

py::array_t<uint8_t> PyAVFrame::to_numpy(int index)
{
    if (index < 0 || index >= AV_NUM_DATA_POINTERS)
        throw std::out_of_range("Invalid data index");

    int height = frame->height;
    int width = frame->linesize[index]; // Usually bytes per line, but depends on format
    if (height == 0 || width == 0)
    {
        throw std::runtime_error("Frame not properly initialized");
    }

    return py::array_t<uint8_t>(
        {height, width},
        {static_cast<size_t>(frame->linesize[index]), sizeof(uint8_t)},
        frame->data[index]);
}