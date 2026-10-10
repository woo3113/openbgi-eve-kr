/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * sjis.c - the Shift-JIS conversions of the Android back end
 *          (OS_SjisToCodePoint / OS_SjisToUtf8 / OS_Utf8ToSjis)
 *
 * Android's libc has no iconv CP932 support, so the base table is generated
 * from Python's cp932 codec into cp932_table.h (scripts/gen_cp932.py).
 *
 * On top of that, the Korean fan patch of EVE burst error A re-encodes the
 * scenario text as double-byte codes whose trail byte is in 0x01 .. 0x3F - a
 * range Shift-JIS never uses - so the original patch's hook (dinput8.dll)
 * can tell them apart and draw a Korean glyph instead.  Its mapping is a
 * linear index into sjis_ext.bin (the Korean characters next to the game),
 * reproduced here from the hook's code (a step of 0x3A per lead and a
 * piecewise adjustment of the trail, minus 0x2B9A).  EVE_BLACK.ttf carries
 * the Korean glyphs at their standard Unicode positions, so the engine's
 * own font renders them once the code point is translated.
 */
#include "bgi/os.h"
#include "bgi/os_common.h"
#include "android_internal.h"
#include "cp932_table.h"

static uint16_t gSjisToUni[0x10000]; // two-byte Shift-JIS -> Unicode, 0 = unmapped (built in SjisInit)

// the Korean patch's extended table (sjis_ext.bin), when the game carries one
static uint16_t* gExt;   // the Korean characters, in the patch's order
static int gExtCount;

// the patch's code -> index into gExt
static int KrIndex(uint8_t L, uint8_t T)
{
	int i1 = (L < 0xA0) ? 64 : 0;
	int edx = (int)T;
	int last, edi;
	edx = edx - 1 + ((T < 0x2D) ? 1 : 0);
	edx = edx - 1 + (((unsigned)edx < 0x21u) ? 1 : 0);
	edx = edx - (edx >= 0x0E ? 1 : 0);
	edx = edx - (edx >= 0x0B ? 1 : 0);
	last = (edx >= 0x0A ? 1 : 0);
	edi = (i1 + (int)L) * 0x3A + edx;
	return edi + (~last) - 0x2BBA;
}

// the Korean character of a patch code, or 0 when it is not one
static uint32_t KrLookup(uint16_t code)
{
	uint8_t L = (uint8_t)(code >> 8), T = (uint8_t)(code & 0xFF);
	int idx;
	if(!gExt)
		return 0;
	if(!((L >= 0x81 && L <= 0x9F) || (L >= 0xE0 && L <= 0xEF)))
		return 0;
	if(T < 0x01 || T > 0x3F)
		return 0;
	idx = KrIndex(L, T);
	if(idx < 0 || idx >= gExtCount || gExt[idx] == 0)
		return 0;
	return gExt[idx];
}

static uint32_t SjisLookup(uint16_t code)
{
	uint32_t kr = KrLookup(code);
	uint16_t u;
	if(kr)
		return kr;
	u = gSjisToUni[code];
	return u ? (uint32_t)u : 0xfffdu;
}

void OsAndroid_SjisInit(void)
{
	int i;
	char path[PATH_MAX];
	FILE* f;
	for(i = 0; i < kCp932PairCount; i++)
		gSjisToUni[kCp932Pairs[i].sjis] = kCp932Pairs[i].uni;
	// the Korean patch's table, next to the game's exe directory
	snprintf(path, sizeof path, "%ssjis_ext.bin", gPosixExeDir);
	f = fopen(path, "rb");
	if(f)
	{
		long len;
		if(fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0)
		{
			gExt = (uint16_t*)malloc((size_t)len);
			if(gExt && fread(gExt, 1, (size_t)len, f) == (size_t)len)
				gExtCount = (int)(len / 2);
			else
			{
				free(gExt);
				gExt = NULL;
				gExtCount = 0;
			}
		}
		fclose(f);
	}
}

void OsAndroid_SjisShutdown(void)
{
	free(gExt);
	gExt = NULL;
	gExtCount = 0;
}

uint32_t OsAndroid_SjisChar(uint16_t code)
{
	return SjisLookup(code);
}

uint32_t OS_SjisToCodePoint(uint16_t sjis)
{
	return SjisLookup(sjis);
}

size_t OS_SjisToUtf8(const char* sjis, char* out, size_t n)
{
	return OsCommon_SjisToUtf8(sjis, out, n, SjisLookup);
}

void OsAndroid_SjisToUtf8(const char* sjis, char* out, size_t n)
{
	OsCommon_SjisToUtf8(sjis, out, n, SjisLookup);
}

/* one code point -> 1 or 2 Shift-JIS bytes in out[]; the byte count, 0 when
 * CP932 has no code for it */
int OsAndroid_UnicodeToSjis(uint32_t cp, char out[2])
{
	int i;
	if(cp < 0x80)
	{
		out[0] = (char)cp;
		return 1;
	}
	if(cp > 0xffff)
		return 0;
	for(i = 0; i < kCp932PairCount; i++)
	{
		if(kCp932Pairs[i].uni == cp && kCp932Pairs[i].sjis > 0xff)
		{
			out[0] = (char)(kCp932Pairs[i].sjis >> 8);
			out[1] = (char)(kCp932Pairs[i].sjis & 0xff);
			return 2;
		}
	}
	return 0;
}

/* UTF-8 -> Shift-JIS character by character ('?' for a character without a
 * code, a stray byte taken as Latin-1); the length written */
size_t OS_Utf8ToSjis(const char* utf8, char* out, size_t n)
{
	const uint8_t* u = (const uint8_t*)utf8;
	size_t o = 0;
	while(*u && o + 2 < n)
	{
		int len, m;
		uint32_t cp = OsCommon_Utf8SeqLen(u) ? OsCommon_Utf8Decode(u, &len) : (len = 1, (uint32_t)*u);
		m = OsAndroid_UnicodeToSjis(cp, out + o);
		if(m == 0)
		{
			out[o] = '?';
			m = 1;
		}
		o += (size_t)m;
		u += len;
	}
	out[o] = 0;
	return o;
}
