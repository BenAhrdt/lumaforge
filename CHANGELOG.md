# Changelog

Alle wichtigen Änderungen an LumaForge werden in dieser Datei dokumentiert.
Das Projekt verwendet [Semantic Versioning](https://semver.org/lang/de/).

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

[0.1.0-alpha.1]: https://github.com/BenAhrdt/lumaforge/releases/tag/v0.1.0-alpha.1
