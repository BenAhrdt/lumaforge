# System status API

`GET /api/v1/status` returns the current system metrics. The ESP32 also sends
the same payload with `type: "system.status"` immediately after a WebSocket
connection and every five seconds thereafter.

```json
{
  "type": "system.status",
  "version": "0.2.0-alpha.4",
  "cpuPercent": 18.4,
  "memoryUsedBytes": 49320,
  "memoryTotalBytes": 327680,
  "flashChipSizeBytes": 4194304,
  "firmwareUsedBytes": 998656,
  "firmwareFreeBytes": 321120,
  "firmwareCapacityBytes": 1319776,
  "filesystemUsedBytes": 12288,
  "filesystemTotalBytes": 917504
}
```

The REST response omits only `type`. Flash fields have distinct meanings:

- `flashChipSizeBytes`: complete physical flash chip.
- `firmwareUsedBytes`: currently installed sketch size.
- `firmwareFreeBytes`: space available to the firmware/OTA app partition.
- `firmwareCapacityBytes`: used plus free app-partition capacity.
- `filesystemUsedBytes` and `filesystemTotalBytes`: LittleFS usage.

The simulator publishes the same schema but uses `null` for flash and
filesystem fields because host storage is not equivalent to ESP32 flash.
