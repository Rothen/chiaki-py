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

// The queue of a ChiakiPySession's decoded audio (interleaved int16 PCM), as returned by
// ChiakiPySession.get_audio_handler(). Every read removes the frames it returns, so use one
// consumer only: get_frame(), an AudioOutput or chiaki_py.AudioSink.
class AudioHandler
{
public:
    AudioHandler();
    AudioHandler(size_t audio_out_sample_size, unsigned int audio_buffer_size, unsigned int audio_channels, unsigned int audio_rate);
    virtual ~AudioHandler() = default;

    void QueueFrame(int16_t *buf, size_t samples_count);

    // Remove and return up to max_frames queued audio frames (0 = all) as an int16 array of shape
    // (frames, channels), or of shape (0, 0) if none are queued.
    py::array_t<int16_t> GetFrame(size_t max_frames = 0);

    size_t ReadFrames(int16_t *out, size_t max_frames);

    // The number of audio frames waiting in the queue.
    size_t GetAudioQueuedFrames();

    // Drop every queued audio frame.
    void ClearAudioQueue();

    // The queue capacity in frames; the oldest frames are dropped beyond it.
    size_t GetAudioQueueMaxFrames();

    // Set the queue capacity in frames (0 = 3x the audio buffer size from Settings).
    void SetAudioQueueMaxFrames(size_t max_frames);

    // The number of audio channels; 0 until the first audio frame arrived.
    unsigned int GetAudioChannels() { return audio_channels; }

    // The audio sample rate in Hz; 0 until the first audio frame arrived.
    unsigned int GetAudioRate() { return audio_rate; }

    size_t audio_out_sample_size = 0;
    unsigned int audio_buffer_size;
    unsigned int audio_channels = 0;
    unsigned int audio_rate = 0;
private:

    std::vector<int16_t> audio_buf;
    std::deque<int16_t> audio_queue;
    size_t audio_queue_max_frames = 0;
    bool audio_queue_overflow_logged = false;
    std::mutex audio_buf_mutex;
};

#endif // CHIAKI_PY_AUDIO_HANDLER_H
