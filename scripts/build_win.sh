#!/usr/bin/env bash
# Build OpenBGI for 64-bit Windows with the MSYS2 mingw-w64 toolchain.
set -e
export PATH=/mingw64/bin:/usr/bin:$PATH
cd /c/Users/woo31/openbgi
rm -rf obj/win32 bin/bgi.exe
make win CROSS=x86_64-w64-mingw32- "$@"
