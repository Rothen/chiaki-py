#ifndef CHIAKI_PY_AUDIO_OUTPUT_H
#define CHIAKI_PY_AUDIO_OUTPUT_H

#include <cstdint>
#include <string>
#include <vector>

#include "audio_handler.h"

// Plays a session's audio on an SDL output device. SDL's audio thread takes the frames straight
// off the session's AudioHandler queue, without the GIL, so don't read that queue anywhere else
// while it is open. chiaki_py.AudioSink wraps it.
//
// SDL calls back into C++ for exactly as many frames as the device needs, so playback never waits
// for the GIL. SDL loads the platform's audio backend (WASAPI, CoreAudio, PulseAudio/PipeWire,
// ALSA) at runtime, so nothing beyond the SDL library bundled with the module is needed.
class AudioOutput
{
public:
    explicit AudioOutput(AudioHandler &audio_handler);
    ~AudioOutput();
    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    // Open the output device (a name from get_devices(), '' = the system default) at the session's audio
    // rate and channel count and start playing. Raises RuntimeError before the first audio frame arrived,
    // or if the device can't be opened.
    void Open(const std::string &device_name = "");

    // Stop playing and close the device. Does nothing if it isn't open.
    void Close();

    // Whether the device is open.
    bool IsOpen() const { return device != 0; }

    // Names of the available audio output devices.
    static std::vector<std::string> GetDevices();

private:
    static void Callback(void *user, uint8_t *stream, int len);

    AudioHandler &audio_handler;
    uint32_t device = 0; // SDL_AudioDeviceID, 0 = closed
    unsigned int channels = 0;
};

#endif // CHIAKI_PY_AUDIO_OUTPUT_H
