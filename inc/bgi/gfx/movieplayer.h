/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * movieplayer.h - MPEG-1 movie playback for the script video textures
 *                 ("92 F0" .. "92 F4", "92 F5"), implemented on pl_mpeg.
 *                 What the original does through DirectShow; see
 *                 src/gfx/movieplayer.c.
 */
#ifndef BGI_GFX_MOVIEPLAYER_H_
#define BGI_GFX_MOVIEPLAYER_H_

#include "bgi/common.h"

/* "92 F2": attach the movie `name` of archive `arc` to bitmap slot `bmpNo`
 * as a video texture; 0 ok, 1 could not be opened/allocated */
int Movie_Attach(const char* arc, const char* name, int bmpNo);
/* "92 F4": control the texture of slot `bmpNo`: 0 reports 1 once the movie
 * has ended (else 0), 1 plays/resumes, 2 pauses, 3 stops/rewinds;
 * any other value just reports the state */
int Movie_Control(int bmpNo, int cmd);
/* "92 F5": write the texture's state (the current frame's time in ms, or -1
 * when the slot has no movie) into *out; 1 when it is a movie slot */
int Movie_Status(int bmpNo, int32_t* out);
/* "91 F1" video_texture_state: the attached movie's length in ms (0 without one) */
int32_t Movie_TextureLength(int bmpNo);
/* "91 F2" / "91 F6" video_texture_release: free the movie attached to `bmpNo` */
void Movie_Release(int bmpNo);
/* "92 F0": play the movie `name` of `arc` in a w x h area, blocking the
 * calling thread until it ends; the length in ms (0 when it could not be
 * played) */
int32_t Movie_PlayArc(const char* arc, const char* name, int x, int y, int w, int h);
/* the "90 F0" movie_play fallback for a host without DirectShow (Android):
 * play `path` (loose file or a data01xxx / data02xxx member) in w x h */
int32_t Movie_PlayFile(const char* path, int x, int y, int w, int h);
/* decode the frames due by now into their bitmaps (the engine frame step) */
void Movie_Pump(void);
/* add the movie's queued audio into the mixer's 32-bit stereo accumulator
 * (called from the audio thread's Mixer_Fill, 44.1 kHz) */
void Movie_MixAudio(int32_t* acc, int frames);
/* the "92 F0" movie: whether it still plays (read by "90 F2" movie_playing)
 * and to stop it (the "90 F1" movie_stop) */
int Movie_ArcIsPlaying(void);
void Movie_ArcStop(void);
void Movie_Shutdown(void);

#endif // BGI_GFX_MOVIEPLAYER_H_
