# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`video_tools` is a cross-platform C++17 + Qt6 desktop tool for A/V dev/test workflows: it records a UDP stream
(unicast or multicast) byte-for-byte into a standard `.pcap` file, and replays a `.pcap` back out to a
destination IP:port with the original inter-packet timing preserved. It never decodes or cares about the payload
codec — fidelity is at the raw-packet level, which is why capture is stored as pcap (with synthesized
Ethernet/IPv4/UDP headers) rather than a raw concatenated payload dump: UDP datagram boundaries carry semantic
meaning that must round-trip exactly.

No libpcap/Npcap dependency — pcap frames are synthesized/parsed by hand (`src/pcap`), which avoids needing
admin rights or a driver install (especially on Windows) just to capture/inject UDP traffic already visible at
the socket layer.

## Build

Qt6 is not on the default CMake search path on this machine; point at the installed copy explicitly:

```sh
cmake -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/macos
cmake --build build
```

Targets produced: `recorder_cli`, `player_cli`, `video_tools_gui`, plus the test binaries below.

`PACKETVCR_BUILD_GUI` and `PACKETVCR_BUILD_TESTS` (both default `ON`) gate whether Qt is required at
all: with both `OFF`, `pcap`/`net`/`core`/`cli` build with zero Qt dependency and no
`CMAKE_PREFIX_PATH` needed. Useful when iterating on core/CLI logic without a Qt install on hand.

## Test

```sh
ctest --test-dir build              # run all tests
ctest --test-dir build -R test_pcap_roundtrip   # run a single test binary by name
./build/tests/test_pcap_roundtrip   # or invoke a test binary directly; Qt Test supports e.g. -functionname
```

Test binaries: `test_checksum`, `test_pcap_roundtrip` (both `pcap`-only, Qt Test-based), and
`test_player_timing` (drives a real `core::Player` against a loopback `net::PlatformSocket` receiver to assert
anti-drift scheduling, pause/resume, and speed-multiplier behavior — these are timing-sensitive and read real
wall-clock deltas, so expect some tolerance windows rather than exact equality).

## Architecture

Layered static libraries, each with a strict dependency direction (lower layers never depend on higher ones):

```
src/pcap  →  src/net  →  src/core  →  src/cli / src/gui
```

- **`src/pcap`** (`video_tools_pcap`) — classic libpcap file format (not pcapng): `PcapFormat.h` has the on-disk
  struct layout and endian helpers; `FakeHeaders` synthesizes/parses a minimal Ethernet II + IPv4 + UDP frame
  around a raw UDP payload purely so captures are Wireshark-readable (only Ethernet MACs and IPv4 TTL are
  fabricated placeholders — IPs/ports are always the real observed values); `PcapWriter`/`PcapReader` do the
  file I/O. Zero Qt dependency.
- **`src/net`** (`video_tools_net`) — `PlatformSocket` is a thin cross-platform UDP socket wrapper (POSIX BSD
  sockets / Winsock2), `NetworkInterfaceInfo` enumerates NICs (`getifaddrs`/`GetAdaptersAddresses`) to populate
  the GUI's interface picker, `IPv4Address` has dotted-quad parse/format helpers. IPv4 addresses are
  represented as host-order `uint32_t` everywhere in this module and in `pcap::FrameParams`. Zero Qt dependency
  **by design** — see below.
- **`src/core`** (`video_tools_core`) — `Recorder` and `Player`, each a self-contained worker-thread state
  machine (construct → `setLogCallback` → `start()` → poll `stats()`/`isRunning()` → `stop()`) driven identically
  by both the CLI and the GUI. Zero Qt dependency.
  - `Player` loads and parses the whole pcap file synchronously in `start()` (so bad/missing files fail fast),
    precomputing each packet's offset from the first packet's timestamp, then schedules sends on the worker
    thread using **absolute-deadline scheduling anchored to a fixed start point** — bounded per-packet jitter,
    no cumulative drift over a long file. Pause/resume freezes in place and shifts all subsequent deadlines by
    the pause duration without touching relative inter-packet timing.
- **`src/cli`** — `recorder_cli` / `player_cli`, thin argv-parsing wrappers around `core::Recorder` /
  `core::Player` with SIGINT-driven graceful shutdown and periodic stats printed to stderr.
- **`src/gui`** — Qt6 Widgets (`QMainWindow` + tabbed Recorder/Player panels, not QML), driving the same
  `core::Recorder`/`core::Player` classes as the CLI. This is the only layer allowed to depend on Qt beyond
  `Qt6::Test` in the timing test.

**Qt-free by design below `src/gui`**: `QUdpSocket` caused packet loss/latency in a prior project, so all
network I/O goes through native OS socket APIs (`src/net/PlatformSocket`) directly rather than Qt::Network. Keep
new capture/playback/network logic in `src/pcap`/`src/net`/`src/core` Qt-free; only reach for Qt in `src/gui`.
