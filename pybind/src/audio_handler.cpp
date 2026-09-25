#include "audio_handler.h"
// #include "pylog.h"

#include <algorithm>
#include <deque>

namespace py = pybind11;

AudioHandler::AudioHandler()
{
}

AudioHandler::AudioHandler(size_t audio_out_sample_size, unsigned int audio_buffer_size, unsigned int audio_channels, unsigned int audio_rate) : audio_out_sample_size(audio_out_sample_size),
                                                                                                                                                 audio_buffer_size(audio_buffer_size),
                                                                                                                                                 audio_channels(audio_channels),
                                                                                                                                                 audio_rate(audio_rate)
{
}

void AudioHandler::QueueFrame(int16_t *buf, size_t samples_count)
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex);
    audio_buf.assign(buf, buf + samples_count * audio_channels);

    audio_queue.insert(audio_queue.end(), audio_buf.begin(), audio_buf.end());

    // If nobody drains the queue, drop the oldest samples to cap latency and memory.
    size_t max_samples = audio_queue_max_frames
                             ? audio_queue_max_frames * audio_channels
                             : 3 * audio_buffer_size / sizeof(int16_t); // audio_buffer_size is in bytes, same threshold as chiaki-ng
    if (audio_queue.size() > max_samples)
    {
        if (!audio_queue_overflow_logged)
        {
            // CHIAKI_LOGW(log.GetChiakiLog();, "Audio queue exceeded latency threshold, dropping oldest samples");
            audio_queue_overflow_logged = true;
        }
        size_t excess = audio_queue.size() - max_samples;
        excess -= excess % audio_channels; // keep channels aligned
        audio_queue.erase(audio_queue.begin(), audio_queue.begin() + excess);
    }
}

py::array_t<int16_t> AudioHandler::GetFrame(size_t max_frames)
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex);
    size_t count = audio_queue.size();
    if (max_frames)
    {
        count = (std::min)(count, max_frames * audio_channels);
    }
    std::vector<int16_t> out(audio_queue.begin(), audio_queue.begin() + count);
    audio_queue.erase(audio_queue.begin(), audio_queue.begin() + count);
    audio_queue_overflow_logged = false;

    if (audio_channels == 0 || out.empty())
        return py::array_t<int16_t>(std::vector<py::ssize_t>{0, 0});
    py::array_t<int16_t> arr({(py::ssize_t)(out.size() / audio_channels), (py::ssize_t)audio_channels});
    std::memcpy(arr.mutable_data(), out.data(), out.size() * sizeof(int16_t));
    return std::move(arr);
}

size_t AudioHandler::GetAudioQueuedFrames()
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex);
    return audio_channels ? audio_queue.size() / audio_channels : 0;
}

void AudioHandler::ClearAudioQueue()
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex);
    audio_queue.clear();
    audio_queue_overflow_logged = false;
}

size_t AudioHandler::GetAudioQueueMaxFrames()
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex);
    if (audio_queue_max_frames)
        return audio_queue_max_frames;
    return audio_out_sample_size ? 3 * audio_buffer_size / audio_out_sample_size : 0;
}

void AudioHandler::SetAudioQueueMaxFrames(size_t max_frames)
{
    std::lock_guard<std::mutex> lock(audio_buf_mutex);
    audio_queue_max_frames = max_frames;
}