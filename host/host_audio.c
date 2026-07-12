#include "host_platform.h"
#include "audio.h"

#include <SDL.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define HOST_SAMPLE_RATE 48000
#define TAU 6.28318530717958647692f

typedef struct {
    float phase;
    float frequency;
    int waveform;
    int64_t samples_left;
    uint32_t noise;
    bool active;
} HostVoice;

static SDL_AudioDeviceID device;
static HostVoice voices[AUDIO_NUM_CHANNELS];
static float master_volume = 0.35f;
static bool mixing_paused;

static float voice_sample(HostVoice *voice) {
    float sample;
    switch (voice->waveform) {
        case WAVE_SINE:
            sample = sinf(voice->phase * TAU);
            break;
        case WAVE_SAW:
            sample = voice->phase * 2.0f - 1.0f;
            break;
        case WAVE_TRIANGLE:
            sample = 1.0f - 4.0f * fabsf(voice->phase - 0.5f);
            break;
        case WAVE_NOISE:
            sample = ((float)((voice->noise >> 16) & 0xffffu) / 32767.5f) - 1.0f;
            break;
        case WAVE_SQUARE:
        default:
            sample = voice->phase < 0.5f ? 1.0f : -1.0f;
            break;
    }

    voice->phase += voice->frequency / (float)HOST_SAMPLE_RATE;
    const bool wrapped = voice->phase >= 1.0f;
    voice->phase -= floorf(voice->phase);
    if (voice->waveform == WAVE_NOISE && wrapped) {
        voice->noise = voice->noise * 1664525u + 1013904223u;
    }
    if (voice->samples_left > 0 && --voice->samples_left == 0) {
        voice->active = false;
    }
    return sample;
}

static void audio_callback(void *userdata, Uint8 *stream, int length) {
    (void)userdata;
    float *output = (float *)stream;
    const int samples = length / (int)sizeof(float);
    memset(stream, 0, (size_t)length);
    if (mixing_paused) {
        return;
    }

    for (int i = 0; i < samples; ++i) {
        float mixed = 0.0f;
        int active = 0;
        for (int channel = 0; channel < AUDIO_NUM_CHANNELS; ++channel) {
            if (voices[channel].active) {
                mixed += voice_sample(&voices[channel]);
                ++active;
            }
        }
        if (active > 0) {
            mixed /= (float)active;
        }
        output[i] = mixed * master_volume;
    }
}

bool audio_init(void) {
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0 &&
        SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        return false;
    }

    SDL_AudioSpec wanted;
    SDL_zero(wanted);
    wanted.freq = HOST_SAMPLE_RATE;
    wanted.format = AUDIO_F32SYS;
    wanted.channels = 1;
    wanted.samples = 512;
    wanted.callback = audio_callback;

    SDL_AudioSpec obtained;
    SDL_zero(obtained);
    device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
    if (device == 0 || obtained.format != AUDIO_F32SYS || obtained.channels != 1) {
        if (device != 0) {
            SDL_CloseAudioDevice(device);
            device = 0;
        }
        return false;
    }

    memset(voices, 0, sizeof(voices));
    for (int i = 0; i < AUDIO_NUM_CHANNELS; ++i) {
        voices[i].noise = 0x12345678u + (uint32_t)i;
    }
    SDL_PauseAudioDevice(device, 0);
    return true;
}

void audio_tone(int channel, float frequency, int duration_ms, int waveform) {
    if (device == 0 || channel < 0 || channel >= AUDIO_NUM_CHANNELS || frequency <= 0.0f) {
        return;
    }
    SDL_LockAudioDevice(device);
    HostVoice *voice = &voices[channel];
    voice->phase = 0.0f;
    voice->frequency = frequency;
    voice->waveform = waveform;
    voice->samples_left = duration_ms > 0
        ? ((int64_t)duration_ms * HOST_SAMPLE_RATE) / 1000
        : -1;
    voice->active = true;
    SDL_UnlockAudioDevice(device);
}

void audio_stop(int channel) {
    if (device == 0) {
        return;
    }
    SDL_LockAudioDevice(device);
    if (channel < 0) {
        for (int i = 0; i < AUDIO_NUM_CHANNELS; ++i) {
            voices[i].active = false;
        }
    } else if (channel < AUDIO_NUM_CHANNELS) {
        voices[channel].active = false;
    }
    SDL_UnlockAudioDevice(device);
}

void audio_volume(int level) {
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    if (device != 0) SDL_LockAudioDevice(device);
    master_volume = (float)level / 14.0f;
    if (device != 0) SDL_UnlockAudioDevice(device);
}

void audio_pause(void) {
    if (device != 0) SDL_LockAudioDevice(device);
    mixing_paused = true;
    if (device != 0) SDL_UnlockAudioDevice(device);
}

void audio_resume(void) {
    if (device != 0) SDL_LockAudioDevice(device);
    mixing_paused = false;
    if (device != 0) SDL_UnlockAudioDevice(device);
}

void host_audio_shutdown(void) {
    if (device != 0) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}
