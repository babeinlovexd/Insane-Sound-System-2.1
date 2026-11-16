<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# Insane Sound System 2.1

[English](README_en.md) · [Fragen & Fehler](https://github.com/babeinlovexd/Insane-Sound-System-2.1/issues) · [Releases](https://github.com/babeinlovexd/Insane-Sound-System-2.1/releases)

Ein **auf echter Hardware vom Entwickler erprobtes 2.1-Audio-Testprojekt**. ESP32-S3, ESP32-WROOM-32D und RP2354A trennen ESPHome-/Netzwerksteuerung, Bluetooth-Empfang und DSP. Es ist eine eigene experimentelle Hardwareplattform und unterscheidet sich vom Sound System V5.

## Architektur

| Controller | Aufgabe |
| :--- | :--- |
| ESP32-S3 | Home Assistant/ESPHome, Netzwerkaudio, Display, Encoder, Temperaturen und Steuerung |
| ESP32-WROOM-32D | Bluetooth-A2DP-Eingang und Metadaten |
| RP2354A | Audiorouting, DSP sowie Verstärker-, LED- und Lüftersteuerung |

Das Design enthält einen Fünfband-EQ für Frontkanäle, Subwoofer-Tiefpass, zwei MA12070P-Verstärker und einen S/PDIF-Eingang. Tatsächliche Latenz und Verhalten hängen von Firmware und Hardware ab; daraus folgt keine Zusage latenzfreier oder universell getesteter Funktion.

## Hardware & Dateien

Die ausführliche [Pin-Belegung](Firmware/Pin_Mapping.md) beschreibt die Controllerverbindungen. Für den konkreten Aufbau mit Haupt-YAML und Firmwarequellcode des verwendeten Stands abgleichen.

- [ESPHome/insane-sound-system-2.1.yaml](ESPHome/insane-sound-system-2.1.yaml): S3-Konfiguration.
- [Firmware/WROOM32D](Firmware/WROOM32D): PlatformIO-Umgebung `esp32dev`.
- [Firmware/RP2354A](Firmware/RP2354A): PlatformIO-Umgebung `rp2354`.
- [InsaneFlasher](InsaneFlasher): Desktop-Tool und Proxy-Flasher-Skripte.

## Installation

1. Hardwareaufbau und Pinbelegung vor dem Flashen kontrollieren.
2. Haupt-YAML nach ESPHome kopieren und eine lokale `secrets.yaml` mit `api_encryption_key`, `ota_password`, `wifi_ssid`, `wifi_password` und `ap_password` anlegen.
3. `ha_player_id` anpassen, Konfiguration validieren/kompilieren und den S3 erstmals per USB flashen.
4. Passende `wroom.bin` und `rp2354.bin` aus diesem Repository bauen oder aus dessen Releases beziehen. Jeden Zielchip über die zu Aufbau und Bootloader passende Schnittstelle flashen.
5. S3 in Home Assistant hinzufügen und Quellenumschaltung, Lautstärke, Kühlung und Fehlereingänge einzeln prüfen.

## Firmware bauen

Mit installiertem PlatformIO im Repository-Stamm ausführen:

```bash
pio run -d Firmware/WROOM32D -e esp32dev
pio run -d Firmware/RP2354A -e rp2354
```

Die WROOM-Umgebung fixiert `espressif32@6.12.0` (Arduino 2.0.17) und ESP32-A2DP `v1.8.12`, passend zur im Projekt verwendeten älteren I2S-Schnittstelle.

Die S3-YAML wird separat mit ESPHome gebaut. Die PlatformIO-Ausgaben liegen unter `.pio/build/<Umgebung>/` im jeweiligen Firmwareordner. Release-Binaries gehören jeweils zum bezeichneten Chip und sind nicht austauschbar.

## InsaneFlasher & Proxy-Voraussetzungen

Der Python-Quellcode liegt unter [InsaneFlasher/Linux_Mac/InsaneFlasher.py](InsaneFlasher/Linux_Mac/InsaneFlasher.py). Benötigte Pakete: `customtkinter requests esptool zeroconf pillow pyserial`. Aus dem eigenen Verzeichnis starten, damit Grafiken gefunden werden.

Das Tool prüft die Releases **dieses 2.1-Repositories** und wählt `wroom.bin` bzw. `rp2354.bin` anhand des exakten Dateinamens. Bereits heruntergeladene EXE-Dateien ändern sich durch Quellcodekorrekturen nicht; verwende eine zum Release gehörende neu gebaute EXE.

Die WROOM-Proxyverbindung nutzt TCP-Port **6666**, der RP2354-Pfad **6667**. **Serielles RP2354-Proxy-Flashen setzt einen kompatiblen eigenen Bootloader voraus.** Die native ROM implementiert das hier verwendete rohe TCP-zu-UART-Datenprotokoll nicht. Eine erfolgreiche Netzwerkübertragung belegt deshalb noch kein erfolgreiches DSP-Flashen. Ohne passenden Bootloader ist der nativ unterstützte Flashweg zu verwenden.

## Fehlersuche

- **Falsche Firmware/Update:** 2.1-Repository, Ziel-Dateiname und Tool-Build kontrollieren; V5-Binaries sind kein Ersatz.
- **DSP startet nach Proxytransfer nicht:** Installierten Bootloader prüfen und passenden lokalen Flashweg nutzen.
- **Quellenumschaltung oder Bedienung:** I2S-, Metadaten-UART- und DSP-Steuerleitungen getrennt prüfen.
- **Temperatur-/Fehlermeldungen:** Tatsächlichen Sensortyp und Adresse mit YAML/Bestückung abgleichen, bevor Schutzparameter geändert werden.

## Lizenz

[CC BY-NC-SA 4.0](LICENSE). Es gilt die bestehende Projektlizenz. Drittanbieter-Komponenten behalten ihre jeweiligen Lizenzen.

---

## Builds und Releases

Version: **0.1.6** ([VERSION](VERSION)). [GitHub Actions](.github/workflows/release.yml) prüft Pushes und Pull Requests. Ein Tag passend zu `v` + `VERSION` veröffentlicht das Release erst nach erfolgreichen Jobs; manuelle Läufe liefern Build-Artefakte. [Downloads](https://github.com/babeinlovexd/Insane-Sound-System-2.1/releases).

ESPHome-CI verwendet Platzhalter-Zugangsdaten und flasht keine Geräte. Vor dem Einsatz eigene Secrets und Pinbelegung prüfen.

## Mitmachen

Fragen, Fehlerberichte und Beiträge sind willkommen. Nenne Version, Aufbau und Schritte zum Nachstellen im [Issue](https://github.com/babeinlovexd/Insane-Sound-System-2.1/issues). Größere Änderungen besprechen wir am besten vor einem Pull Request.

Dieses Hobbyprojekt unterstützen: [PayPal](https://www.paypal.me/babeinlovexd).

[BxD / BabeinlovexD](https://github.com/babeinlovexd) · [Website](https://babeinlovexd.de/)
