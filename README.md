# opentrack-npclient-udp

A solution for Linux gamers who wish to use native Linux [OpenTrack](https://github.com/opentrack/opentrack) with Windows games running under Proton (Steam).

It provides a drop-in replacement for `NPClient64.dll` (the TrackIR/NaturalPoint NPClient API used by many games) that receives head-tracking data from OpenTrack over UDP instead of Windows shared memory.

## How it works

```text
+---------------------------------------------------------------------------------------+
| Linux                                                                                 |
|                                                                                       |
|  +----------------------+                               +---------------------------+ |
|  | Opentrack            |                               |  Proton prefix            | |
|  |                      |      127.0.0.1 (localhost)    |  +---------------------+  | |
|  | UDP Sender           | --------------------------->  |  |  Windows Game       |  | |
|  |                      |            UDP packets        |  |  +----------------+ |  | |
|  +----------------------+                               |  |  | NPClient64.dll | |  | |
|                                                         |  |  | UDP listener   | |  | |
|                                                         |  |  | on port 4242   | |  | |
|                                                         |  |  +---------------+| |  | |
|                                                         |  +---------------------+  | |
|                                                         +---------------------------+ |
+---------------------------------------------------------------------------------------+
```

## How to use

1. **Configure OpenTrack (Linux, native):**
   - Select the **UDP over network** output.
   - Address: `127.0.0.1`
   - Port: `4242`

2. **Place `NPClient64.dll` where the game expects it.**
   - For **Euro Truck Simulator 2 (ETS2)** running in Proton, the game loads the DLL from the path stored in the Proton prefix registry file:
     - `~/.steam/steam/steamapps/compatdata/227300/pfx/user.reg`
     - Key: `[Software\\NaturalPoint\\NATURALPOINT\\NPClient Location]`
   - The value must point to the directory containing `NPClient64.dll` (and end with a path separator, e.g. `C:\npclient-udp\` inside the prefix). Put the DLL in that location (or update the key to point to where you placed it). Make sure the game is closed before editing `user.reg`.
   - Placing the file within the Linux filesystem and specifying Z:\ path did not work for me.
   - For **American Truck Simulator (ATS)**, the equivalent prefix is typically `compatdata/270880`.
   - Other games may look in different locations; place the DLL according to that game’s NPClient/TrackIR expectations.

3. **Start OpenTrack and the game in any order.**
   - The game might still report missing head tracking hardware in "interface" options menu, but head tracking should work.

> **Warning:** While this solution has been reported to work for single-player SCS games (ETS2/ATS), modifying/replacing game-loaded DLLs may trigger anti-cheat systems in other games. Use at your own risk.

## Technical details

### Source provenance

`npclient.c` is based on OpenTrack at commit
[`37d15b7a32a63994ec292499e500e967c9313803`](https://github.com/opentrack/opentrack/commit/37d15b7a32a63994ec292499e500e967c9313803)
("add npclient source", 2015-06-02), where it lived at
`clientfiles/very-important-source-code/npclient.c` (later moved to
`contrib/npclient/npclient.c`). The only behavioural change is in `NP_GetData`,
which now calls `npclient_udp_poll()` instead of reading the shared-memory mapping.

OpenTrack in turn obtained this code from the
[linuxtrack](https://github.com/uglyDwarf/linuxtrack) project by uglyDwarf; see
`contrib/npclient/README-CREDIT.txt` in OpenTrack.

That 2015 revision was chosen deliberately. It is the source revision whose
behaviour matches OpenTrack's prebuilt `bin/NPClient64.dll`: it enables
`timeBeginPeriod`/`timeEndPeriod`, keeps a persistent `hFTMutex`, and advances
the frame counter by 10. `NPClient64.def` reconstructs the prebuilt binary's
export table, including the otherwise-unexported `DllMain` at ordinal 1.

### How it works (implementation)

`NP_GetData` polls a non-blocking UDP socket on every call. OpenTrack's
"UDP over network" output sends one 48-byte datagram per pose, containing six
little-endian doubles:

    [ TX, TY, TZ, Yaw, Pitch, Roll ]

Translations are centimetres and rotations are degrees. The DLL converts them
to the units OpenTrack's FreeTrack protocol stores in shared memory
(translations ×10, yaw and pitch negated, pitch clamped near +90°), then applies
the existing NPClient scaling. If no packet is pending, the last valid pose is
returned again, so a slow sender or scheduling jitter cannot cause a one-frame
snap to centre.

The listening port is fixed in `npclient_udp.c`:

```c
#define NPCLIENT_UDP_PORT 4242
```

The DLL still creates the `FT_SharedMem` mapping for compatibility, but pose
data comes only from UDP. The DLL binds to `127.0.0.1:4242`, so packets arriving
on another interface are ignored.

### Building

Requires MinGW-w64 (e.g. on Arch: `sudo pacman -S mingw-w64-gcc`).

```sh
make          # produces NPClient64.dll
make clean
```

The build is 64-bit only, mirrors OpenTrack's `contrib/npclient/COMPILE.TXT`
flags, links `-lwinmm -lws2_32`, and strips the result.

Known non-behavioural differences from the prebuilt binary: this build uses the
UCRT instead of `msvcrt.dll`, sets `DYNAMIC_BASE`/`NX_COMPAT`, and is smaller.

### Limitations

- Fixed UDP port 4242; change the constant and rebuild to use another port.
- 64-bit only. 32-bit games need a separate 32-bit build.

## Licensing

The C source comes from two projects with permissive licenses; both notices
apply:

- **linuxtrack** — original `npclient.c` code, MIT License,
  Copyright (c) 2009 Tulthix, uglyDwarf.
- **OpenTrack** — later modifications and the build recipe, ISC License,
  Copyright (c) 2012–2016 Stanislaw Halik.

### MIT License (linuxtrack)

Copyright (c) 2009 Tulthix, uglyDwarf

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

### ISC License (OpenTrack)

Copyright (c) 2012–2016 Stanislaw Halik

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

### Trademarks and third-party code

`NPClient`, `TrackIR`, and `NaturalPoint` are trademarks of their respective
owners. No NaturalPoint source code or binaries are included here; the export
interface was reconstructed from publicly documented behaviour and
reverse-engineered headers (via `winedump`) distributed with OpenTrack. This
project is not affiliated with or endorsed by NaturalPoint.
