# LumaForge

**Visual LED Mapping & Animation Firmware** — a local-first visual studio for
mapping a real addressable-LED installation and designing light on its geometry.
This first development release runs in a Linux/LXC container and simulates the
result in the browser. No cloud, account or telemetry is involved.

## What works

- Draw, edit, reverse, select and delete geometric LED strips.
- See physical direction and generated LED points; select a strip or individual LEDs.
- Save a selection as a reusable zone.
- Live color, brightness, speed, direction and effect preview over WebSocket.
- C++ implementations of Solid, Blink, Pulse, Wipe, Chase and Rainbow.
- Create scenes with parallel animation data, display timeline tracks and play scenes.
- Highest-priority-wins layer composition.
- Persist/import/export device, layout, zones and scenes as JSON.
- Undo/redo for editor changes and validation at the API boundary.

Effects are evaluated by the portable C++ core. The JavaScript UI only displays
frames, so the simulator does not contain a competing animation engine.

## Start in Linux/LXC

Requirements: Node.js 20+, npm, GNU Make and a C++17 compiler.

```bash
npm install
./dev.sh
```

Open **http://localhost:8080**. For another port use `PORT=8090 ./dev.sh`.
The server binds to `0.0.0.0`, so use the container IP from another machine.

## Test

```bash
./test.sh
```

The tests cover layout mapping and reversal, zone selection, scene scheduling,
layer priority/composition, effect progress, color serialization, atomic file
storage, and API-model validation.

## Project structure

```text
core/       Portable C++ domain, renderer, effects and simulator output
server/     LXC HTTP/WebSocket host and FileStorage adapter
web/        Dependency-free static visual editor
config/     Human-readable committed JSON state
firmware/   PlatformIO ESP32 firmware with web UI and physical LED output
docs/       Architecture notes
scripts/    Reserved for tooling
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for data flow and porting notes.

## Internal API (v1)

- `GET /api/config`, `/api/layout`, `/api/zones`, `/api/scenes`, `/api/project`
- `PUT /api/layout`, `/api/zones`, `/api/scenes`
- `POST /api/project/import`
- WebSocket `/ws`: `preview.set`, `preview.cancel`, `preview.apply`, `scene.play`;
  server messages include `hello`, `frame`, `error`, and resource updates.

## ESP32 provisioning prototype

The first flashable ESP32 firmware is in `firmware/`. It tries the Wi-Fi network
stored in NVS for 15 seconds. Without a working configuration it opens:

- SSID: `LumaForge-XXXXXX` (the suffix is unique per ESP32)
- Password: `lumaforge`
- Setup page: `http://192.168.4.1`

Select a network, enter its password and choose a device name. After restarting,
the device is available through its IP address and, where mDNS is supported, at
`http://<device-name>.local`. Credentials are never returned by the status API.

Build and flash a generic ESP32 development board over USB:

```bash
cd firmware
pio run
pio device list
pio run --target upload --upload-port /dev/ttyUSB0
pio device monitor --baud 115200 --port /dev/ttyUSB0
```

Some boards appear as `/dev/ttyACM0`. Replace the port accordingly. If the board
is an ESP32-S3, C3 or another specific variant, select the matching PlatformIO
board before flashing instead of using the current generic `esp32dev` target.

In normal Wi-Fi operation, `/` serves the embedded LumaForge editor. Its project
resources are persisted in LittleFS, while Wi-Fi credentials remain in NVS. The
network setup can then be opened from the editor settings or directly at `/setup`.
The device additionally exposes `GET /api/v1/info`, `GET /api/v1/device` and
`GET /api/v1/status`.
The editor REST resources, browser-based OTA and the portable C++ renderer are
present. Scene and preview frames are calculated on the ESP32, streamed back to
the browser over WebSocket and sent to the configured physical LED outputs.

After this firmware has been flashed once over USB, later application updates can
be installed from **Settings → Firmware aktualisieren (OTA)** or `/update`. Upload
only the `firmware.bin` built for the matching ESP32 board. Do not erase flash for
an OTA update; Wi-Fi credentials and LittleFS project data remain intact.

### Device Identity

Every ESP32 derives an immutable public `device_id` from its factory-programmed
eFuse base MAC. LumaForge formats the complete 48-bit value canonically, hashes
`LumaForge:<mac>` with SHA-256 and publishes the first 12 lowercase hexadecimal
hash characters with an `lf-` prefix, for example `lf-51bf60200d1e`. The raw MAC
is never exposed. Because the ID is derived rather than stored, it survives
restarts, firmware updates, Wi-Fi changes and erasure of NVS or LittleFS.

The user-editable `device_name` is independent of this identity. The stable
default hostname is `lumaforge-<first six hash characters>`, for example
`lumaforge-51bf60.local`.

### Network Discovery

In addition to `_http._tcp.local`, each connected device advertises the dedicated
service `_lumaforge._tcp.local` on TCP port 80. This service is intended for
Home Assistant, ioBroker and other local automation clients.

### TXT Records

The `_lumaforge._tcp` announcement contains:

| Field | Meaning |
| --- | --- |
| `id` | Stable public `device_id` |
| `name` | User-editable `device_name` |
| `api` | Independent API version, currently `1` |
| `fw` | Firmware version |
| `model` | Hardware model, currently `esp32` |
| `manufacturer` | `LumaForge` |
| `product` | `LumaForge` |
| `protocol` | `http` |

No credentials, tokens, Wi-Fi password or raw MAC address are announced.

### Device Info API

`GET /api/v1/info` is read-only and returns the stable discovery contract:

```json
{
  "product": "LumaForge",
  "device_id": "lf-51bf60200d1e",
  "device_name": "Garage",
  "model": "esp32",
  "firmware_version": "0.2.0-alpha.2",
  "api_version": 1,
  "hostname": "lumaforge-51bf60",
  "network": {
    "connected": true,
    "ip": "192.168.2.123",
    "rssi": -55
  },
  "capabilities": ["led_output", "scenes", "zones"]
}
```

### Discovery Example

On Linux with Avahi installed, discover and resolve all LumaForge devices with:

```bash
avahi-browse -rt _lumaforge._tcp
```

An integration reads the TXT `id`, resolves the advertised address and port,
then requests `GET /api/v1/info`. The `device_id`, not the IP address, hostname
or display name, is the durable identity used to recognize the device again.

## ESP32 roadmap

The mapping types, colors, effects, scheduler, layers, renderer and `LedOutput`
interface are standard C++17 and already portable. The PlatformIO target uses
Arduino for a pragmatic first port and includes Wi-Fi/AP provisioning, NVS,
mDNS, LittleFS storage, REST/WebSocket APIs, browser-based OTA, the embedded
editor UI and physical output through Adafruit NeoPixel.

Later roadmap items include advanced transitions/snapshots, draggable timeline
editing, blend modes, multiple physical outputs, MQTT/Home Assistant, trigger
adapters, RGBW calibration and optional WLED API compatibility.
