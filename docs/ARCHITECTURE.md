# LumaForge architecture

LumaForge uses a portable C++17 domain core. Layout points map visual geometry to
physical indices. Effects produce logical layer frames; the compositor resolves
overlaps by priority into a frame buffer and sends it through `LedOutput`.

```text
Browser editor ── REST/WS ── LXC server ── line protocol ── C++ core
      ▲                              ▲                         │
      └──────── frame events ────────┴── SimulatorLedOutput ◀──┘

                                    ESP32 app ── Esp32LedOutput (stub)
```

The browser never implements effects. The server owns JSON persistence and API
validation; storage is behind a small adapter so LittleFS can replace host files.
The engine protocol is deliberately narrow and replaceable by direct linking on
ESP32. Frames reuse allocated vectors; the render loop does not allocate per LED.

## Main domain objects

- `Layout` / `Strip`: geometry and logical-to-physical mapping.
- `Zone`: a named set of logical LED IDs.
- `Animation`: effect, timing, target, colors, speed and direction.
- `Scene`: parallel animations plus start/end behavior metadata.
- `Layer`: a sparse frame and priority; highest priority wins.
- `Renderer`: scheduler, effect evaluation and composition.
- `LedOutput`: hardware-independent output boundary.

The first ESP32 port should use PlatformIO with Arduino: it provides pragmatic
FastLED/NeoPixel, Wi-Fi, filesystem and web-server support while preserving this
standard C++ core. An ESP-IDF adapter can be added without changing domain types.
