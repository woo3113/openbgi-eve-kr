/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * movieplayer.c - MPEG-1 (.mpg) playback for the scripts' movie commands,
 *                 on pl_mpeg (public domain, https://github.com/phoboslab/
 *                 pl_mpeg, dropped into src/gfx).  Interface:
 *                 bgi/gfx/movieplayer.h.
 *
 * Two forms of the original's video are covered:
 *
 *  - "92 F2" video textures: a movie attached to a numbered bitmap slot,
 *    decoded into the slot's pixels so the game's own compositing and
 *    moviemngr._bp, which polls the slot, see the frames.
 *
 *  - "92 F0" movie_play_arc: a movie out of an archive shown in a rectangle
 *    of the picture.  The original hands it to DirectShow in a child
 *    window; here it is decoded into the back buffer and presented
 *    directly, since the scripts hold presenting off ("90 01" 0) while the
 *    movie runs.  Playback is asynchronous: `movie_play_arc` starts it and
 *    the script waits on `movie_playing` ("90 F2") / stops with
 *    `movie_stop` ("90 F1"), exactly as the original's scripts do.
 *
 * pl_mpeg hands decoded frames to a callback as Y/Cr/Cb planes; every
 * decode below converts them to RGB through plm_frame_to_rgb into the
 * movie's own buffer.
 */
#include "bgi/gfx/movieplayer.h"
#include "bgi/gfx/bmpmgr.h"
#include "bgi/gfx/bitmap.h"
#include "bgi/gfx/gfxmgr.h"
#include "bgi/display.h"
#include "bgi/file.h"
#include "bgi/os.h"
#include "bgi/msg.h"
#include "bgi/common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#define MOVIE_SLOTS 8

// ---- a movie and its decoded RGB frame ---------------------------------------

typedef struct MovieBuf
{
	plm_t* plm;
	uint8_t* data;
	uint8_t* frame; // the last decoded frame, RGB, vw*vh*3
	int vw, vh;
	int dirty;
} MovieBuf_t;

// pl_mpeg's decoded frame: convert it to RGB into the movie's buffer
static void MovieBuf_OnVideo(plm_t* plm, plm_frame_t* frame, void* user)
{
	MovieBuf_t* b = (MovieBuf_t*)user;
	(void)plm;
	if(frame->width != (unsigned)b->vw || frame->height != (unsigned)b->vh)
	{
		free(b->frame);
		b->frame = NULL;
		b->vw = (int)frame->width;
		b->vh = (int)frame->height;
	}
	if(!b->frame)
		b->frame = (uint8_t*)malloc((size_t)b->vw * b->vh * 3);
	if(b->frame)
	{
		plm_frame_to_rgb(frame, b->frame, b->vw * 3);
		b->dirty = 1;
	}
}

// load `name` of `arc` and prepare it; 0 ok, 1 could not be opened
static int MovieBuf_Open(MovieBuf_t* b, const char* arc, const char* name)
{
	uint32_t size;
	memset(b, 0, sizeof *b);
	size = GetFileSize_(arc, name);
	if(size == 0)
	{
		fprintf(stderr, "[MOVIE] open fail: size 0 (arc=%s name=%s)\n", arc ? arc : "(file)", name ? name : "");
		return 1;
	}
	b->data = (uint8_t*)malloc(size);
	if(!b->data)
		return 1;
	if(LoadFile(b->data, arc, name) == 0)
	{
		fprintf(stderr, "[MOVIE] open fail: load (arc=%s name=%s size=%u)\n", arc ? arc : "(file)", name ? name : "", (unsigned)size);
		free(b->data);
		b->data = NULL;
		return 1;
	}
	b->plm = plm_create_with_memory(b->data, (int)size, 0);
	if(!b->plm)
	{
		fprintf(stderr, "[MOVIE] open fail: plm (name=%s size=%u)\n", name ? name : "", (unsigned)size);
		free(b->data);
		b->data = NULL;
		return 1;
	}
	plm_set_audio_enabled(b->plm, 0);
	plm_set_video_decode_callback(b->plm, MovieBuf_OnVideo, b);
	b->vw = plm_get_width(b->plm);
	b->vh = plm_get_height(b->plm);
	if(b->vw <= 0 || b->vh <= 0)
	{
		fprintf(stderr, "[MOVIE] open fail: size %dx%d (name=%s)\n", b->vw, b->vh, name ? name : "");
		plm_destroy(b->plm);
		free(b->data);
		memset(b, 0, sizeof *b);
		return 1;
	}
	return 0;
}

static void MovieBuf_Close(MovieBuf_t* b)
{
	if(b->plm)
		plm_destroy(b->plm);
	free(b->data);
	free(b->frame);
	memset(b, 0, sizeof *b);
}

// decode the frames due by `elapsedMs`; 1 while it still runs, 0 when it ended
static int MovieBuf_Step(MovieBuf_t* b, double elapsedMs, double* doneMs)
{
	if(elapsedMs > *doneMs)
	{
		plm_decode(b->plm, (elapsedMs - *doneMs) / 1000.0);
		*doneMs = elapsedMs;
	}
	return !plm_has_ended(b->plm);
}

// the movie's duration in ms (0 when unknown)
static int32_t MovieBuf_Duration(MovieBuf_t* b)
{
	double d = plm_get_duration(b->plm);
	return d > 0.0 ? (int32_t)(d * 1000.0) : 0;
}

// ---- movie audio: decoded by pl_mpeg, mixed into the engine's mixer ----------

#define MOVIE_RING_FRAMES 65536 // ~1.5 s at 44.1 kHz
static int16_t gRing[MOVIE_RING_FRAMES * 2];
static volatile int gRingHead; // producer writes here (the VM thread)
static volatile int gRingTail; // mixer reads here (the audio thread)
static uint32_t gMoviePos;     // 16.16 position within the queued samples
static uint32_t gMovieStep = 0x10000; // source frames per output frame

static void RingReset(void)
{
	gRingHead = gRingTail = 0;
	gMoviePos = 0;
	gMovieStep = 0x10000;
}

static void RingPush(int16_t l, int16_t r)
{
	int next = (gRingHead + 1) % MOVIE_RING_FRAMES;
	if(next == gRingTail)
		return; // full: drop the sample
	gRing[gRingHead * 2] = l;
	gRing[gRingHead * 2 + 1] = r;
	gRingHead = next;
}

// pl_mpeg's decoded audio: queue it as stereo 16-bit at the stream's rate
static void MovieBuf_OnAudio(plm_t* plm, plm_samples_t* s, void* user)
{
	unsigned int i;
	(void)user;
	if(gMovieStep == 0x10000)
	{
		int rate = plm_get_samplerate(plm);
		if(rate > 0)
			gMovieStep = (uint32_t)(((uint64_t)rate << 16) / 44100u);
	}
	for(i = 0; i < s->count; i++)
	{
		float l = s->interleaved[i * 2 + 0];
		float r = s->interleaved[i * 2 + 1];
		RingPush((int16_t)(l < -1.f ? -32768 : l > 0.999f ? 32767 : (int)(l * 32767.f)),
			(int16_t)(r < -1.f ? -32768 : r > 0.999f ? 32767 : (int)(r * 32767.f)));
	}
}

/* Add the queued movie audio into the mixer's 32-bit stereo accumulator
 * (audio thread; 44.1 kHz).  A 16.16 step resamples a stream of another
 * rate, the way the voices do.  Nothing is added while the queue is empty. */
void Movie_MixAudio(int32_t* acc, int frames)
{
	int i;
	for(i = 0; i < frames; i++)
	{
		int a = gRingTail, b;
		uint32_t f;
		int32_t l, r;
		if(a == gRingHead)
			break; // underrun: silence
		b = (a + 1) % MOVIE_RING_FRAMES;
		f = gMoviePos;
		if(b == gRingHead)
			f = 0; // no next sample to interpolate towards
		l = ((int32_t)gRing[a * 2 + 0] * (int32_t)(0x10000 - f) + (int32_t)gRing[b * 2 + 0] * (int32_t)f) >> 16;
		r = ((int32_t)gRing[a * 2 + 1] * (int32_t)(0x10000 - f) + (int32_t)gRing[b * 2 + 1] * (int32_t)f) >> 16;
		acc[i * 2 + 0] += l;
		acc[i * 2 + 1] += r;
		gMoviePos += gMovieStep;
		while(gMoviePos >= 0x10000)
		{
			gMoviePos -= 0x10000;
			if(gRingTail == gRingHead)
			{
				gMoviePos = 0;
				break;
			}
			gRingTail = (gRingTail + 1) % MOVIE_RING_FRAMES;
		}
	}
}

// ---- "92 F2" video textures: a movie decoded into a script bitmap slot ------

typedef struct MovieSlot
{
	int used;
	int bmpNo;
	MovieBuf_t buf;
	double startMs;
	double doneMs;
	int playing;
	int paused;
	int ended;
} MovieSlot_t;

static MovieSlot_t gSlots[MOVIE_SLOTS];

static MovieSlot_t* FindByBmp(int bmpNo)
{
	int i;
	for(i = 0; i < MOVIE_SLOTS; i++)
		if(gSlots[i].used && gSlots[i].bmpNo == bmpNo)
			return &gSlots[i];
	return NULL;
}

/* Present a full-screen video texture directly.  The original renders these
 * through DirectShow (a child window over the picture), so the game itself
 * does not have to present; the small video textures (the bust shots) are
 * the game's own to composite and are left alone. */
static void PresentSlotFrame(MovieSlot_t* m)
{
	Bmp_t back, sb;
	int x, y;
	if(!gGfx || !BmpMgr_GetInfo(gBmpMgr, &sb, m->bmpNo))
		return;
	Gfx_CopyBackBmp(gGfx, &back);
	if(sb.w < back.w || sb.h < back.h)
		return;
	for(y = 0; y < back.h; y++)
	{
		uint32_t* d = (uint32_t*)(back.pixels + (size_t)y * back.pitch);
		const uint32_t* srow = (const uint32_t*)(sb.pixels + (size_t)((int)((int64_t)y * sb.h / back.h)) * sb.pitch);
		for(x = 0; x < back.w; x++)
			d[x] = srow[(int)((int64_t)x * sb.w / back.w)];
	}
	Gfx_UpdateScaled(gGfx, 0, NULL);
	Window_Repaint();
}

static void WriteSlotFrame(MovieSlot_t* m)
{
	Bmp_t b;
	const uint8_t* s;
	int x, y;
	if(!m->buf.frame || !m->buf.dirty)
		return;
	if(!BmpMgr_GetInfo(gBmpMgr, &b, m->bmpNo))
		return;
	s = m->buf.frame;
	for(y = 0; y < m->buf.vh && y < b.h; y++)
	{
		uint32_t* d = (uint32_t*)(b.pixels + (size_t)y * b.pitch);
		for(x = 0; x < m->buf.vw && x < b.w; x++, s += 3)
			d[x] = (uint32_t)s[2] | (uint32_t)s[1] << 8 | (uint32_t)s[0] << 16;
		if(x < m->buf.vw)
			s += (size_t)(m->buf.vw - x) * 3;
	}
	m->buf.dirty = 0;
	PresentSlotFrame(m);
}

int Movie_Attach(const char* arc, const char* name, int bmpNo)
{
	MovieSlot_t* m = NULL;
	int i;
	Movie_Release(bmpNo); // a slot already showing a movie there is replaced
	for(i = 0; i < MOVIE_SLOTS; i++)
		if(!gSlots[i].used)
		{
			m = &gSlots[i];
			break;
		}
	if(!m)
		return 1;
	memset(m, 0, sizeof *m);
	if(MovieBuf_Open(&m->buf, arc, name))
		return 1;
	plm_set_audio_enabled(m->buf.plm, 1);
	plm_set_audio_decode_callback(m->buf.plm, MovieBuf_OnAudio, NULL);
	RingReset();
	plm_decode(m->buf.plm, 0.0); // the first frame
	BmpMgr_Create(gBmpMgr, bmpNo, m->buf.vw, m->buf.vh, PM_RGB32);
	m->used = 1;
	m->bmpNo = bmpNo;
	m->startMs = (double)OS_TicksMs();
	m->doneMs = 0.0;
	m->playing = 1;
	m->ended = 0;
	WriteSlotFrame(m);
	fprintf(stderr, "[MOVIE] attach %s/%s bmp=%d %dx%d\n",
		arc ? arc : "", name ? name : "", bmpNo, m->buf.vw, m->buf.vh);
	return 0;
}

int Movie_Control(int bmpNo, int cmd)
{
	MovieSlot_t* m = FindByBmp(bmpNo);
	if(!m)
		return 1;
	switch(cmd)
	{
	case 0:
		return m->ended ? 1 : 0;
	case 1:
		if(m->ended)
		{
			m->startMs = (double)OS_TicksMs();
			m->doneMs = 0.0;
			m->ended = 0;
		}
		m->paused = 0;
		m->playing = 1;
		break;
	case 2:
		m->paused = 1;
		break;
	case 3:
		m->playing = 0;
		m->ended = 1;
		break;
	default:
		break;
	}
	return m->ended ? 1 : 0;
}

int Movie_Status(int bmpNo, int32_t* out)
{
	MovieSlot_t* m = FindByBmp(bmpNo);
	if(!m)
	{
		if(out)
			*out = -1;
		return 0;
	}
	if(out)
		*out = (int32_t)m->doneMs;
	return 1;
}

/* the "92 F2" player's length in ms (read by "91 F1" video_texture_state);
 * 0 when the slot has no movie */
int32_t Movie_TextureLength(int bmpNo)
{
	MovieSlot_t* m = FindByBmp(bmpNo);
	return m ? MovieBuf_Duration(&m->buf) : 0;
}

/* "91 F2" / "91 F6" video_texture_release: drop the movie attached to `bmpNo`,
 * so the player and its slots are freed for the next attach */
void Movie_Release(int bmpNo)
{
	int i;
	for(i = 0; i < MOVIE_SLOTS; i++)
		if(gSlots[i].used && gSlots[i].bmpNo == bmpNo)
		{
			MovieBuf_Close(&gSlots[i].buf);
			memset(&gSlots[i], 0, sizeof gSlots[i]);
		}
}

// ---- "92 F0" movie_play_arc: a movie shown in a rectangle of the picture ----

typedef struct ArcMovie
{
	int used;
	MovieBuf_t buf;
	int x, y, w, h;
	double startMs;
	double doneMs;
	int playing;
} ArcMovie_t;

static ArcMovie_t gArc;

// copy the current frame into the back buffer and present it
static void BlitArcFrame(void)
{
	Bmp_t back;
	const uint8_t* src;
	int x, y;

	src = gArc.buf.frame;
	if(!src || !gGfx)
		return;
	Gfx_CopyBackBmp(gGfx, &back);
	for(y = 0; y < gArc.h; y++)
	{
		int dy = gArc.y + y, sy;
		uint8_t* row;
		if(dy < 0 || dy >= back.h)
			continue;
		sy = (int)((int64_t)y * gArc.buf.vh / gArc.h);
		if(sy >= gArc.buf.vh)
			sy = gArc.buf.vh - 1;
		row = back.pixels + (size_t)dy * back.pitch;
		for(x = 0; x < gArc.w; x++)
		{
			int dx = gArc.x + x, sx;
			const uint8_t* p;
			if(dx < 0 || dx >= back.w)
				continue;
			sx = (int)((int64_t)x * gArc.buf.vw / gArc.w);
			if(sx >= gArc.buf.vw)
				sx = gArc.buf.vw - 1;
			p = src + ((size_t)sy * gArc.buf.vw + sx) * 3;
			if(back.mode == PM_RGB16)
				((uint16_t*)row)[dx] = (uint16_t)(((p[0] >> 3) << 10) | ((p[1] >> 3) << 5) | (p[2] >> 3));
			else
				((uint32_t*)row)[dx] = (uint32_t)p[2] | (uint32_t)p[1] << 8 | (uint32_t)p[0] << 16;
		}
	}
	Gfx_UpdateScaled(gGfx, 0, NULL); // stretch into the scaled surface, when there is one
	Window_Repaint();                // copy it to the window (no compositor pass)
}

void Movie_ArcStop(void)
{
	MovieBuf_Close(&gArc.buf);
	memset(&gArc, 0, sizeof gArc);
	RingReset();
}

int Movie_ArcIsPlaying(void)
{
	return gArc.used && gArc.playing;
}

// prepare the open movie's audio and start it in the (x, y, w, h) rectangle
static int32_t StartArcPlayback(int x, int y, int w, int h)
{
	int32_t length;
	plm_set_audio_enabled(gArc.buf.plm, 1);
	plm_set_audio_decode_callback(gArc.buf.plm, MovieBuf_OnAudio, NULL);
	RingReset();
	if(w <= 0)
		w = gArc.buf.vw;
	if(h <= 0)
		h = gArc.buf.vh;
	gArc.x = x;
	gArc.y = y;
	gArc.w = w;
	gArc.h = h;
	plm_decode(gArc.buf.plm, 0.0); // the first frame
	BlitArcFrame();
	gArc.startMs = (double)OS_TicksMs();
	gArc.doneMs = 0.0;
	gArc.playing = 1;
	gArc.used = 1;
	length = MovieBuf_Duration(&gArc.buf);
	return length;
}

int32_t Movie_PlayArc(const char* arc, const char* name, int x, int y, int w, int h)
{
	int32_t length;
	Movie_ArcStop();
	if(!name || !*name)
		return 0;
	if(MovieBuf_Open(&gArc.buf, arc, name))
		return 0;
	length = StartArcPlayback(x, y, w, h);
	fprintf(stderr, "[MOVIE] arc %s/%s %dx%d dur=%dms rect=%d,%d,%dx%d\n",
		arc ? arc : "", name, gArc.buf.vw, gArc.buf.vh, length, x, y, w, h);
	return length;
}

/* "90 F0" movie_play as a fallback for a host without DirectShow: the movie
 * is a loose file (searched like any other), or a member of the data01xxx /
 * data02xxx archives. */
int32_t Movie_PlayFile(const char* path, int x, int y, int w, int h)
{
	int32_t length;
	Movie_ArcStop();
	if(!path || !*path)
		return 0;
	if(MovieBuf_Open(&gArc.buf, NULL, path) && MovieBuf_Open(&gArc.buf, "data01xxx.arc", path) &&
		MovieBuf_Open(&gArc.buf, "data02xxx.arc", path))
		return 0;
	length = StartArcPlayback(x, y, w, h);
	fprintf(stderr, "[MOVIE] file %s %dx%d dur=%dms\n", path, gArc.buf.vw, gArc.buf.vh, length);
	return length;
}

// ---- the engine frame step ---------------------------------------------------

void Movie_Pump(void)
{
	double now = (double)OS_TicksMs();
	int i;

	for(i = 0; i < MOVIE_SLOTS; i++)
	{
		MovieSlot_t* m = &gSlots[i];
		if(!m->used || !m->playing || m->paused || m->ended)
			continue;
		if(!MovieBuf_Step(&m->buf, now - m->startMs, &m->doneMs))
			m->ended = 1;
		else
			WriteSlotFrame(m);
	}

	if(gArc.used && gArc.playing)
	{
		if(!MovieBuf_Step(&gArc.buf, now - gArc.startMs, &gArc.doneMs))
			gArc.playing = 0;
		else if(gArc.buf.dirty)
			BlitArcFrame();
	}
}

void Movie_Shutdown(void)
{
	int i;
	Movie_ArcStop();
	for(i = 0; i < MOVIE_SLOTS; i++)
	{
		if(gSlots[i].used)
			MovieBuf_Close(&gSlots[i].buf);
	}
	memset(gSlots, 0, sizeof gSlots);
}
