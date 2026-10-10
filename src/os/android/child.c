/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * child.c - the Android back end's child windows ("B0 1x", OS_Child* of
 *           inc/bgi/os.h).  A phone has no multiple top-level windows to
 *           spare, so the pop-ups are not created: OS_ChildCreate reports
 *           failure and the engine skips them, as it does on any back end
 *           without the service.  The rest are no-ops that accept NULL.
 */
#include "android_internal.h"

OsChild_t* OS_ChildCreate(int index, const char* title, int x, int y, int outerW, int outerH)
{
	BGI_UNUSED(index); BGI_UNUSED(title); BGI_UNUSED(x); BGI_UNUSED(y); BGI_UNUSED(outerW); BGI_UNUSED(outerH);
	return NULL;
}

void OS_ChildClose(OsChild_t* c) { BGI_UNUSED(c); }
void OS_ChildShow(OsChild_t* c, int show) { BGI_UNUSED(c); BGI_UNUSED(show); }
void OS_ChildSetTitle(OsChild_t* c, const char* title) { BGI_UNUSED(c); BGI_UNUSED(title); }
void OS_ChildMove(OsChild_t* c, int x, int y, int outerW, int outerH) { BGI_UNUSED(c); BGI_UNUSED(x); BGI_UNUSED(y); BGI_UNUSED(outerW); BGI_UNUSED(outerH); }
void OS_ChildGetPos(OsChild_t* c, int* x, int* y) { BGI_UNUSED(c); if(x) *x = 0; if(y) *y = 0; }
OsDc_t* OS_ChildGetDc(OsChild_t* c) { BGI_UNUSED(c); return NULL; }
void OS_ChildReleaseDc(OsChild_t* c, OsDc_t* dc) { BGI_UNUSED(c); BGI_UNUSED(dc); }
