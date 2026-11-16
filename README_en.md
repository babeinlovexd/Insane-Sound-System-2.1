<p align="center">
  <a href="https://github.com/babeinlovexd"><img src="https://raw.githubusercontent.com/babeinlovexd/babeinlovexd/main/bxd-github-header.svg" width="520" alt="BxD / BabeinlovexD"></a>
</p>

# Insane Sound System 2.1

[Deutsch](README.md) · [Issues](https://github.com/babeinlovexd/Insane-Sound-System-2.1/issues) · [Releases](https://github.com/babeinlovexd/Insane-Sound-System-2.1/releases)

A **2.1 audio test project verified on real hardware by its author**. It separates ESPHome/network control, Bluetooth reception and DSP across an ESP32-S3, ESP32-WROOM-32D and RP2354A. It is an experimental hardware platform, distinct from Sound System V5.

## Architecture

| Controller | Responsibility |
| :--- | :--- |
| ESP32-S3 | Home Assistant/ESPHome, network audio, display, encoder, temperatures and control |
| ESP32-WROOM-32D | Bluetooth A2DP input and metadata |
| RP2354A | Audio routing, DSP, amplifier/LED/fan control |

The design includes a five-band front-channel EQ, subwoofer low-pass, two MA12070P amplifiers and S/PDIF input. Actual latency and supported behavior depend on firmware and hardware; this is not a zero-latency or universally tested compatibility claim.

## Hardware & files

The detailed [pin mapping](Firmware/Pin_Mapping.md) describes controller interconnections. Compare it with the actual main YAML and firmware sources for the revision being built.

- [ESPHome/insane-sound-system-2.1.yaml](ESPHome/insane-sound-system-2.1.yaml): S3 configuration.
- [Firmware/WROOM32D](Firmware/WROOM32D): PlatformIO environment `esp32dev`.
- [Firmware/RP2354A](Firmware/RP2354A): PlatformIO environment `rp2354`.
- [InsaneFlasher](InsaneFlasher): desktop utility and proxy flashing scripts.

## Installation

1. Check the assembled hardware and its pin mapping before flashing.
2. Copy the main YAML into ESPHome and create a local `secrets.yaml` with `api_encryption_key`, `ota_password`, `wifi_ssid`, `wifi_password` and `ap_password`.
3. Adjust `ha_player_id`, validate/compile the configuration and initially flash the S3 via USB.
4. Build or obtain the matching `wroom.bin` and `rp2354.bin` from this repository's releases. Flash each target using the appropriate interface and bootloader for the assembled board.
5. Add the S3 to Home Assistant and test audio routing, volume, cooling and fault inputs individually.

## Building firmware

With PlatformIO installed, run from the repository root:

```bash
pio run -d Firmware/WROOM32D -e esp32dev
pio run -d Firmware/RP2354A -e rp2354
```

The S3 YAML is built with ESPHome separately. Outputs for the PlatformIO targets are under their respective `.pio/build/<environment>/` directories. Release binaries are target-specific, not interchangeable.

## InsaneFlasher & proxy prerequisites

The Python desktop source is [InsaneFlasher/Linux_Mac/InsaneFlasher.py](InsaneFlasher/Linux_Mac/InsaneFlasher.py). Its dependencies are `customtkinter requests esptool zeroconf pillow pyserial`. Run it from its own directory to keep its graphics available.

The utility checks this **2.1 repository's releases**. It selects `wroom.bin` or `rp2354.bin` by exact filename. Existing downloaded EXEs are not changed by a source-code update; use an executable built from the corresponding release.

WROOM proxy traffic uses TCP port **6666**; the RP2354 path uses **6667**. **RP2354 serial proxy flashing requires a compatible custom serial bootloader.** The native ROM does not implement this project's raw TCP-to-UART payload protocol. A successful network transfer alone is not proof of successful DSP flashing; use the native supported flashing method when that bootloader is absent.

## Troubleshooting

- **Wrong firmware/update:** Confirm the 2.1 repository, target filename and current tool build; V5 binaries are not substitutes.
- **DSP does not restart after proxy transfer:** Check the installed bootloader and use a matching local flashing method.
- **Source switching or controls fail:** Check the separate I2S, metadata UART and DSP-control connections.
- **Temperature/fault reports:** Compare YAML sensor type, address and physical components before changing protection settings.

## License

[CC BY-NC-SA 4.0](LICENSE). The existing project license applies; third-party components retain their own licenses.

---

## Builds and releases

Version: **0.1.6** ([VERSION](VERSION)). [GitHub Actions](.github/workflows/release.yml) checks each push and pull request. A tag matching `v` + `VERSION` publishes a release after all jobs succeed; manual runs provide build artifacts. [Downloads](https://github.com/babeinlovexd/Insane-Sound-System-2.1/releases).

ESPHome CI uses dummy credentials and does not flash devices. Configure your own secrets and pin assignments before deployment.

## Contributing

Questions, bug reports and contributions are welcome. Include version, setup and reproduction steps in an [Issue](https://github.com/babeinlovexd/Insane-Sound-System-2.1/issues). Please discuss larger changes before a pull request.

Support this hobby project: [PayPal](https://www.paypal.me/babeinlovexd).

[BxD / BabeinlovexD](https://github.com/babeinlovexd) · [Website](https://babeinlovexd.de/)
