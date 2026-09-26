#ifndef CHIAKI_PY_AUDIO_OUTPUT_H
#define CHIAKI_PY_AUDIO_OUTPUT_H

#include <cstdint>
#include <string>
#include <vector>

#include "audio_handler.h"

// Plays an AudioHandler's queue on an SDL audio device. SDL's audio thread calls back into C++ for
// exactly as many frames as the device needs and takes them straight off the queue, so playback never
// waits for the GIL. SDL loads the platform's audio backend (WASAPI, CoreAudio, PulseAudio/PipeWire,
// ALSA) at runtime, so nothing beyond the SDL library bundled with the module is needed.
class AudioOutput
{
public:
    explicit AudioOutput(AudioHandler &audio_handler);
    ~AudioOutput();
    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    // Opens the device (`device_name` as listed by GetDevices(), empty = the system default) at the
    // handler's current rate and channel count, and starts playing. Throws if audio has not started
    // yet (the rate is only known once the first frame arrived), or if SDL can't open the device.
    void Open(const std::string &device_name = "");
    // Stops playback and closes the device. Does nothing if it isn't open.
    void Close();
    bool IsOpen() const { return device != 0; }

    // Names of the available output devices.
    static std::vector<std::string> GetDevices();

private:
    static void Callback(void *user, uint8_t *stream, int len);

    AudioHandler &audio_handler;
    uint32_t device = 0; // SDL_AudioDeviceID, 0 = closed
    unsigned int channels = 0;
};

#endif // CHIAKI_PY_AUDIO_OUTPUT_H
