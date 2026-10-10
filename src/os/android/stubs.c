/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * stubs.c - the Android back end's stand-ins for the services a phone has
 *           no counterpart for: CD audio (MCI), movies, the shell services,
 *           the installer's dialogs, the debugger's window, and the script
 *           dialogs the reimplementation does not draw here yet.  Each
 *           reports failure or its default, as the POSIX back end's own
 *           stubs do; the engine tolerates all of them.
 */
#include "android_internal.h"
#include <SDL.h>

// ---- message box / dialogs -----------------------------------------------------------------

int OS_MessageBox(const char* text, const char* caption, uint32_t flags)
{
	char t[0x400], c[0x100];
	int def;
	/* always keep the text in the log (the engine's errors are message
	 * boxes, which otherwise only reach the screen) */
	fprintf(stderr, "MSG [%s] %s\n", caption ? caption : "", text ? text : "");
	if(getenv("BGI_MSGBOX_STDERR"))
	{
		switch(flags & 0xf)
		{
		case OS_MB_OKCANCEL: def = (flags & OS_MB_DEFBUTTON2) ? OS_IDCANCEL : OS_IDOK; break;
		case OS_MB_YESNO: def = (flags & OS_MB_DEFBUTTON2) ? OS_IDNO : OS_IDYES; break;
		case OS_MB_RETRYCANCEL: def = (flags & OS_MB_DEFBUTTON2) ? OS_IDCANCEL : OS_IDRETRY; break;
		default: def = OS_IDOK; break;
		}
		fprintf(stderr, "bgi: message box [%s] %s (-> %d)\n", caption ? caption : "", text ? text : "", def);
		return def;
	}
	OsAndroid_SjisToUtf8(text ? text : "", t, sizeof t);
	OsAndroid_SjisToUtf8(caption ? caption : "", c, sizeof c);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, c[0] ? c : "OpenBGI", t, NULL);
	switch(flags & 0xf)
	{
	case OS_MB_OKCANCEL: return OS_IDOK;
	case OS_MB_YESNO: return OS_IDYES;
	case OS_MB_RETRYCANCEL: return OS_IDRETRY;
	default: return OS_IDOK;
	}
}

int OS_DialogInput1(const char* title, const char* initial, int maxLen, char* out)
{
	BGI_UNUSED(title); BGI_UNUSED(initial); BGI_UNUSED(maxLen); BGI_UNUSED(out);
	return 0;
}

int OS_DialogInput2(const char* title, const char* label1, const char* initial1, int maxLen1, char* out1,
	const char* label2, const char* initial2, int maxLen2, char* out2)
{
	BGI_UNUSED(title); BGI_UNUSED(label1); BGI_UNUSED(initial1); BGI_UNUSED(maxLen1); BGI_UNUSED(out1);
	BGI_UNUSED(label2); BGI_UNUSED(initial2); BGI_UNUSED(maxLen2); BGI_UNUSED(out2);
	return 0;
}

int OS_DialogProfile(char* surname, char* givenName, char* nickname, char* pronoun, int32_t* month, int32_t* day)
{
	BGI_UNUSED(surname); BGI_UNUSED(givenName); BGI_UNUSED(nickname); BGI_UNUSED(pronoun);
	BGI_UNUSED(month); BGI_UNUSED(day);
	return 0;
}

int OS_LauncherDialog(struct OsLauncher* l)
{
	BGI_UNUSED(l);
	return 0;
}

int OS_FileDialog(int save, char* path, size_t n, const char* filter, const char* defExt, const char* initialDir, const char* title)
{
	BGI_UNUSED(save); BGI_UNUSED(path); BGI_UNUSED(n); BGI_UNUSED(filter);
	BGI_UNUSED(defExt); BGI_UNUSED(initialDir); BGI_UNUSED(title);
	return 0;
}

int OS_BrowseFolder(char* path, size_t n, const char* title, const char* initial)
{
	BGI_UNUSED(path); BGI_UNUSED(n); BGI_UNUSED(title); BGI_UNUSED(initial);
	return 0;
}

// ---- shell services ------------------------------------------------------------------------

int OS_ClipboardSetText(const char* text)
{
	char utf[0x10000];
	if(SDL_HasClipboardText())
		SDL_SetClipboardText("");
	OsAndroid_SjisToUtf8(text ? text : "", utf, sizeof utf);
	SDL_SetClipboardText(utf);
	return 1;
}

// ---- CD audio (MCI) ------------------------------------------------------------------------

uint32_t OS_MciOpenCdAudio(const char* element) { BGI_UNUSED(element); return 0; }
int OS_MciSetTimeFormatTmsf(uint32_t dev) { BGI_UNUSED(dev); return 0; }
int OS_MciClose(uint32_t dev) { BGI_UNUSED(dev); return 0; }
int OS_MciStatus(uint32_t dev, uint32_t item, uint32_t* out) { BGI_UNUSED(dev); BGI_UNUSED(item); BGI_UNUSED(out); return 0; }
int OS_MciPlay(uint32_t dev, uint32_t from, uint32_t to, int notify) { BGI_UNUSED(dev); BGI_UNUSED(from); BGI_UNUSED(to); BGI_UNUSED(notify); return 0; }
int OS_MciStop(uint32_t dev) { BGI_UNUSED(dev); return 0; }
int OS_PlaySoundFile(const char* path) { BGI_UNUSED(path); return 1; }

// ---- movies --------------------------------------------------------------------------------

int OS_MovieOpen(const char* path) { BGI_UNUSED(path); return OS_MOVIE_FAILED; }
int OS_MovieStart(int x, int y, int w, int h, int32_t volume, int32_t* lengthMs)
{
	BGI_UNUSED(x); BGI_UNUSED(y); BGI_UNUSED(w); BGI_UNUSED(h); BGI_UNUSED(volume); BGI_UNUSED(lengthMs);
	return 0;
}
void OS_MovieStop(void) {}
int OS_MovieIsRunning(void) { return 0; }
void OS_MovieSetVolume(int32_t vol) { BGI_UNUSED(vol); }

// ---- debugger window -----------------------------------------------------------------------

int OS_DebugWindowOpen(int w, int h, const char* title) { BGI_UNUSED(w); BGI_UNUSED(h); BGI_UNUSED(title); return 0; }
void OS_DebugWindowClose(void) {}
int OS_DebugWindowAlive(void) { return 0; }
void OS_DebugWindowPresent(const OsSurface_t* s) { BGI_UNUSED(s); }
