#ifndef CHIAKI_PY_AUDIO_HANDLER_H
#define CHIAKI_PY_AUDIO_HANDLER_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

namespace py = pybind11;

class AudioHandler
{
public:
    AudioHandler();
    AudioHandler(size_t audio_out_sample_size, unsigned int audio_buffer_size, unsigned int audio_channels, unsigned int audio_rate);
    virtual ~AudioHandler() = default;

    void QueueFrame(int16_t *buf, size_t samples_count);
    py::array_t<int16_t> GetFrame(size_t max_frames = 0);
    size_t GetAudioQueuedFrames();
    void ClearAudioQueue();
    size_t GetAudioQueueMaxFrames();
    void SetAudioQueueMaxFrames(size_t max_frames);
    unsigned int GetAudioChannels() { return audio_channels; }
    unsigned int GetAudioRate() { return audio_rate; }

    size_t audio_out_sample_size = 0;
    unsigned int audio_buffer_size;
    unsigned int audio_channels = 0;
    unsigned int audio_rate = 0;
private:

    std::vector<int16_t> audio_buf;    // latest decoded frame, interleaved PCM
    std::deque<int16_t> audio_queue;   // every decoded sample not yet read, interleaved PCM
    size_t audio_queue_max_frames = 0; // 0 = derive from audio_buffer_size
    bool audio_queue_overflow_logged = false;
    std::mutex audio_buf_mutex; // guards audio_buf and audio_queue: written by the decoder thread, read from Python
};

#endif // CHIAKI_PY_AUDIO_HANDLER_H
