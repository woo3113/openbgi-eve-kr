/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * window.c - the Android back end's main window, display and input
 *            (the OS_Window* / OS_Display* / OS_Blit* / OS_SetEventHandlers
 *            / OS_PumpMessages services of inc/bgi/os.h), on SDL2.
 *
 * The engine renders into a memory surface and hands it over with
 * OS_StretchToWindow / OS_BlitToWindow; here that surface is kept in an
 * SDL streaming texture at the picture size and stretched to the window.
 * Input reaches the engine through the OsEventHandlers_t callbacks from
 * OS_PumpMessages(): keys are translated to Windows virtual keys, the
 * touch screen and the mouse as mouse events, and game controllers
 * (SDL_GameController) as virtual keys and cursor motion so a visual novel
 * is fully playable with a pad.
 */
#include "android_internal.h"
#include <SDL.h>
#include <android/log.h>
#include "bgi/engine.h"
#include "bgi/sysobj/database.h"

#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, "BGI", __VA_ARGS__)

// ---- state ---------------------------------------------------------------------------------

static SDL_Window* gWin;
static SDL_Renderer* gRen;
static SDL_Texture* gTex;    // the picture at its own resolution
static uint32_t* gPix;       // the client buffer (0x00RRGGBB), gPixW x gPixH
static int gPixW, gPixH;     // the picture size (the display mode)
static int gWinW, gWinH;     // the window's client size in pixels
static int gDirty;           // the texture needs an update before the next present
static int gAlive;
static int gFullscreen;
static int gMinimized;
static int gActive = 1;
static int gQuitPosted;
static int gCursorShape = 1;
static int gCursorShown = 1;
static int gMouseX = -1, gMouseY = -1; // the cursor in picture coordinates (-1 until first known)
static int gImeOpen;

static int gOutW, gOutH;                               // the renderer output (window drawable) size
static int gPresentX, gPresentY, gPresentW, gPresentH; // the picture's rect within that output

/* Persist the game's global database, which holds the options (the system
 * area).  The game itself only writes it on its quit path ("sys.gdb_save"
 * in sysmsg._bp), which Android rarely reaches; the back end calls this when
 * the app leaves the foreground instead.  Runs on the game's thread. */
void OS_Android_SaveConfig(void)
{
	if(!gEngineUp)
		return;
	GlobalDB_Save();
}

// window pixels <-> picture pixels (the picture is fit, with possible bars)
static int WinToPicX(int x) { return gPresentW ? (x - gPresentX) * gPixW / gPresentW : x; }
static int WinToPicY(int y) { return gPresentH ? (y - gPresentY) * gPixH / gPresentH : y; }
static int PicToWinX(int x) { return gPresentW ? gPresentX + x * gPresentW / gPixW : x; }
static int PicToWinY(int y) { return gPresentH ? gPresentY + y * gPresentH / gPixH : y; }

static OsEventHandlers_t gHandlers; // copied by OS_SetEventHandlers
static int gDragAccept;

static SDL_GameController* gPad;
static SDL_JoystickID gPadId = -1;
static int gPadMouseSpeed = 6; // cursor pixels per tick at full deflection
static int gPadMouseDx, gPadMouseDy;
static int gPadHatDx, gPadHatDy; // the direction pad's contribution
static int gHatLast = -1;
static uint32_t gLastInput; // the tick of the last user input, for the cursor auto-hide
static uint32_t gPadLast, gPadAccum;

void OsAndroid_RequestClose(void)
{
	if(gHandlers.close_request)
		gHandlers.close_request();
}

/* The game and configuration directories.  The game data is gigabytes, so
 * it is not bundled in the APK; the user copies the game folder to the
 * app's external storage (or BGI_GAMEDIR names it).  The configuration
 * (registry, saves) goes to the app's internal storage. */
void OsAndroid_DefaultDirs(char* gameDir, size_t gN, char* configDir, size_t cN)
{
	const char* g = getenv("BGI_GAMEDIR");
	const char* c = getenv("BGI_CONFIGDIR");
	const char* ext = SDL_AndroidGetExternalStoragePath();
	const char* intl = SDL_AndroidGetInternalStoragePath();
	if(g && *g)
		snprintf(gameDir, gN, "%s", g);
	else if(ext)
		snprintf(gameDir, gN, "%s", ext);
	else
		snprintf(gameDir, gN, ".");
	if(c && *c)
		snprintf(configDir, cN, "%s", c);
	else if(intl)
		snprintf(configDir, cN, "%s/bgi", intl);
	else
		snprintf(configDir, cN, "/data/local/tmp/bgi");
}

// ---- cursor / client<->screen helpers ------------------------------------------------------

void OS_CursorSetShape(int shape)
{
	gCursorShape = shape;
	// Android has no arrow/custom cursors; the touch screen shows none
	SDL_ShowCursor(shape == OS_CURSOR_NONE || !gCursorShown ? SDL_DISABLE : SDL_ENABLE);
}

void OS_CursorShow(int show)
{
	if(show)
		gCursorShown++;
	else
		gCursorShown--;
	SDL_ShowCursor(gCursorShown > 0 ? SDL_ENABLE : SDL_DISABLE);
}

void OS_CursorGetPos(int* x, int* y)
{
	if(gMouseX < 0)
	{
		int wx, wy;
		SDL_GetMouseState(&wx, &wy);
		gMouseX = WinToPicX(wx);
		gMouseY = WinToPicY(wy);
	}
	*x = gMouseX;
	*y = gMouseY;
}

void OS_CursorSetPos(int x, int y)
{
	/* Android cannot warp the system cursor, so the position lives in the
	 * back end; SDL_WarpMouseInWindow is a no-op here and would only raise
	 * a spurious motion event, so it is not called */
	gMouseX = x;
	gMouseY = y;
}

void OS_ScreenToClient(int* x, int* y)
{
	int wx, wy;
	SDL_GetWindowPosition(gWin, &wx, &wy);
	*x -= wx;
	*y -= wy;
}

void OS_ClientToScreen(int* x, int* y)
{
	int wx, wy;
	SDL_GetWindowPosition(gWin, &wx, &wy);
	*x += wx;
	*y += wy;
}

int OS_MouseButtonsSwapped(void)
{
	return 0;
}

int OS_KeyState(int vk)
{
	const Uint8* ks = SDL_GetKeyboardState(NULL);
	SDL_Scancode sc = SDL_GetScancodeFromKey((SDL_Keycode)0);
	BGI_UNUSED(ks);
	BGI_UNUSED(sc);
	return 0;
}

void OS_KeyboardState(uint8_t out[256])
{
	// "81 11" of 1.69/472 on: bit 7 = down; SDL has no such table cheaply,
	// report nothing down (scripts use it for shortcut checks)
	memset(out, 0, 256);
}

// ---- window --------------------------------------------------------------------------------

/* (Re)create the picture buffer and its texture at w x h.  The engine draws
 * at the picture size it chose from the scripts (1280x720 for this game);
 * the buffer follows the surface it hands over, so the frame is never
 * cropped or rescaled before the final fit to the display. */
static void EnsureSurface(int w, int h)
{
	if(w <= 0 || h <= 0)
		return;
	if(gPix && w == gPixW && h == gPixH)
		return;
	gPixW = w;
	gPixH = h;
	ALOG("picture size %dx%d", w, h);
	if(gTex)
		SDL_DestroyTexture(gTex);
	gTex = SDL_CreateTexture(gRen, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
	free(gPix);
	gPix = (uint32_t*)calloc((size_t)w * h, 4);
	gDirty = 1;
}

int OS_WindowCreate(const OsWindowDesc_t* d)
{
	int w, h;
	if(gWin)
		return 1;
	if(SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0)
		return 0;
	gPixW = d->width > 0 ? d->width : 800;
	gPixH = d->height > 0 ? d->height : 600;
	w = gPixW;
	h = gPixH;
	gWin = SDL_CreateWindow("bgi", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if(!gWin)
		return 0;
	gRen = SDL_CreateRenderer(gWin, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(!gRen)
		gRen = SDL_CreateRenderer(gWin, -1, 0);
	gTex = SDL_CreateTexture(gRen, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, gPixW, gPixH);
	gPix = (uint32_t*)calloc((size_t)gPixW * gPixH, 4);
	SDL_GetWindowSize(gWin, &gWinW, &gWinH);
	EnsureSurface(gPixW, gPixH);
	/* Android has no desktop window management: go immersive
	 * fullscreen (no status/navigation bars) from the start and stay
	 * there; the engine's windowed requests are ignored below. */
	SDL_SetWindowFullscreen(gWin, SDL_WINDOW_FULLSCREEN);
	gFullscreen = 1;
	gAlive = 1;
	gMouseX = gPixW / 2; // the software cursor starts at the centre of the picture
	gMouseY = gPixH / 2;
	gLastInput = SDL_GetTicks(); // show the cursor until the first idle period
	SDL_GameControllerEventState(SDL_ENABLE);
	/* No text input here: the soft keyboard is shown only while the
	 * engine's edit control asks for it (OS_EditShow), so it does not pop
	 * up over the game at start-up. */
	if(gHandlers.created)
		gHandlers.created();
	return gTex && gPix;
}

void OS_WindowDestroy(void)
{
	if(!gWin)
		return;
	if(gHandlers.destroyed)
		gHandlers.destroyed();
	if(gPad)
	{
		SDL_GameControllerClose(gPad);
		gPad = NULL;
	}
	if(gTex)
		SDL_DestroyTexture(gTex);
	if(gRen)
		SDL_DestroyRenderer(gRen);
	SDL_DestroyWindow(gWin);
	free(gPix);
	gTex = NULL;
	gRen = NULL;
	gWin = NULL;
	gPix = NULL;
	gAlive = 0;
}

int OS_WindowAlive(void)
{
	return gAlive;
}

void OS_WindowShow(int show)
{
	if(gWin)
	{
		if(show)
			SDL_ShowWindow(gWin);
		else
			SDL_HideWindow(gWin);
	}
}

void OS_WindowSetTitle(const char* t)
{
	char utf[0x200];
	OsAndroid_SjisToUtf8(t ? t : "", utf, sizeof utf);
	if(gWin)
		SDL_SetWindowTitle(gWin, utf);
}

void OS_WindowMove(int x, int y)
{
	if(gWin)
		SDL_SetWindowPosition(gWin, x, y);
}

void OS_WindowGetRect(int* x, int* y, int* w, int* h)
{
	int ww, wh, x0, y0;
	if(!gWin)
	{
		*x = *y = 0;
		*w = gPixW;
		*h = gPixH;
		return;
	}
	SDL_GetWindowSize(gWin, &ww, &wh);
	SDL_GetWindowPosition(gWin, &x0, &y0);
	if(x)
		*x = x0;
	if(y)
		*y = y0;
	if(w)
		*w = gFullscreen ? gPixW : ww;
	if(h)
		*h = gFullscreen ? gPixH : wh;
}

int OS_WindowMinimized(void)
{
	return gMinimized;
}

int OS_WindowActive(void)
{
	return gActive;
}

void OS_WindowMinimize(void)
{
	if(gWin)
		SDL_MinimizeWindow(gWin);
}

void OS_WindowClose(void)
{
	if(gHandlers.close_request)
		gHandlers.close_request();
}

void OS_ScreenSize(int* w, int* h)
{
	SDL_DisplayMode m;
	if(SDL_GetDesktopDisplayMode(0, &m) == 0)
	{
		*w = m.w;
		*h = m.h;
	}
	else
	{
		*w = gPixW;
		*h = gPixH;
	}
}

void OS_FrameMetrics(int* fw, int* fh)
{
	*fw = 0;
	*fh = 0;
}

int OS_FrameTop(void)
{
	return 0;
}

void OS_ImeStatus(int open)
{
	gImeOpen = open;
}

// ---- display modes -------------------------------------------------------------------------

int OS_DisplaySetMode(int fullscreen, int w, int h, int bpp)
{
	BGI_UNUSED(bpp);
	gPixW = w;
	gPixH = h;
	ALOG("DisplaySetMode fullscreen=%d mode=%dx%d", fullscreen, w, h);
	EnsureSurface(w, h);
	gDirty = 1;
	return 1;
}

void OS_DisplayRestore(void)
{
}

void OS_WindowSetFullscreen(int modeW, int modeH)
{
	BGI_UNUSED(modeW);
	BGI_UNUSED(modeH);
	gFullscreen = 1;
	if(gWin)
		SDL_SetWindowFullscreen(gWin, SDL_WINDOW_FULLSCREEN);
}

void OS_WindowSetWindowed(int x, int y, int clientW, int clientH)
{
	/* Android stays immersive fullscreen; just record the size the engine
	 * asked for (the picture is scaled to the display). */
	BGI_UNUSED(x);
	BGI_UNUSED(y);
	BGI_UNUSED(clientW);
	BGI_UNUSED(clientH);
	gFullscreen = 1;
}

// ---- presentation --------------------------------------------------------------------------

typedef struct AndroidDc
{
	int dummy;
} AndroidDc_t;

static AndroidDc_t gMainDc;

OsDc_t* OS_WindowGetDc(void)
{
	return gWin ? (OsDc_t*)&gMainDc : NULL;
}

void OS_WindowReleaseDc(OsDc_t* dc)
{
	BGI_UNUSED(dc);
}

/* Android has no system cursor, so the engine's default arrow is drawn here
 * (the game relies on it; custom cursor sprites are drawn by the engine).
 * 'B' black outline, 'W' white fill, '.' transparent. */
static const char* gCurArrow[19] = {
	"B...........", "BB..........", "BWB.........", "BWWB........", "BWWWB.......",
	"BWWWWB......", "BWWWWWB.....", "BWWWWWWB....", "BWWWWWWWB...", "BWWWWWWWWB..",
	"BWWWWWBBBBB.", "BWWBWWB.....", "BWB.BWWB....", "BB..BWWB....", "B....BWWB...",
	".....BWWB...", "......BWWB..", "......BWB...", ".......B....",
};

static void DrawCursor(int dstx, int dsty, int dstw, int dsth)
{
	int px, py, r, c, bx, by, cw, ch;
	if(!gCursorShown || gCursorShape == OS_CURSOR_NONE)
		return;
	if(SDL_GetTicks() - gLastInput > 3000)
		return; // auto-hide after a few seconds without input
	OS_CursorGetPos(&px, &py);
	if(px < 0 || py < 0 || px >= gPixW || py >= gPixH)
		return;
	bx = dstx + (int)((int64_t)px * dstw / gPixW);
	by = dsty + (int)((int64_t)py * dsth / gPixH);
	/* the arrow is 12 x 19 cells; draw each cell about two picture pixels
	 * wide so the cursor stays visible (the picture is scaled on the display) */
	cw = (dstw * 2 + gPixW - 1) / gPixW;
	if(cw < 1)
		cw = 1;
	ch = (dsth * 2 + gPixH - 1) / gPixH;
	if(ch < 1)
		ch = 1;
	for(r = 0; r < 19; r++)
		for(c = 0; c < 12; c++)
		{
			char k = gCurArrow[r][c];
			SDL_Rect q;
			if(k == '.')
				continue;
			q.x = bx + c * cw;
			q.y = by + r * ch;
			q.w = cw;
			q.h = ch;
			if(k == 'B')
				SDL_SetRenderDrawColor(gRen, 0, 0, 0, 255);
			else
				SDL_SetRenderDrawColor(gRen, 255, 255, 255, 255);
			SDL_RenderFillRect(gRen, &q);
		}
}

static void Present(void)
{
	int ow, oh, dw, dh;
	SDL_Rect dst;
	if(!gRen || !gTex || !gPix)
		return;
	if(gDirty)
	{
		SDL_UpdateTexture(gTex, NULL, gPix, gPixW * 4);
		gDirty = 0;
	}
	SDL_SetRenderDrawColor(gRen, 0, 0, 0, 255);
	SDL_RenderClear(gRen);
	/* The picture keeps its own aspect (4:3 for this game), centred in the
	 * display with black bars around it - never cropped or stretched. */
	SDL_GetRendererOutputSize(gRen, &ow, &oh);
	if((int64_t)ow * gPixH < (int64_t)oh * gPixW)
	{
		dw = ow;
		dh = (int)((int64_t)gPixH * ow / gPixW);
	}
	else
	{
		dh = oh;
		dw = (int)((int64_t)gPixW * oh / gPixH);
	}
	dst.x = (ow - dw) / 2;
	dst.y = (oh - dh) / 2;
	dst.w = dw;
	dst.h = dh;
	gOutW = ow;
	gOutH = oh;
	gPresentX = dst.x;
	gPresentY = dst.y;
	gPresentW = dst.w;
	gPresentH = dst.h;
	SDL_RenderCopy(gRen, gTex, NULL, &dst);
	DrawCursor(dst.x, dst.y, dst.w, dst.h);
	SDL_RenderPresent(gRen);
}

void OS_BlitToWindow(OsDc_t* dc, int dstX, int dstY, int w, int h, const OsSurface_t* src, int srcX, int srcY)
{
	int y;
	BGI_UNUSED(dc);
	if(!gPix || !src)
		return;
	EnsureSurface(src->width, src->height);
	for(y = 0; y < h; y++)
	{
		int dy = dstY + y, sy = srcY + y, x;
		if(dy < 0 || dy >= gPixH)
			continue;
		for(x = 0; x < w; x++)
		{
			int dx = dstX + x, sx = srcX + x;
			uint8_t* p;
			uint32_t pix;
			if(dx < 0 || dx >= gPixW)
				continue;
			p = (uint8_t*)src->pixels + (size_t)sy * src->pitch + (size_t)sx * (src->bpp == 16 ? 2 : 4);
			if(src->bpp == 16)
			{
				uint16_t v = *(uint16_t*)p;
				pix = ((v & 0x7c00u) << 9) | ((v & 0x03e0u) << 6) | ((v & 0x001fu) << 3);
			}
			else
				pix = *(uint32_t*)p;
			gPix[dy * gPixW + dx] = pix & 0x00ffffffu;
		}
	}
	gDirty = 1;
	Present();
}

void OS_StretchToWindow(OsDc_t* dc, int dstX, int dstY, int dstW, int dstH, const OsSurface_t* src, const int* clip)
{
	int y;
	BGI_UNUSED(dc);
	BGI_UNUSED(dstX);
	BGI_UNUSED(dstY);
	BGI_UNUSED(dstW);
	BGI_UNUSED(dstH);
	if(!gPix || !src)
		return;
	EnsureSurface(src->width, src->height);
	// the engine asks for a whole-surface stretch at the picture size; copy it
	for(y = 0; y < src->height && y < gPixH; y++)
	{
		const uint8_t* sp = (const uint8_t*)src->pixels + (size_t)y * src->pitch;
		int x;
		if(clip && (y < clip[1] || y >= clip[3]))
			continue;
		for(x = 0; x < src->width && x < gPixW; x++)
		{
			uint32_t pix;
			if(src->bpp == 16)
			{
				uint16_t v = *(const uint16_t*)(sp + (size_t)x * 2);
				pix = ((v & 0x7c00u) << 9) | ((v & 0x03e0u) << 6) | ((v & 0x001fu) << 3);
			}
			else
				pix = *(const uint32_t*)(sp + (size_t)x * 4);
			gPix[y * gPixW + x] = pix & 0x00ffffffu;
		}
	}
	gDirty = 1;
	Present();
}

void OS_WindowFillBlack(void)
{
	if(gPix)
		memset(gPix, 0, (size_t)gPixW * gPixH * 4);
	gDirty = 1;
	Present();
}

// ---- events --------------------------------------------------------------------------------

void OS_SetEventHandlers(const OsEventHandlers_t* h)
{
	gHandlers = *h;
}

void OS_PostQuit(void)
{
	gQuitPosted = 1;
}

void OS_DragAccept(int accept)
{
	gDragAccept = accept;
}

// SDL keycode -> Windows virtual key
static int SdlToVk(SDL_Keycode k)
{
	if(k >= SDLK_a && k <= SDLK_z)
		return 'A' + (int)(k - SDLK_a);
	if(k >= SDLK_0 && k <= SDLK_9)
		return '0' + (int)(k - SDLK_0);
	switch(k)
	{
	case SDLK_RETURN: return 0x0d;
	case SDLK_ESCAPE: return 0x1b;
	case SDLK_BACKSPACE: return 0x08;
	case SDLK_TAB: return 0x09;
	case SDLK_SPACE: return 0x20;
	case SDLK_LEFT: return 0x25;
	case SDLK_UP: return 0x26;
	case SDLK_RIGHT: return 0x27;
	case SDLK_DOWN: return 0x28;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT: return 0x10;
	case SDLK_LCTRL:
	case SDLK_RCTRL: return 0x11;
	case SDLK_LALT:
	case SDLK_RALT: return 0x12;
	case SDLK_F1: return 0x70;
	case SDLK_F2: return 0x71;
	case SDLK_F3: return 0x72;
	default: break;
	}
	return 0;
}

static void PadOpen(int deviceIndex)
{
	if(gPad)
		return;
	gPad = SDL_GameControllerOpen(deviceIndex);
	if(gPad)
		gPadId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gPad));
}

/* A pad as a visual novel's controls: the d-pad and left stick move the
 * cursor and press the left button (A / the right trigger is "click"),
 * B is Escape, the shoulders scroll.  Kept intentionally simple; the
 * mapping can grow. */
static void PadButton(int vk, int down)
{
	if(gHandlers.key_down || gHandlers.key_up)
	{
		if(vk)
		{
			if(down && gHandlers.key_down)
				gHandlers.key_down(vk, 0);
			else if(!down && gHandlers.key_up)
				gHandlers.key_up(vk);
		}
	}
}

static void PadAxis(void)
{
	const int dead = 8000;
	int lx = SDL_GameControllerGetAxis(gPad, SDL_CONTROLLER_AXIS_LEFTX);
	int ly = SDL_GameControllerGetAxis(gPad, SDL_CONTROLLER_AXIS_LEFTY);
	if(lx > -dead && lx < dead)
		lx = 0;
	if(ly > -dead && ly < dead)
		ly = 0;
	gPadMouseDx = lx * gPadMouseSpeed / 32767;
	gPadMouseDy = ly * gPadMouseSpeed / 32767;
}

int OS_PumpMessages(void)
{
	SDL_Event e;
	int had = 0;
	uint32_t now;
	while(SDL_PollEvent(&e))
	{
		had = 1;
		if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP ||
			e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEWHEEL || e.type == SDL_CONTROLLERBUTTONDOWN ||
			e.type == SDL_CONTROLLERBUTTONUP || e.type == SDL_CONTROLLERAXISMOTION || e.type == SDL_FINGERDOWN ||
			e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP)
			gLastInput = SDL_GetTicks(); // for the cursor auto-hide
		switch(e.type)
		{
		case SDL_QUIT:
			if(gHandlers.close_request)
				gHandlers.close_request();
			break;
		/* Android apps rarely go through the game's own quit path (where the
		 * engine writes the global database with the options), so persist it
		 * here when the app leaves the foreground.  Runs on the game's own
		 * thread: the event pump is part of the engine's frame loop. */
		case SDL_APP_WILLENTERBACKGROUND:
		case SDL_APP_DIDENTERBACKGROUND:
		case SDL_APP_TERMINATING:
			OS_Android_SaveConfig();
			break;
		case SDL_WINDOWEVENT:
			switch(e.window.event)
			{
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				gActive = 1;
				if(gHandlers.activate)
					gHandlers.activate(1);
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				gActive = 0;
				if(gHandlers.activate)
					gHandlers.activate(0);
				break;
			case SDL_WINDOWEVENT_MINIMIZED:
				gMinimized = 1;
				OS_Android_SaveConfig();
				if(gHandlers.sized)
					gHandlers.sized(1);
				break;
			case SDL_WINDOWEVENT_RESTORED:
				gMinimized = 0;
				if(gHandlers.sized)
					gHandlers.sized(0);
				break;
			case SDL_WINDOWEVENT_EXPOSED:
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				gDirty = 1;
				if(gHandlers.paint)
					gHandlers.paint();
				Present();
				break;
			default: break;
			}
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		{
			int vk = SdlToVk(e.key.keysym.sym);
			if(e.type == SDL_KEYDOWN && gHandlers.key_down)
				gHandlers.key_down(vk, e.key.repeat);
			else if(e.type == SDL_KEYUP && gHandlers.key_up)
				gHandlers.key_up(vk);
			break;
		}
		case SDL_MOUSEMOTION:
			gMouseX = WinToPicX(e.motion.x);
			gMouseY = WinToPicY(e.motion.y);
			if(gHandlers.mouse_move)
				gHandlers.mouse_move(gMouseX, gMouseY);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		{
			int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1
					: e.button.button == SDL_BUTTON_MIDDLE ? 2 : 3;
			if(gHandlers.mouse_button)
				gHandlers.mouse_button(b, e.type == SDL_MOUSEBUTTONDOWN, WinToPicX(e.button.x), WinToPicY(e.button.y));
			break;
		}
		case SDL_MOUSEWHEEL:
			if(gHandlers.mouse_wheel)
				gHandlers.mouse_wheel(e.wheel.y * 120);
			break;
		case SDL_CONTROLLERDEVICEADDED:
			PadOpen(e.cdevice.which);
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			if(gPad && e.cdevice.which == gPadId)
			{
				SDL_GameControllerClose(gPad);
				gPad = NULL;
				gPadId = -1;
			}
			break;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
		{
			int down = e.type == SDL_CONTROLLERBUTTONDOWN;
			switch(e.cbutton.button)
			{
			case SDL_CONTROLLER_BUTTON_DPAD_UP: PadButton(0x26, down); break;
			case SDL_CONTROLLER_BUTTON_DPAD_DOWN: PadButton(0x28, down); break;
			case SDL_CONTROLLER_BUTTON_DPAD_LEFT: PadButton(0x25, down); break;
			case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: PadButton(0x27, down); break;
			case SDL_CONTROLLER_BUTTON_A: PadButton(0x0d, down); break; // Enter (decide)
			case SDL_CONTROLLER_BUTTON_B:
			{
				int cx, cy;
				OS_CursorGetPos(&cx, &cy);
				if(gHandlers.mouse_button)
					gHandlers.mouse_button(1, down, cx, cy); // right button
				break;
			}
			case SDL_CONTROLLER_BUTTON_X:
			{
				int cx, cy;
				OS_CursorGetPos(&cx, &cy);
				if(gHandlers.mouse_button)
					gHandlers.mouse_button(0, down, cx, cy); // left button (select)
				break;
			}
			case SDL_CONTROLLER_BUTTON_Y: PadButton(0x11, down); break;    // Ctrl (skip)
			case SDL_CONTROLLER_BUTTON_BACK: PadButton(0x1b, down); break; // Escape (system menu)
			case SDL_CONTROLLER_BUTTON_START: PadButton(0x1b, down); break; // Escape (system menu)
			case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
				if(down && gHandlers.mouse_wheel)
					gHandlers.mouse_wheel(120);
				break;
			case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
				if(down && gHandlers.mouse_wheel)
					gHandlers.mouse_wheel(-120);
				break;
			default: break;
			}
			break;
		}
		case SDL_CONTROLLERAXISMOTION:
			PadAxis();
			break;
		case SDL_JOYHATMOTION:
		{
			/* the MANGMI pad's direction pad comes as a joystick hat; the
			 * polling in OS_PumpMessages covers it, so nothing here */
			break;
		}
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			break;
		case SDL_JOYAXISMOTION:
			break;
		default: break;
		}
	}
	// the direction pad (a raw joystick hat on this device): cursor + arrow keys
	if(gPad)
	{
		int h = SDL_JoystickGetHat(SDL_GameControllerGetJoystick(gPad), 0);
		int up = (h & SDL_HAT_UP) != 0, dn = (h & SDL_HAT_DOWN) != 0;
		int lf = (h & SDL_HAT_LEFT) != 0, rt = (h & SDL_HAT_RIGHT) != 0;
		if(h != gHatLast)
		{
			gHatLast = h;
			PadButton(0x26, up);
			PadButton(0x28, dn);
			PadButton(0x25, lf);
			PadButton(0x27, rt);
		}
		gPadHatDx = ((rt ? 1 : 0) - (lf ? 1 : 0)) * gPadMouseSpeed;
		gPadHatDy = ((dn ? 1 : 0) - (up ? 1 : 0)) * gPadMouseSpeed;
		if(gPadHatDx || gPadHatDy)
			gLastInput = SDL_GetTicks();
	}
	// stick / direction pad -> cursor motion, a few pixels per frame (picture coordinates)
	if(gPad && (gPadMouseDx || gPadMouseDy || gPadHatDx || gPadHatDy))
	{
		int mx, my;
		OS_CursorGetPos(&mx, &my);
		mx += gPadMouseDx + gPadHatDx;
		my += gPadMouseDy + gPadHatDy;
		if(mx < 0) mx = 0;
		if(my < 0) my = 0;
		if(mx > gPixW - 1) mx = gPixW - 1;
		if(my > gPixH - 1) my = gPixH - 1;
		OS_CursorSetPos(mx, my);
		if(gHandlers.mouse_move)
			gHandlers.mouse_move(mx, my);
	}
	now = SDL_GetTicks();
	if(now - gPadLast >= 8)
	{
		gPadLast = now;
		gPadAccum = 0;
	}
	BGI_UNUSED(gPadAccum);
	if(gQuitPosted && gHandlers.quit)
		gHandlers.quit();
	SDL_PumpEvents();
	return had;
}
