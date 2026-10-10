/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * edit.c - the Android back end's text entry (the "B0 2x" instructions,
 *          OS_Edit* of inc/bgi/os.h).  SDL_StartTextInput already delivers
 *          the soft keyboard to the window through SDL_TEXTINPUT; the
 *          engine's own edit path draws the field itself, so this back end
 *          keeps the text in a small buffer and reports it, without a
 *          separate control.  Good enough for save names and profile input.
 */
#include "android_internal.h"
#include <SDL.h>

static char gText[0x100];
static int gLimit;
static int gFocus;
static int gRejectAscii;
static int gShown;

void OS_EditShow(int show)
{
	gShown = show;
	if(show)
		SDL_StartTextInput();
	else
		SDL_StopTextInput();
}

void OS_EditMove(int x, int y, int w, int h) { BGI_UNUSED(x); BGI_UNUSED(y); BGI_UNUSED(w); BGI_UNUSED(h); }
void OS_EditSetFont(const char* face, int size) { BGI_UNUSED(face); BGI_UNUSED(size); }

void OS_EditLimitText(int maxChars)
{
	gLimit = maxChars;
	if(gLimit > 0 && (int)strlen(gText) > gLimit)
		gText[gLimit] = 0;
}

void OS_EditSetText(const char* t)
{
	snprintf(gText, sizeof gText, "%s", t ? t : "");
	if(gLimit > 0 && (int)strlen(gText) > gLimit)
		gText[gLimit] = 0;
}

void OS_EditSelectAll(void) {}
void OS_EditFocus(int toEdit) { gFocus = toEdit; }
int OS_EditGetText(char* buf, size_t n) { size_t l = strlen(gText); if(n) { snprintf(buf, n, "%s", gText); } return (int)l; }
void OS_EditSetColour(uint32_t rgb) { BGI_UNUSED(rgb); }
void OS_EditRefresh(void) {}
void OS_EditDestroy(void) { gText[0] = 0; }
void OS_EditRejectAscii(int reject) { gRejectAscii = reject; }
int OS_EditImeOpen(void) { return 0; }
