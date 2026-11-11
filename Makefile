# NPClient64.dll - Windows FreeTrack / NaturalPoint protocol DLL
#
# Source: opentrack's contrib/npclient/npclient.c at commit 37d15b7a
#         ("add npclient source", 2015-06-02), plus npclient_udp.c which
#         replaces the shared-memory pose read with an OpenTrack UDP poll.
#         This is the revision whose behaviour matches the prebuilt
#         bin/NPClient64.dll: DllMain enables timeBeginPeriod/timeEndPeriod
#         and keeps a persistent hFTMutex.
#
# Build on Linux / EndeavourOS with the MinGW-w64 cross toolchain:
#     sudo pacman -S mingw-w64-gcc
#
# Produces:
#     NPClient64.dll (Windows 64-bit, PE32+)
#
# The compile/link flags mirror upstream's contrib/npclient/COMPILE.TXT
# (clang -> gcc) and the DLL is stripped the same way.  -lwinmm is needed
# because this source enables the timeBeginPeriod/timeEndPeriod calls;
# -lws2_32 is needed for the OpenTrack UDP receiver.

CC      = x86_64-w64-mingw32-gcc
STRIP   = x86_64-w64-mingw32-strip

CFLAGS = -Wall -Wextra -Wpedantic -Os -fomit-frame-pointer -mtune=generic \
         -ffunction-sections -fdata-sections -fvisibility=hidden \
         -fno-math-errno -fno-trapping-math -fmerge-all-constants \
         -fno-stack-protector -shared

LDFLAGS = -Wl,--kill-at,--nxcompat,--dynamicbase,--as-needed,--gc-sections,--no-insert-timestamp
LIBS    = -lwinmm -lws2_32

STRIPFLAGS = --strip-all --remove-section=.eh_frame

NPCLIENT64 = NPClient64.dll
DEF        = NPClient64.def
SOURCES    = npclient.c npclient_udp.c

.PHONY: all clean

all: $(NPCLIENT64)

$(NPCLIENT64): $(SOURCES) $(DEF)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) $(DEF) $(LIBS)
	$(STRIP) $(STRIPFLAGS) $@

clean:
	rm -f $(NPCLIENT64) npclient.o npclient_udp.o
