# System status API

`GET /api/v1/status` returns the current system metrics. The ESP32 also sends
the same payload with `type: "system.status"` immediately after a WebSocket
connection and every five seconds thereafter.

```json
{
  "type": "system.status",
  "version": "0.2.0-alpha.5",
  "cpuPercent": 18.4,
  "memoryUsedBytes": 49320,
  "memoryTotalBytes": 327680,
  "flashChipSizeBytes": 4194304,
  "firmwareUsedBytes": 1159941,
  "firmwareFreeBytes": 150779,
  "firmwareCapacityBytes": 1310720,
  "filesystemUsedBytes": 12288,
  "filesystemTotalBytes": 917504
}
```

The REST response omits only `type`. Consumers should prefer the WebSocket
event and must not poll this endpoint every second. A REST request at startup
and a slow fallback while the WebSocket is disconnected are sufficient.

Flash fields have distinct meanings:

- `flashChipSizeBytes`: complete physical flash chip.
- `firmwareUsedBytes`: currently installed sketch size.
- `firmwareFreeBytes`: unused space in the currently running app partition.
- `firmwareCapacityBytes`: size of the currently running app partition.
- `filesystemUsedBytes` and `filesystemTotalBytes`: LittleFS usage.

The simulator publishes the same schema but uses `null` for flash and
filesystem fields because host storage is not equivalent to ESP32 flash.

LED frame events are opt-in. An editor that needs the live LED visualization
sends `{"type":"frame.subscribe"}` after connecting and can later send
`{"type":"frame.unsubscribe"}`. Integrations such as Home Assistant should
not subscribe unless they actually consume the high-frequency pixel frames.
Status and automation events are sent independently of this subscription.
