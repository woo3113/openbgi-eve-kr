#!/usr/bin/env bash
# Build the OpenBGI command-line tools (bpasm, bgiscn) for 64-bit Windows.
set -e
export PATH=/mingw64/bin:/usr/bin:$PATH
cd /c/Users/woo31/openbgi
make PLATFORM=win32 CROSS=x86_64-w64-mingw32- tools
