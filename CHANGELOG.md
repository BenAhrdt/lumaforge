# Changelog

Alle wichtigen Änderungen an LumaForge werden in dieser Datei dokumentiert.
Das Projekt verwendet [Semantic Versioning](https://semver.org/lang/de/).

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

[0.2.0-alpha.2]: https://github.com/BenAhrdt/lumaforge/compare/v0.2.0-alpha.1...v0.2.0-alpha.2
[0.2.0-alpha.1]: https://github.com/BenAhrdt/lumaforge/compare/v0.1.0-alpha.1...v0.2.0-alpha.1
[0.1.0-alpha.1]: https://github.com/BenAhrdt/lumaforge/releases/tag/v0.1.0-alpha.1
