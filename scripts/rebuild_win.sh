#!/usr/bin/env bash
# Incremental rebuild of the 64-bit Windows engine.
set -e
export PATH=/mingw64/bin:/usr/bin:$PATH
cd /c/Users/woo31/openbgi
make win CROSS=x86_64-w64-mingw32- "$@"
