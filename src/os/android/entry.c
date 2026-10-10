/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * entry.c - the Android entry point (replaces src/sys/main.c for PLATFORM
 *           android).  SDLActivity loads libmain.so and calls SDL_main with
 *           the command line; the arguments are rejoined into the one
 *           command line the engine parses, exactly as the POSIX main does.
 */
#include <SDL.h>
#include <SDL_main.h>
#include <stdlib.h>
#include <stdio.h>

#include "bgi/engine.h"
#include "bgi/os.h"

int SDL_main(int argc, char** argv)
{
	char cmdLine[0x1000];
	size_t len = 0;
	int i, r;
	cmdLine[0] = 0;
	for(i = 1; i < argc; i++)
	{
		size_t n = strlen(argv[i]);
		if(len + n + 2 >= sizeof cmdLine)
			break;
		if(len)
			cmdLine[len++] = ' ';
		memcpy(cmdLine + len, argv[i], n + 1);
		len += n;
	}
	OS_SetArgv0(argv[0]);
	/* Keep the engine's own diagnostics (the --lang=ja/English messages,
	 * the stub trace, script errors) in a file under the app's internal
	 * storage, which logcat does not carry. */
	{
		const char* ip = SDL_AndroidGetInternalStoragePath();
		if(ip)
		{
			char lp[0x400];
			snprintf(lp, sizeof lp, "%s/bgi_log.txt", ip);
			freopen(lp, "w", stderr);
			setvbuf(stderr, NULL, _IONBF, 0);
		}
	}
	/* The build's opcodes this reimplementation does not cover yet are
	 * emulated while they are brought up (see Opcode_NotImplemented); on
	 * Android there is no environment to set, so enable it unless the
	 * caller asked otherwise. */
	setenv("BGI_STUB", "1", 0);
	if(!OS_Init())
		return -1;
	r = Engine_Main(cmdLine);
	OS_Shutdown();
	return r;
}
