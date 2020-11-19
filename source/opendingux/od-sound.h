#ifndef GCWSOUND_H_
#define GCWSOUND_H_

void init_sdlaudio();

// Modified by the audio thread to match VideoFastForwarded after it finds
// that the video thread has skipped a frame. The audio thread must deal
// with the condition as soon as possible.
extern volatile unsigned int AudioFastForwarded;

// User fast forward volume settings
// 0: 100%
// 1: 50%
// 2: 25%
// 3: Mute
extern uint32_t PerGameFastForwardVolume;
extern uint32_t FastForwardVolume;
// A 'thread safe' copy of the the above user
// setting (updated in draw.c)
extern volatile unsigned int FastForwardVolumeLevel;

// OUTPUT_SOUND_FREQUENCY should be a power-of-2 fraction of SOUND_FREQUENCY;
// if not, gcwsound.c's feed_buffer() needs to resample the output.
#define OUTPUT_SOUND_FREQUENCY 44100

#ifdef RG350
#define AUDIO_OUTPUT_BUFFER_SIZE 1024
#else
#define AUDIO_OUTPUT_BUFFER_SIZE 2048
#endif

#define OUTPUT_FREQUENCY_DIVISOR ((int) (SOUND_FREQUENCY) / (OUTPUT_SOUND_FREQUENCY))

#endif /* GCWSOUND_H_ */
