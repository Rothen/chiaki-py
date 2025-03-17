#ifndef CHIAKI_PY_CORE_AUDIORECEIVER_H
#define CHIAKI_PY_CORE_AUDIORECEIVER_H

#include "core/struct_wrapper.h"
#include "core/audio.h"

#include <chiaki/audioreceiver.h>

#include <vector>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_audioreceiver(py::module &m);

typedef std::function<void(AudioHeaderWrapper &header, void *user)> ChiakiAudioSinkHeaderFunc;
typedef std::function<void(std::vector<uint8_t> &buf, void *user)> ChiakiAudioSinkFrameFunc;

class AudioSinkWrapper : public StructWrapper<ChiakiAudioSink>
{
public:
    using StructWrapper::StructWrapper;

    AudioSinkWrapper(uint8_t channels, uint8_t bits, uint32_t rate, uint32_t frame_size) : StructWrapper()
    {
        chiaki_audio_header_set(ptr(), channels, bits, rate, frame_size);
    }

    void load(const std::vector<uint8_t> &buf)
    {
        return chiaki_audio_header_load(ptr(), buf.data());
    }

    void save(std::vector<uint8_t> &buf)
    {
        return chiaki_audio_header_save(ptr(), buf.data());
    }

    size_t frame_buf_size()
    {
        return chiaki_audio_header_frame_buf_size(ptr());
    }
};

#endif // CHIAKI_PY_CORE_AUDIORECEIVER_H