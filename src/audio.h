#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Waveform types
#define WAVE_SINE     0
#define WAVE_SQUARE   1
#define WAVE_SAW      2
#define WAVE_TRIANGLE 3
#define WAVE_NOISE    4

#define AUDIO_NUM_CHANNELS 8

#define AUDIO_VOLUME_MAX 255
#define AUDIO_PAN_LEFT   (-127)
#define AUDIO_PAN_CENTER 0
#define AUDIO_PAN_RIGHT  127

// Initialize codec (I2C), I2S (PIO), and DMA. Call once after DVI init.
bool audio_init(void);

// Play a tone on a synth channel (0-7).
// freq: Hz, duration_ms: 0 = infinite, waveform: WAVE_* constant
void audio_tone(int channel, float freq, int duration_ms, int waveform);

// Play an articulated stereo tone on a synth channel (0-7).
// frequency and end_frequency are in Hz. The pitch slides linearly from the
// start frequency to the end frequency over duration_ms; pass the same value
// for both to hold a constant pitch. A non-positive end_frequency also holds
// the starting pitch.
//
// duration_ms includes the attack and release. A duration of 0 sustains until
// stopped (release and pitch slide are then ignored). volume is 0..255, pan is
// -127 (left) through 0 (center) to 127 (right), and attack_ms/release_ms are
// clamped to fit inside the note duration.
void audio_tone_ex(int channel, float frequency, float end_frequency,
                   int duration_ms, int waveform, int volume, int pan,
                   int attack_ms, int release_ms);

// Stop a channel, or all channels if channel < 0.
void audio_stop(int channel);

// Set DAC volume level (0-7). Writes to codec registers via I2C.
void audio_volume(int level);

// Pause/resume audio mixing. While paused, the DMA ISR outputs silence.
void audio_pause(void);
void audio_resume(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_H
