#include "audio_output.h"
#include "pylog.h"

#include <cstring>
#include <stdexcept>

#define SDL_MAIN_HANDLED
#include <SDL.h>

static constexpr Uint16 AUDIO_OUT_SAMPLES = 512;

static void InitSdlAudio()
{
    SDL_SetMainReady();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        throw std::runtime_error(std::string("Failed to initialize SDL audio: ") + SDL_GetError());
}

AudioOutput::AudioOutput(AudioHandler &audio_handler) : audio_handler(audio_handler)
{
}

AudioOutput::~AudioOutput()
{
    GilReleaseIfHeld release;
    Close();
}

void AudioOutput::Open(const std::string &device_name)
{
    if (device)
        throw std::runtime_error("AudioOutput is already open");
    unsigned int rate = audio_handler.GetAudioRate();
    unsigned int audio_channels = audio_handler.GetAudioChannels();
    if (!rate || !audio_channels)
        throw std::runtime_error("The session has no audio yet: open the AudioOutput once the first audio frame arrived");

    InitSdlAudio();

    SDL_AudioSpec want = {};
    want.freq = (int)rate;
    want.format = AUDIO_S16SYS;
    want.channels = (Uint8)audio_channels;
    want.samples = AUDIO_OUT_SAMPLES;
    want.callback = &AudioOutput::Callback;
    want.userdata = this;
    channels = audio_channels;

    SDL_AudioSpec have;
    device = SDL_OpenAudioDevice(device_name.empty() ? nullptr : device_name.c_str(), 0, &want, &have, 0);
    if (!device)
    {
        std::string error = SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        throw std::runtime_error("Failed to open audio device '" + (device_name.empty() ? std::string("default") : device_name) + "': " + error);
    }
    SDL_PauseAudioDevice(device, 0);
}

void AudioOutput::Close()
{
    if (!device)
        return;
    SDL_CloseAudioDevice(device);
    device = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

std::vector<std::string> AudioOutput::GetDevices()
{
    InitSdlAudio();
    std::vector<std::string> devices;
    int count = SDL_GetNumAudioDevices(0);
    for (int i = 0; i < count; i++)
    {
        const char *name = SDL_GetAudioDeviceName(i, 0);
        if (name)
            devices.emplace_back(name);
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return devices;
}

void AudioOutput::Callback(void *user, uint8_t *stream, int len)
{
    auto *self = static_cast<AudioOutput *>(user);
    auto *out = reinterpret_cast<int16_t *>(stream);
    size_t frames = (size_t)len / (sizeof(int16_t) * self->channels);
    size_t read = self->audio_handler.ReadFrames(out, frames);
    std::memset(out + read * self->channels, 0, (size_t)len - read * self->channels * sizeof(int16_t));
}
