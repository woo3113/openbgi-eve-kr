cmake_minimum_required(VERSION 3.6)
project(openbgi C)

# The OpenBGI engine sources live outside the Android project tree (the
# repository root is three levels up from app/jni/src).
set(OPENBGI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." CACHE PATH "OpenBGI source root")
set(OB_SRC "${OPENBGI_ROOT}/src")
set(OB_INC "${OPENBGI_ROOT}/inc")

# the platform-independent engine (everything but the desktop entry point)
file(GLOB_RECURSE OB_ENGINE CONFIGURE_DEPENDS
	"${OB_SRC}/core/*.c" "${OB_SRC}/asm/*.c" "${OB_SRC}/scn/*.c" "${OB_SRC}/bpc/*.c"
	"${OB_SRC}/vm/*.c" "${OB_SRC}/wait/*.c" "${OB_SRC}/sys/*.c" "${OB_SRC}/sysobj/*.c"
	"${OB_SRC}/gfx/*.c" "${OB_SRC}/snd/*.c" "${OB_SRC}/dbg/*.c")
list(REMOVE_ITEM OB_ENGINE "${OB_SRC}/sys/main.c")

# the OS layer: the shared helpers plus the Android back end
file(GLOB OB_OS CONFIGURE_DEPENDS "${OB_SRC}/os/os_common.c" "${OB_SRC}/os/android/*.c")

add_library(main SHARED ${OB_ENGINE} ${OB_OS})

target_include_directories(main PRIVATE
	"${OB_INC}"
	"${OB_SRC}/os/android")

target_compile_options(main PRIVATE -std=c99 -O2 -Wall -Wno-unused-parameter -DBGI_USE_STB_VORBIS)

target_link_libraries(main SDL2 log android GLESv2 m)
