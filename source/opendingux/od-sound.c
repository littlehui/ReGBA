#include <SDL/SDL.h>

#include "common.h"
#include "../sound.h"

volatile unsigned int AudioFastForwarded;

uint32_t PerGameFastForwardVolume            = 0;
uint32_t FastForwardVolume                   = 0;
volatile unsigned int FastForwardVolumeLevel = 0;

#ifdef RG350
#define DAMPEN_SAMPLE_COUNT (AUDIO_OUTPUT_BUFFER_SIZE >> 4)
#else
#define DAMPEN_SAMPLE_COUNT (AUDIO_OUTPUT_BUFFER_SIZE >> 5)
#endif

#ifdef SOUND_TO_FILE
FILE* WaveFile;
#endif

static inline void RenderSample(int16_t* Left, int16_t* Right)
{
	int16_t LeftPart, RightPart;
	uint32_t j;
	for (j = 0; j < OUTPUT_FREQUENCY_DIVISOR; j++) {
		ReGBA_LoadNextAudioSample(&LeftPart, &RightPart);

		/* The GBA outputs in 12-bit sound. Make it louder. */
		if      (LeftPart >  2047) LeftPart =  2047;
		else if (LeftPart < -2048) LeftPart = -2048;
		*Left += LeftPart / OUTPUT_FREQUENCY_DIVISOR;

		if      (RightPart >  2047) RightPart =  2047;
		else if (RightPart < -2048) RightPart = -2048;
		*Right += RightPart / OUTPUT_FREQUENCY_DIVISOR;
	}
}

void feed_buffer(void *udata, Uint8 *buffer, int len)
{
	s16* stream = (s16*) buffer;
	u32 Samples = ReGBA_GetAudioSamplesAvailable() / OUTPUT_FREQUENCY_DIVISOR;
	u32 Requested = len / (2 * sizeof(s16));

	u8 WasInUnderrun = Stats.InSoundBufferUnderrun;
	Stats.InSoundBufferUnderrun = Samples < Requested * 2;
	if (Stats.InSoundBufferUnderrun && !WasInUnderrun)
		Stats.SoundBufferUnderrunCount++;

	/* There must be AUDIO_OUTPUT_BUFFER_SIZE * 2 samples generated in order
	 * for the first AUDIO_OUTPUT_BUFFER_SIZE to be valid. Some sound is
	 * generated in the past from the future, and if the first
	 * AUDIO_OUTPUT_BUFFER_SIZE is grabbed before the core has had time to
	 * generate all of it (at AUDIO_OUTPUT_BUFFER_SIZE * 2), the end may
	 * still be silence, causing crackling. */
	if (Samples < Requested * 2)
		return; // Generate more sound first, please!
	else
		Stats.InSoundBufferUnderrun = 0;
		
	s16* Next = stream;

	// Take the first half of the sound.
	uint32_t i;
	for (i = 0; i < Requested / 2; i++)
	{
		s16 Left = 0, Right = 0;
		RenderSample(&Left, &Right);

		*Next++ = Left  << 4;
		*Next++ = Right << 4;
	}
	Samples -= Requested / 2;
	
	// Discard as many samples as are generated in 1 frame, if fast-forwarding.
	bool Skipped = false;
	unsigned int VideoFastForwardedCopy = VideoFastForwarded;
	if (VideoFastForwardedCopy != AudioFastForwarded)
	{
		int32_t SamplesToSkip = (int32_t)Samples - (Requested * 3 - Requested / 2);
		if (SamplesToSkip > 0)
		{
			ReGBA_DiscardAudioSamples(SamplesToSkip * OUTPUT_FREQUENCY_DIVISOR);
			Samples -= SamplesToSkip;
		}
		AudioFastForwarded = VideoFastForwardedCopy;
		Skipped = true;
	}

	// Take the second half of the sound now.
	for (i = 0; i < Requested - Requested / 2; i++)
	{
		s16 Left = 0, Right = 0;
		RenderSample(&Left, &Right);

		*Next++ = Left  << 4;
		*Next++ = Right << 4;
	}
	Samples -= Requested - Requested / 2;

	/* Check whether audio samples have been skipped */
	if (Skipped)
	{
		/* FastForwardVolumeLevel:
		 * 0: 100%
		 * 1: 50%
		 * 2: 25%
		 * 3: Mute
		 */
		unsigned int FastForwardVolumeLevelCopy = FastForwardVolumeLevel;

		if (FastForwardVolumeLevelCopy == 3)
			memset(stream, 0, len);
		else
		{
			/* Dampen the transition between the two halves */
			uint32_t dampen_sample_count_max = (Requested / 4) - 2;
			uint32_t dampen_sample_count     = (DAMPEN_SAMPLE_COUNT < dampen_sample_count_max) ?
					DAMPEN_SAMPLE_COUNT : dampen_sample_count_max;

			for (i = 0; i < dampen_sample_count; i++)
			{
				uint_fast8_t j;
				for (j = 0; j < 2; j++)
				{
					stream[Requested / 2 + i * 2 + j] = (int16_t) (
						  (int32_t) stream[Requested / 2 - i * 2 - 2 + j] * (1 + (dampen_sample_count - (int32_t) i) / (dampen_sample_count + 1))
						+ (int32_t) stream[Requested / 2 + i * 2 + j] * (1 + ((int32_t) i + 1) / (dampen_sample_count + 1))
						);
				}
			}

			/* Attenuate overall volume, if required */
			if (FastForwardVolumeLevelCopy > 0)
			{
				uint_fast8_t attenuation_shift = (FastForwardVolumeLevelCopy < 3) ?
						FastForwardVolumeLevelCopy : 2;

				for (i = 0; i < Requested << 1; i++)
					stream[i] = stream[i] >> attenuation_shift;
			}
		}
	}
}

void init_sdlaudio()
{
	SDL_AudioSpec spec;

	spec.freq = OUTPUT_SOUND_FREQUENCY;
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.samples = AUDIO_OUTPUT_BUFFER_SIZE;
	spec.callback = feed_buffer;
	spec.userdata = NULL;

	if (SDL_OpenAudio(&spec, NULL) < 0) {
		ReGBA_Trace("E: Failed to open audio: %s", SDL_GetError());
		return;
	}

	SDL_PauseAudio(0);
}

signed int ReGBA_AudioUpdate()
{
	return 0;
}
