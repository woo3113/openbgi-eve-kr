/*
 * This file is part of OpenBGI (https://openbgi.net).
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * android_internal.h - the common includes and the shared state of the
 *                      Android back end (src/os/android/).  Derived from
 *                      the POSIX back end's posix_internal.h; the
 *                      character conversions use a generated CP932 table
 *                      instead of iconv (see sjis.c).
 *
 * The directories below are set once by OS_Init (process.c): the game
 * directory is the root of the one drive "C:" this back end presents, the
 * configuration directory holds the registry file and the instance lock
 * files.
 */
#ifndef BGI_OS_ANDROID_INTERNAL_H
#define BGI_OS_ANDROID_INTERNAL_H

#include "bgi/os.h"
#include "bgi/os_common.h"

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <sched.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <limits.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// the CPUID / RDTSC intrinsics of machine.c exist on x86 only
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#include <x86intrin.h>
#define BGI_X86 1
#endif

// set by OS_Init (process.c)
extern char gPosixExeDir[PATH_MAX];    // the game directory with a trailing '/': the one drive this layer presents
extern char gPosixConfigDir[PATH_MAX]; // the configuration directory
extern char gPosixRegFile[PATH_MAX];   // the registry emulation's file (registry.c)

void OsAndroid_SjisInit(void);     // fill the CP932 table (sjis.c)
void OsAndroid_SjisShutdown(void); // release it
void OsAndroid_SjisToUtf8(const char* sjis, char* out, size_t n);
uint32_t OsAndroid_SjisChar(uint16_t code);
int OsAndroid_UnicodeToSjis(uint32_t cp, char out[2]);

// the Shift-JIS Windows path -> POSIX path conversion (paths.c)
void OsAndroid_Path(const char* win, char* out, size_t n);

// the configuration directory (process.c)
const char* OsAndroid_ConfigDir(void);
// SIGTERM / SIGINT -> the window's close request (window.c)
void OsAndroid_RequestClose(void);

// fonts (font.c)
void OsAndroid_FontShutdown(void);

/* the default game and configuration directories (window.c, through SDL):
 * the game under the external storage the user copied it to, the config
 * under the app's internal storage.  Both get a trailing separator-free
 * POSIX path; n is the buffer size. */
void OsAndroid_DefaultDirs(char* gameDir, size_t gN, char* configDir, size_t cN);

#endif // BGI_OS_ANDROID_INTERNAL_H
