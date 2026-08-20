# Automation sequences

Automations are stored by `GET /api/automations` and replaced as a complete list
by `PUT /api/automations`. Legacy objects with one `sceneId` remain readable.
New automations use a non-empty `steps` array:

```json
{
  "id": "arrival",
  "name": "Arrival",
  "trigger": "manual",
  "time": "18:00",
  "enabled": true,
  "steps": [
    { "sceneId": "welcome", "advance": "scene_finished" },
    { "sceneId": "accent", "advance": "after_delay", "durationSeconds": 30 },
    { "sceneId": "idle", "advance": "manual" }
  ]
}
```

`advance` is one of:

- `scene_finished`: advance after the last finite animation ends. A scene with a
  looping scene or animation does not finish automatically.
- `after_delay`: stop the current scene and advance after the positive
  `durationSeconds` value.
- `manual`: wait for an `automation.next` command.

The ESP32 WebSocket server is at `ws://<device>:81/`. The simulator uses
`ws://<host>/ws`. Commands are JSON objects:

```json
{ "type": "automation.start", "automationId": "arrival" }
{ "type": "automation.next" }
{ "type": "automation.stop" }
```

Accepted commands receive `<command>.accepted`. Errors receive
`{"type":"error","message":"..."}`. State is broadcast when a client
connects and whenever an automation starts, advances, finishes, or stops:

```json
{
  "type": "automation.state",
  "automationId": "arrival",
  "state": "running",
  "stepIndex": 1,
  "sceneId": "accent",
  "elapsedSeconds": 4.2
}
```

Stopped state always includes `automationId`, `stepIndex`, and `sceneId` with
`null` values and `elapsedSeconds` with the value `0` on both ESP32 and simulator.
Starting a standalone scene cancels the current automation. `scene.stop` also
stops an active automation. Daily time triggers are currently initiated by the
open browser editor; sequence execution after it starts runs on the device.
