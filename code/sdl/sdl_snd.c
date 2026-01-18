#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Для MSVC нужна malloc.h для alloca
#ifdef _MSC_VER
#include <malloc.h>
#endif

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL3/SDL.h>
#endif

#include "../qcommon/q_shared.h"
#include "../client/snd_local.h"

qboolean snd_inited = qfalse;

cvar_t* s_sdlBits;
cvar_t* s_sdlSpeed;
cvar_t* s_sdlChannels;
cvar_t* s_sdlDevSamps; // В SDL3 это используется только как подсказка для размера буфера движка
cvar_t* s_sdlMixSamps;

/* SDL3 Audio Stream & Synchronization */
static SDL_AudioStream* audio_stream = NULL;
static SDL_Mutex* audio_mutex = NULL;

static int dmapos = 0;
static int dmasize = 0;

/*
===============
SNDDMA_AudioCallback

In SDL3, the callback is called when the stream needs more data.
We fetch data from the dma.buffer and push it to the stream.
===============
*/
static void SDLCALL SNDDMA_AudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
	// 'additional_amount' is roughly equivalent to 'len' in the old callback
	// It's the minimum amount of data SDL requests right now.
	int len = additional_amount;

	if (!snd_inited || !dma.buffer) {
		// Feed silence if not initialized
		// In SDL3 formats, 0 is silence for signed/float, specific value for unsigned.
		// Usually safer to just put nothing or zeros.
		Uint8* silence = (Uint8*)alloca(len);
		memset(silence, 0, len);
		SDL_PutAudioStreamData(stream, silence, len);
		return;
	}

	int pos = (dmapos * (dma.samplebits / 8));
	if (pos >= dmasize)
		dmapos = pos = 0;

	int tobufend = dmasize - pos;
	int len1 = len;
	int len2 = 0;

	if (len1 > tobufend) {
		len1 = tobufend;
		len2 = len - len1;
	}

	// Push first part
	SDL_PutAudioStreamData(stream, dma.buffer + pos, len1);

	if (len2 <= 0) {
		dmapos += (len1 / (dma.samplebits / 8));
	}
	else {
		// Push wrapped part
		SDL_PutAudioStreamData(stream, dma.buffer, len2);
		dmapos = (len2 / (dma.samplebits / 8));
	}

	if (dmapos >= dmasize)
		dmapos = 0;
}

static struct
{
	SDL_AudioFormat enumFormat;
	const char* stringFormat;
} formatToStringTable[] =
{
	{ SDL_AUDIO_U8,     "SDL_AUDIO_U8" },
	{ SDL_AUDIO_S8,     "SDL_AUDIO_S8" },
	{ SDL_AUDIO_S16LE,  "SDL_AUDIO_S16LE" },
	{ SDL_AUDIO_S16BE,  "SDL_AUDIO_S16BE" },
	{ SDL_AUDIO_S32LE,  "SDL_AUDIO_S32LE" },
	{ SDL_AUDIO_S32BE,  "SDL_AUDIO_S32BE" },
	{ SDL_AUDIO_F32LE,  "SDL_AUDIO_F32LE" },
	{ SDL_AUDIO_F32BE,  "SDL_AUDIO_F32BE" }
};

static int formatToStringTableSize = ARRAY_LEN(formatToStringTable);

/*
===============
SNDDMA_PrintAudiospec
===============
*/
static void SNDDMA_PrintAudiospec(const char* str, const SDL_AudioSpec* spec)
{
	int		i;
	const char* fmt = NULL;

	Com_Printf("%s:\n", str);

	for (i = 0; i < formatToStringTableSize; i++) {
		if (spec->format == formatToStringTable[i].enumFormat) {
			fmt = formatToStringTable[i].stringFormat;
		}
	}

	if (fmt) {
		Com_Printf("  Format:   %s\n", fmt);
	}
	else {
		Com_Printf("  Format:   " S_COLOR_RED "UNKNOWN (0x%x)\n", spec->format);
	}

	Com_Printf("  Freq:     %d\n", (int)spec->freq);
	Com_Printf("  Channels: %d\n", (int)spec->channels);
}

/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init(void)
{
	const char* drivername;
	SDL_AudioSpec desired;
	// In SDL3 obtained spec is retrieved via SDL_GetAudioStreamFormat
	SDL_AudioSpec obtained;
	int tmp;

	if (snd_inited)
		return qtrue;

	if (!s_sdlBits) {
		s_sdlBits = Cvar_Get("s_sdlBits", "16", CVAR_ARCHIVE);
		s_sdlSpeed = Cvar_Get("s_sdlSpeed", "0", CVAR_ARCHIVE);
		s_sdlChannels = Cvar_Get("s_sdlChannels", "2", CVAR_ARCHIVE);
		s_sdlDevSamps = Cvar_Get("s_sdlDevSamps", "0", CVAR_ARCHIVE);
		s_sdlMixSamps = Cvar_Get("s_sdlMixSamps", "0", CVAR_ARCHIVE);
	}

	Com_Printf("SDL_Init( SDL_INIT_AUDIO )... ");

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		if (SDL_Init(SDL_INIT_AUDIO) == -1)
		{
			Com_Printf("FAILED (%s)\n", SDL_GetError());
			return qfalse;
		}
	}

	Com_Printf("OK\n");

	// Create a mutex to replace SDL_LockAudio/SDL_UnlockAudio
	audio_mutex = SDL_CreateMutex();
	if (!audio_mutex) {
		Com_Printf("Failed to create audio mutex: %s\n", SDL_GetError());
		return qfalse;
	}

	drivername = SDL_GetCurrentAudioDriver();
	if (drivername == NULL) {
		drivername = "(UNKNOWN)";
	}
	Com_Printf("SDL audio driver is \"%s\".\n", drivername);

	memset(&desired, '\0', sizeof(desired));
	// Obtained needs to be filled after stream creation
	memset(&obtained, '\0', sizeof(obtained));

	tmp = ((int)s_sdlBits->value);
	if ((tmp != 16) && (tmp != 8))
		tmp = 16;

	desired.freq = (int)s_sdlSpeed->value;
	if (!desired.freq) desired.freq = 22050;

	// SDL3 Format Constants
	desired.format = ((tmp == 16) ? SDL_AUDIO_S16 : SDL_AUDIO_U8);
	desired.channels = (int)s_sdlChannels->value;

	// SDL3: We open a stream attached to the default playback device
	// The callback is provided here.
	audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, SNDDMA_AudioCallback, NULL);

	if (audio_stream == NULL)
	{
		Com_Printf("SDL_OpenAudioDeviceStream() failed: %s\n", SDL_GetError());
		SDL_DestroyMutex(audio_mutex);
		audio_mutex = NULL;
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return qfalse;
	}

	// Get the actual format obtained.
	// В SDL3 функция принимает: (stream, &src_spec, &dst_spec).
	// Нас интересует src_spec (первый указатель), так как это формат, который стрим
	// ожидает от нашего коллбэка (SNDDMA_AudioCallback).
	// Второй указатель (dst_spec) - это формат устройства, его можно передать как NULL, если не интересно.
	if (!SDL_GetAudioStreamFormat(audio_stream, &obtained, NULL)) {
		// Fallback if query fails, assume desired
		obtained = desired;
	}

	SNDDMA_PrintAudiospec("SDL_AudioSpec", &obtained);

	// Calculate buffer size logic (Quake specific)
	tmp = s_sdlMixSamps->value;
	if (!tmp) {
		// Approximate samples based on a safe buffer size since we don't get 'samples' from SDL3 directly
		// Assuming ~10 frames of audio buffering
		int samples_approx = (obtained.freq * obtained.channels) / 10;
		tmp = samples_approx;
	}

	if (tmp & (tmp - 1))  // not a power of two?
	{
		int val = 1;
		while (val < tmp)
			val <<= 1;
		tmp = val;
	}

	dmapos = 0;
	dma.samplebits = SDL_AUDIO_BITSIZE(obtained.format);
	dma.channels = obtained.channels;
	dma.samples = tmp;
	dma.submission_chunk = 1;
	dma.speed = obtained.freq;
	dmasize = (dma.samples * (dma.samplebits / 8));
	dma.buffer = calloc(1, dmasize);

	Com_Printf("Starting SDL audio callback...\n");

	// In SDL3, we must resume the device associated with the stream
	SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audio_stream));

	Com_Printf("SDL audio initialized.\n");
	snd_inited = qtrue;
	return qtrue;
}

/*
===============
SNDDMA_GetDMAPos
===============
*/
int SNDDMA_GetDMAPos(void)
{
	return dmapos;
}

/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown(void)
{
	Com_Printf("Closing SDL audio device...\n");

	if (audio_stream) {
		SDL_DestroyAudioStream(audio_stream);
		audio_stream = NULL;
	}

	if (audio_mutex) {
		SDL_DestroyMutex(audio_mutex);
		audio_mutex = NULL;
	}

	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	if (dma.buffer) {
		free(dma.buffer);
		dma.buffer = NULL;
	}

	dmapos = dmasize = 0;
	snd_inited = qfalse;
	Com_Printf("SDL audio device shut down.\n");
}

/*
===============
SNDDMA_Submit
===============
*/
void SNDDMA_Submit(void)
{
	if (audio_mutex) {
		SDL_UnlockMutex(audio_mutex);
	}
}

/*
===============
SNDDMA_BeginPainting
===============
*/
void SNDDMA_BeginPainting(void)
{
	if (audio_mutex) {
		SDL_LockMutex(audio_mutex);
	}
}