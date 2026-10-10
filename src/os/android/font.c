/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * font.c - the Android back end's glyph rasteriser (the OS_Font* services of
 *          inc/bgi/os.h), on stb_truetype.
 *
 * The engine asks for a GDI font by (Shift-JIS) family name and draws one
 * character at a time into an 8-bit coverage buffer (0 = blank, 255 = ink)
 * or a 1-bit bitmap, then box-filters the result itself.  There is no GDI
 * on Android, so a TrueType/OpenType face is loaded with stb_truetype and
 * the glyphs are rasterised directly.  Faces the scripts add (OS_FontAddFile
 * / OS_FontAddMemory, the "B0 C2" / "B0 C3" instructions, which the Korean
 * patch uses for its font) are remembered and preferred; otherwise a system
 * CJK face is used.
 */
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "android_internal.h"

typedef struct AddedFace
{
	char face[0x80];   // the Shift-JIS family name it was added under
	char path[PATH_MAX]; // the file, when added from one (else empty)
	uint8_t* data;     // the bytes (owned when from memory)
	uint32_t size;
} AddedFace_t;

#define MAX_ADDED 16
static AddedFace_t gAdded[MAX_ADDED];
static int gAddedCount;

// the system fallback faces, in order (Android ships at least the first)
static const char* const kSystemFaces[] = {
	"/system/fonts/NotoSansCJK-Regular.ttc",
	"/system/fonts/NotoSansCJK-Regular.ttf",
	"/system/fonts/NotoSansKR-Regular.otf",
	"/system/fonts/DroidSansFallback.ttf",
	"/system/fonts/DroidSansJapanese.ttf",
};

struct OsFont
{
	stbtt_fontinfo info;
	uint8_t* data;  // the loaded font (owned)
	uint32_t size;
	float scale;
	int height;     // requested cell height (ascent + descent)
	int width;      // requested half-width advance
	int bold;
};

static uint8_t* gSysData;
static uint32_t gSysSize;

static uint8_t* ReadFileAll(const char* path, uint32_t* size)
{
	FILE* f = fopen(path, "rb");
	long len;
	uint8_t* b;
	*size = 0;
	if(!f)
		return NULL;
	if(fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET) != 0)
	{
		fclose(f);
		return NULL;
	}
	b = (uint8_t*)malloc((size_t)len);
	if(!b || fread(b, 1, (size_t)len, f) != (size_t)len)
	{
		free(b);
		fclose(f);
		return NULL;
	}
	fclose(f);
	*size = (uint32_t)len;
	return b;
}

static int InitStb(stbtt_fontinfo* info, uint8_t* data, uint32_t size)
{
	int off = stbtt_GetFontOffsetForIndex(data, 0);
	if(off < 0)
		return 0;
	return stbtt_InitFont(info, data, off);
}

static uint8_t* SystemFont(uint32_t* size)
{
	size_t i;
	if(gSysData)
	{
		*size = gSysSize;
		return gSysData;
	}
	for(i = 0; i < sizeof kSystemFaces / sizeof kSystemFaces[0]; i++)
	{
		uint8_t* d = ReadFileAll(kSystemFaces[i], size);
		if(d)
		{
			gSysData = d;
			gSysSize = *size;
			return d;
		}
	}
	*size = 0;
	return NULL;
}

// the face a script added (by name), matched case-insensitively
static AddedFace_t* FindAdded(const char* face)
{
	int i;
	if(!face || !*face)
		return NULL;
	for(i = 0; i < gAddedCount; i++)
	{
		if(strcasecmp(gAdded[i].face, face) == 0)
			return &gAdded[i];
	}
	return NULL;
}

int OS_FontAddFile(const char* path)
{
	char p[PATH_MAX];
	uint8_t* d;
	uint32_t sz;
	if(!path || gAddedCount >= MAX_ADDED)
		return 0;
	OsAndroid_Path(path, p, sizeof p);
	d = ReadFileAll(p, &sz);
	if(!d)
		return 0;
	gAdded[gAddedCount].data = d;
	gAdded[gAddedCount].size = sz;
	snprintf(gAdded[gAddedCount].path, sizeof gAdded[gAddedCount].path, "%s", p);
	gAdded[gAddedCount].face[0] = 0;
	gAddedCount++;
	return 1;
}

int OS_FontAddMemory(const void* data, uint32_t size)
{
	uint8_t* d;
	if(!data || !size || gAddedCount >= MAX_ADDED)
		return 0;
	d = (uint8_t*)malloc(size);
	if(!d)
		return 0;
	memcpy(d, data, size);
	gAdded[gAddedCount].data = d;
	gAdded[gAddedCount].size = size;
	gAdded[gAddedCount].path[0] = 0;
	gAdded[gAddedCount].face[0] = 0;
	gAddedCount++;
	return 1;
}

int OS_FontFaceExists(const char* face)
{
	uint32_t sz;
	BGI_UNUSED(face);
	if(FindAdded(face))
		return 1;
	return SystemFont(&sz) != NULL;
}

int OS_FontEnumFaces(char* buf, int charset, int* outBytes)
{
	// report the faces the scripts added, then nothing else
	int i, o = 0;
	BGI_UNUSED(charset);
	for(i = 0; i < gAddedCount && buf; i++)
	{
		size_t n = strlen(gAdded[i].face);
		if(o + (int)n + 1 > 0x400)
			break;
		memcpy(buf + o, gAdded[i].face, n + 1);
		o += (int)n + 1;
	}
	if(outBytes)
		*outBytes = o;
	return gAddedCount;
}

OsFont_t* OS_FontOpen(const char* face, int height, int width, int bold, int italic)
{
	OsFont_t* f = (OsFont_t*)calloc(1, sizeof *f);
	uint32_t sz = 0;
	uint8_t* data = NULL;
	AddedFace_t* a;
	BGI_UNUSED(italic);
	if(!f)
		return NULL;
	a = FindAdded(face);
	if(a)
	{
		data = a->data;
		sz = a->size;
	}
	else
		data = SystemFont(&sz);
	if(!data || !InitStb(&f->info, data, sz))
	{
		free(f);
		return NULL;
	}
	f->data = data;
	f->size = sz;
	f->height = height > 0 ? height : 16;
	f->width = width > 0 ? width : height / 2;
	f->bold = bold;
	f->scale = stbtt_ScaleForPixelHeight(&f->info, (float)f->height);
	return f;
}

OsFont_t* OS_FontOpenMono(const char* face, int size, int bold)
{
	return OS_FontOpen(face, size, size / 2, bold, 0);
}

void OS_FontClose(OsFont_t* f)
{
	free(f);
}

// the Unicode scalar of the character `len` bytes long in `sjis`
static uint32_t CharToUni(const char* sjis, int len)
{
	if(len == 1)
		return (uint8_t)sjis[0];
	if(len == 2)
		return OS_SjisToCodePoint((uint16_t)(((uint8_t)sjis[0] << 8) | (uint8_t)sjis[1]));
	// UTF-8 (three bytes at most here)
	return ((uint8_t)sjis[0] & 0x0f) << 12 | ((uint8_t)sjis[1] & 0x3f) << 6 | ((uint8_t)sjis[2] & 0x3f);
}

static void RenderGlyph(OsFont_t* f, const char* sjis, int len, uint8_t* buf, int pitch, int w, int h, int mono)
{
	uint32_t cp = CharToUni(sjis, len);
	int gi = stbtt_FindGlyphIndex(&f->info, (int)cp);
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	int gy, gw, gh, cy, cx;
	int ascent, descent, lineGap;
	float scale = f->scale;
	uint8_t* tmp;
	if(mono)
		memset(buf, 0, (size_t)pitch * h);
	else
		memset(buf, 0, (size_t)pitch * h);
	if(gi == 0 && cp != ' ')
		return;
	stbtt_GetFontVMetrics(&f->info, &ascent, &descent, &lineGap);
	stbtt_GetGlyphBitmapBox(&f->info, gi, scale * (f->bold ? 1.05f : 1.0f), scale, &x0, &y0, &x1, &y1);
	gw = x1 - x0;
	gh = y1 - y0;
	if(gw <= 0 || gh <= 0)
		return;
	tmp = (uint8_t*)malloc((size_t)gw * gh);
	if(!tmp)
		return;
	memset(tmp, 0, (size_t)gw * gh);
	stbtt_MakeGlyphBitmap(&f->info, tmp, gw, gh, gw, scale, scale, gi);
	// the baseline sits at the ascent; the glyph box's top is y0 above it
	gy = (int)(ascent * scale) + y0;
	for(cy = 0; cy < gh; cy++)
	{
		int dy = gy + cy;
		if(dy < 0 || dy >= h)
			continue;
		for(cx = 0; cx < gw; cx++)
		{
			int dx = x0 + cx;
			uint8_t v = tmp[cy * gw + cx];
			if(dx < 0 || dx >= w)
				continue;
			if(mono)
			{
				if(v >= 128)
					buf[(size_t)dy * pitch + (dx >> 3)] |= (uint8_t)(0x80 >> (dx & 7));
			}
			else
			{
				uint8_t* p = buf + (size_t)dy * pitch + dx;
				if(v > *p)
					*p = v;
			}
		}
	}
	free(tmp);
}

void OS_FontRender(OsFont_t* f, const char* sjis, int len, uint8_t* buf, int pitch, int w, int h)
{
	RenderGlyph(f, sjis, len, buf, pitch, w, h, 0);
}

void OS_FontRenderMono(OsFont_t* f, const char* sjis, int len, uint8_t* bits, int pitch, int w, int h)
{
	RenderGlyph(f, sjis, len, bits, pitch, w, h, 1);
}

int OS_FontCharAdvance(OsFont_t* f, const char* sjis, int len)
{
	uint32_t cp = CharToUni(sjis, len);
	int gi = stbtt_FindGlyphIndex(&f->info, (int)cp);
	int adv = 0, lsb = 0;
	stbtt_GetGlyphHMetrics(&f->info, gi, &adv, &lsb);
	return (int)(adv * f->scale + 0.5f);
}

void OsAndroid_FontShutdown(void)
{
	int i;
	for(i = 0; i < gAddedCount; i++)
		free(gAdded[i].data);
	gAddedCount = 0;
	if(gSysData)
	{
		free(gSysData);
		gSysData = NULL;
		gSysSize = 0;
	}
}
