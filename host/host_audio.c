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
    float frequency_step;
    float envelope;
    float attack_step;
    float release_step;
    float left_gain;
    float right_gain;
    int waveform;
    int64_t samples_left;
    int64_t attack_samples_left;
    int64_t release_samples;
    uint16_t noise_lfsr;
    uint8_t noise_index;
    bool active;
} HostVoice;

static SDL_AudioDeviceID device;
static HostVoice voices[AUDIO_NUM_CHANNELS];
static float master_volume = 0.35f;
static bool mixing_paused;

static float voice_sample(HostVoice *voice, float *envelope) {
    if (voice->attack_samples_left > 0) {
        voice->envelope += voice->attack_step;
        --voice->attack_samples_left;
        if (voice->attack_samples_left == 0 || voice->envelope > 1.0f) {
            voice->envelope = 1.0f;
        }
    } else if (voice->samples_left > 0 && voice->release_samples > 0 &&
               voice->samples_left <= voice->release_samples) {
        voice->envelope -= voice->release_step;
        if (voice->envelope < 0.0f) voice->envelope = 0.0f;
    }
    *envelope = voice->envelope;

    const uint8_t phase_index = (uint8_t)(voice->phase * 256.0f);
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
            if (phase_index != voice->noise_index) {
                const uint16_t bit = ((voice->noise_lfsr >> 0) ^
                                      (voice->noise_lfsr >> 1) ^
                                      (voice->noise_lfsr >> 5) ^
                                      (voice->noise_lfsr >> 6)) & 1u;
                voice->noise_lfsr = (voice->noise_lfsr >> 1) | (bit << 15);
                voice->noise_index = phase_index;
            }
            sample = ((float)voice->noise_lfsr / 32767.5f) - 1.0f;
            break;
        case WAVE_SQUARE:
        default:
            sample = voice->phase < 0.5f ? 1.0f : -1.0f;
            break;
    }

    voice->phase += voice->frequency / (float)HOST_SAMPLE_RATE;
    voice->phase -= floorf(voice->phase);
    voice->frequency += voice->frequency_step;
    if (voice->samples_left > 0 && --voice->samples_left == 0) {
        voice->active = false;
    }
    return sample;
}

static float soft_clip(float sample) {
    const float knee = 0.75f;
    float magnitude = fabsf(sample);
    if (magnitude > knee) magnitude = knee + (magnitude - knee) * 0.25f;
    if (magnitude > 1.0f) magnitude = 1.0f;
    return sample < 0.0f ? -magnitude : magnitude;
}

static void audio_callback(void *userdata, Uint8 *stream, int length) {
    (void)userdata;
    float *output = (float *)stream;
    const int frames = length / ((int)sizeof(float) * 2);
    memset(stream, 0, (size_t)length);
    if (mixing_paused) {
        return;
    }

    for (int i = 0; i < frames; ++i) {
        float mixed_left = 0.0f;
        float mixed_right = 0.0f;
        for (int channel = 0; channel < AUDIO_NUM_CHANNELS; ++channel) {
            if (voices[channel].active) {
                float envelope;
                const float sample = voice_sample(&voices[channel], &envelope) * envelope;
                mixed_left += sample * voices[channel].left_gain;
                mixed_right += sample * voices[channel].right_gain;
            }
        }
        output[i * 2] = soft_clip(mixed_left) * master_volume;
        output[i * 2 + 1] = soft_clip(mixed_right) * master_volume;
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
    wanted.channels = 2;
    wanted.samples = 512;
    wanted.callback = audio_callback;

    SDL_AudioSpec obtained;
    SDL_zero(obtained);
    device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
    if (device == 0 || obtained.format != AUDIO_F32SYS || obtained.channels != 2) {
        if (device != 0) {
            SDL_CloseAudioDevice(device);
            device = 0;
        }
        return false;
    }

    memset(voices, 0, sizeof(voices));
    for (int i = 0; i < AUDIO_NUM_CHANNELS; ++i) {
        voices[i].noise_lfsr = (uint16_t)(0xACE1u + (uint16_t)i * 0x1111u);
        voices[i].noise_index = 0xff;
    }
    SDL_PauseAudioDevice(device, 0);
    return true;
}

void audio_tone(int channel, float frequency, int duration_ms, int waveform) {
    audio_tone_ex(channel, frequency, frequency, duration_ms, waveform,
                  AUDIO_VOLUME_MAX, AUDIO_PAN_CENTER, 0, 0);
}

static int64_t milliseconds_to_samples(int milliseconds) {
    if (milliseconds <= 0) return 0;
    int64_t samples = ((int64_t)milliseconds * HOST_SAMPLE_RATE) / 1000;
    return samples > 0 ? samples : 1;
}

void audio_tone_ex(int channel, float frequency, float end_frequency,
                   int duration_ms, int waveform, int volume, int pan,
                   int attack_ms, int release_ms) {
    if (device == 0 || channel < 0 || channel >= AUDIO_NUM_CHANNELS || frequency <= 0.0f) {
        return;
    }
    if (end_frequency <= 0.0f) end_frequency = frequency;
    if (waveform < 0 || waveform > WAVE_NOISE) waveform = WAVE_SQUARE;
    if (volume < 0) volume = 0;
    if (volume > AUDIO_VOLUME_MAX) volume = AUDIO_VOLUME_MAX;
    if (pan < AUDIO_PAN_LEFT) pan = AUDIO_PAN_LEFT;
    if (pan > AUDIO_PAN_RIGHT) pan = AUDIO_PAN_RIGHT;

    const int64_t total_samples = milliseconds_to_samples(duration_ms);
    int64_t attack_samples = milliseconds_to_samples(attack_ms);
    int64_t release_samples = milliseconds_to_samples(release_ms);
    if (total_samples == 0) {
        release_samples = 0;
        end_frequency = frequency;
    } else {
        const int64_t articulation_samples = attack_samples + release_samples;
        if (articulation_samples > total_samples) {
            attack_samples = attack_samples * total_samples / articulation_samples;
            release_samples = total_samples - attack_samples;
        }
    }

    float left_gain = (float)volume / (float)AUDIO_VOLUME_MAX;
    float right_gain = left_gain;
    if (pan > 0) {
        left_gain *= (float)(AUDIO_PAN_RIGHT - pan) / (float)AUDIO_PAN_RIGHT;
    } else if (pan < 0) {
        right_gain *= (float)(pan - AUDIO_PAN_LEFT) / (float)AUDIO_PAN_RIGHT;
    }

    SDL_LockAudioDevice(device);
    HostVoice *voice = &voices[channel];
    voice->active = false;
    voice->phase = 0.0f;
    voice->frequency = frequency;
    voice->frequency_step = total_samples > 1
        ? (end_frequency - frequency) / (float)(total_samples - 1) : 0.0f;
    voice->envelope = attack_samples > 0 ? 0.0f : 1.0f;
    voice->attack_step = attack_samples > 0 ? 1.0f / (float)attack_samples : 0.0f;
    voice->release_step = release_samples > 0
        ? 1.0f / (float)(release_samples + 1) : 0.0f;
    voice->left_gain = left_gain;
    voice->right_gain = right_gain;
    voice->waveform = waveform;
    voice->samples_left = total_samples > 0 ? total_samples : -1;
    voice->attack_samples_left = attack_samples;
    voice->release_samples = release_samples;
    voice->noise_lfsr = 0xACE1;
    voice->noise_index = 0xff;
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
