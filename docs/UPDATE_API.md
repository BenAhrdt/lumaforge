# Firmware update API

The ESP32 checks `update-manifest.json` after connecting and every six hours.
WebSocket commands are `{"type":"update.check"}` and
`{"type":"update.install","version":"<version>"}`. The device publishes
`update.status` with `state`, `currentVersion`, `latestVersion`, `releaseUrl`,
`sizeBytes`, optional `progress`, and optional `error`. States are `idle`,
`checking`, `available`, `up_to_date`, `downloading`, `installing`, `restarting`,
and `failed`. The manifest is authenticated with GitHub TLS; the downloaded
binary is accepted only when its size and SHA-256 digest match the manifest.
