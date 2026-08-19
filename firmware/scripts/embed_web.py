Import("env")

import gzip
from pathlib import Path

project = Path(env.subst("$PROJECT_DIR"))
source = project.parent / "web"
target = project / "src" / "generated_web.hpp"
assets = ["index.html", "app.css", "snap.css", "color-mode.css", "theme.css", "app.js", "logo.svg"]

lines = ["#pragma once", "#include <Arduino.h>", "namespace embedded_web {"]
entries = []
for index, name in enumerate(assets):
    content = (source / name).read_bytes()
    if name == "app.js":
        content = content.replace(
            b"new WebSocket(`${location.protocol==='https:'?'wss':'ws'}://${location.host}/ws`)",
            b"new WebSocket(`ws://${location.hostname}:81/`)",
        )
        content += '''\nconst renderFirmwareDeviceSettings=renderDeviceSettings;renderDeviceSettings=function(){renderFirmwareDeviceSettings();const panel=document.querySelector('#device-settings');if(panel&&!panel.querySelector('.firmware-network-settings')){const network=document.createElement('button');network.className='wide firmware-network-settings';network.textContent=locale==='de'?'WLAN ändern':'Change Wi-Fi';network.onclick=()=>location.href='/setup';const update=document.createElement('button');update.className='wide firmware-update';update.textContent=locale==='de'?'Firmware aktualisieren (OTA)':'Update firmware (OTA)';update.onclick=()=>location.href='/update';panel.append(network,update);}};\n'''.encode("utf-8")
    payload = gzip.compress(content, compresslevel=9, mtime=0)
    symbol = f"asset_{index}"
    values = ",".join(f"0x{byte:02x}" for byte in payload)
    lines.append(f"static const uint8_t {symbol}[] PROGMEM = {{{values}}};")
    mime = {"html": "text/html; charset=utf-8", "css": "text/css; charset=utf-8", "js": "text/javascript; charset=utf-8", "svg": "image/svg+xml"}[name.rsplit(".", 1)[1]]
    entries.append(("/" if name == "index.html" else f"/{name}", mime, symbol, len(payload)))
lines.append("struct Asset { const char* path; const char* mime; const uint8_t* data; size_t size; };")
lines.append("static const Asset assets[] = {")
for path, mime, symbol, size in entries:
    lines.append(f'  {{"{path}", "{mime}", {symbol}, {size}}},')
lines.extend(["};", "}"])
target.write_text("\n".join(lines) + "\n")
