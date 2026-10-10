/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * audio.c - the Android back end's audio output (OS_AudioStart / OS_AudioStop
 *           / OS_AudioAvailable of inc/bgi/os.h), on SDL2.
 *
 * The engine mixes in software and hands interleaved stereo 16-bit frames to
 * the pull callback; SDL opens the device of the requested rate and calls it
 * from SDL's audio thread, exactly the way the contract in os.h describes.
 * CDs (MCI) and the .wav jingle are the stubs of stubs.c.
 */
#include "android_internal.h"
#include <SDL.h>

static SDL_AudioDeviceID gDev;
static OsAudioFill_t gFill;
static void* gUser;
static int gRate;

static void SdlAudioCallback(void* user, Uint8* stream, int len)
{
	BGI_UNUSED(user);
	if(gFill)
		gFill(gUser, (int16_t*)stream, len / 4); // 2 channels * 2 bytes
	else
		memset(stream, 0, (size_t)len);
}

int OS_AudioAvailable(void)
{
	return 1; // SDL always offers an output (or a silent one)
}

int OS_AudioStart(int rate, OsAudioFill_t fill, void* user)
{
	SDL_AudioSpec want, have;
	if(rate <= 0)
		rate = 44100;
	if(SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
		return 0;
	memset(&want, 0, sizeof want);
	want.freq = rate;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = (Uint16)(rate / 50); // ~20 ms
	want.callback = SdlAudioCallback;
	gFill = fill;
	gUser = user;
	gRate = rate;
	gDev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if(!gDev)
		return 0;
	SDL_PauseAudioDevice(gDev, 0);
	return 1;
}

void OS_AudioStop(void)
{
	if(gDev)
	{
		SDL_PauseAudioDevice(gDev, 1);
		SDL_CloseAudioDevice(gDev);
		gDev = 0;
	}
	gFill = NULL;
	gUser = NULL;
}
