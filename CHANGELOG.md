# Changelog

Alle wichtigen Änderungen an LumaForge werden in dieser Datei dokumentiert.
Das Projekt verwendet [Semantic Versioning](https://semver.org/lang/de/).

## [0.2.0-alpha.5] – 2026-08-20

### Behoben

- Das sekündliche REST-Statuspolling der Weboberfläche wurde entfernt; bei aktiver WebSocket-Verbindung kommen Statuswerte ausschließlich über `system.status` im Fünf-Sekunden-Takt.
- LittleFS-Belegungswerte werden gecacht, sodass Statusabfragen keine wiederholten, teuren Dateisystemscans mehr auslösen.
- Die Firmware-Kapazität wird aus der tatsächlich laufenden ESP32-App-Partition ermittelt und nicht mehr fälschlich aus Sketchgröße und OTA-Freiraum addiert.
- Hochfrequente LED-Frame-Ereignisse werden nur noch an ausdrücklich angemeldete Editor-Clients gesendet. Home-Assistant-Verbindungen erhalten weiterhin Status- und Automationsereignisse ohne Pixel-Datenlast.
- REST-Fallback-Abfragen der Weboberfläche erfolgen bei getrennter WebSocket-Verbindung nur noch alle 30 Sekunden.

## [0.2.0-alpha.4] – 2026-08-20

### Hinzugefügt

- Flash-, Firmware-/OTA- und LittleFS-Belegung in REST-API, WebSocket und Weboberfläche.
- Regelmäßiges WebSocket-Ereignis `system.status` für Integrationen.
- Automatischer, TLS-gesicherter Updatecheck über ein GitHub-Manifest.
- WebSocket-Befehle `update.check` und `update.install` sowie Fortschrittsereignisse `update.status`.
- SHA-256-Prüfung des Firmwareimages vor der Installation und Neustart.
- Bedienung für Updateprüfung und bestätigte Installation in den Geräteeinstellungen.

## [0.2.0-alpha.3] – 2026-08-20

### Behoben

- Die ESP32-Firmware sendet beim Stoppen einer Automation nun immer ein vollständiges `automation.state`-Ereignis.
- Der gestoppte Zustand enthält einheitlich `automationId: null`, `stepIndex: null`, `sceneId: null` und `elapsedSeconds: 0`, damit Integrationen den vorherigen Laufzustand zuverlässig zurücksetzen können.
- Die initiale Statusmeldung nach einer WebSocket-Verbindung verwendet dasselbe vollständige Schema wie der Simulator.

## [0.2.0-alpha.2] – 2026-08-20

### Hinzugefügt

- Mehrstufige geräteinterne Automationen mit beliebig vielen Szenenschritten.
- Schrittwechsel nach Szenenende, nach einer festgelegten Zeit oder per manuellem Befehl.
- WebSocket-Befehle `automation.start`, `automation.stop` und `automation.next`.
- Laufzeitereignis `automation.state` mit Automation, Schritt, Szene und verstrichener Zeit.
- Editor zum Anlegen und Bearbeiten mehrstufiger Automationssequenzen.
- Capabilities `automations` und `automation_sequences` für Integrationen.
- Technische Dokumentation des Automations-API-Vertrags.

### Kompatibilität

- Bestehende Automationen mit einer einzelnen `sceneId` bleiben lesbar.
- Der tägliche Zeitauslöser wird weiterhin vom geöffneten Web-Editor angestoßen; gestartete Sequenzen laufen anschließend autonom auf dem Gerät.

## [0.2.0-alpha.1] – 2026-08-19

### Hinzugefügt

- Stabile, hardwaregebundene Geräte-ID aus der vollständigen ESP32-eFuse-Basis-MAC.
- Öffentliche Gerätekennung als gekürzter SHA-256-Hash, ohne die rohe MAC-Adresse offenzulegen.
- Dedizierte LumaForge-Erkennung über `_lumaforge._tcp.local` zusätzlich zum HTTP-mDNS-Service.
- mDNS-TXT-Metadaten für ID, Gerätename, API, Firmware, Modell, Hersteller, Produkt und Protokoll.
- Explizite, von der Firmware-Version unabhängige API-Versionierung.
- Read-only-Endpunkt `GET /api/v1/info` mit Identität, Netzwerkstatus und realen Gerätefähigkeiten.
- Anzeige der unveränderlichen Discovery-Geräte-ID in den Einstellungen.
- Direkt editierbarer Gerätename mit persistenter Speicherung und sofortiger Aktualisierung der mDNS-Metadaten.
- Einheitliche Geräteinfo- und Namens-API für ESP32-Firmware und Simulator.
- Discovery-Grundlage für zukünftige Home-Assistant-Integrationen und ioBroker-Adapter.

## [0.1.0-alpha.1] – 2026-08-14

Erste öffentliche Alpha-Version.

### Hinzugefügt

- Visueller Editor zum Zeichnen und Konfigurieren mehrerer LED-Abschnitte.
- GPIO-Stränge mit den gemeinsamen ESP32-/ESP8266-Ausgängen GPIO 4, 5, 13 und 14.
- Einstellbare Farbreihenfolge pro GPIO-Strang: RGB, GRB, BRG, RBG, GBR und BGR.
- Zonen mit stabiler LED-Zuordnung bei Größenänderungen von Abschnitten.
- Szenen, parallele Animationen und Automationen.
- Effekte wie Statisch, Blinken, Pulsieren, Wischen, Chase, Regenbogen und Scanner.
- Live-Vorschau sowie physische WS2812B-/NeoPixel-Ausgabe auf dem ESP32.
- WLAN-Einrichtung, mDNS, LittleFS-Speicherung und Browser-OTA.
- Helles und dunkles Oberflächendesign.
- CPU-, RAM- und Versionsanzeige in der Kopfleiste.
- JSON-Import und -Export für Projekte.
- Node.js-Simulator und portabler C++-Animationskern.

### Hinweise

- Dies ist eine frühe Alpha-Version. Projektformate und Bedienung können sich noch ändern.
- Vor dem OTA-Update muss die Firmware zum verwendeten ESP32-Board passen.

[0.2.0-alpha.5]: https://github.com/BenAhrdt/lumaforge/compare/v0.2.0-alpha.4...v0.2.0-alpha.5
[0.2.0-alpha.4]: https://github.com/BenAhrdt/lumaforge/compare/v0.2.0-alpha.3...v0.2.0-alpha.4
[0.2.0-alpha.3]: https://github.com/BenAhrdt/lumaforge/compare/v0.2.0-alpha.2...v0.2.0-alpha.3
[0.2.0-alpha.2]: https://github.com/BenAhrdt/lumaforge/compare/v0.2.0-alpha.1...v0.2.0-alpha.2
[0.2.0-alpha.1]: https://github.com/BenAhrdt/lumaforge/compare/v0.1.0-alpha.1...v0.2.0-alpha.1
[0.1.0-alpha.1]: https://github.com/BenAhrdt/lumaforge/releases/tag/v0.1.0-alpha.1
