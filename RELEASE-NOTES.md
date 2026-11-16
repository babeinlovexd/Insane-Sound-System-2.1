# Insane-Sound-System-2.1 v0.1.6

Flasher bezieht Updates jetzt aus diesem Repository und wählt wroom.bin bzw. rp2354.bin eindeutig aus. Die Anleitung beschreibt das auf echter Hardware erprobte Testprojekt.

Die Release-Version bezeichnet den Repository-Stand, nicht eine neue PCB-Revision. Quellcode-Pakete enthalten keine `secrets.yaml`. Persönliche Einstellungen müssen vor einem Firmware-Build angepasst werden.

Die Audio-YAML verwendet die aktuelle `audio.codecs`-Konfiguration für ESPHome 2026.10.2; FLAC-Transcoding bleibt eingestellt. Bluetooth-Toolchains mit älterer I2S-API werden passend festgelegt.
