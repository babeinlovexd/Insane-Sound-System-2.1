# Contributing to Insane Sound System 2.1

First off, thank you for considering contributing to the **Insane Sound System**! It's people like you that make this "Triple-Brain" project even better.

## How Can I Contribute?

### Reporting Bugs
- Use the **Bug Report** template when opening an issue.
- Describe the hardware setup (e.g., specific PCB version) and the firmware version you are using.
- Include logs from the S3 (USB Serial) or the RP2354 if applicable.

### Suggesting Enhancements
- We love new DSP features (EQ presets, filter types) or UI improvements for the Widescreen LCD.
- Open an issue first to discuss your idea before starting the implementation.

### Pull Requests (The "Triple-Brain" Workflow)
Our system consists of three distinct firmwares. Please specify which part your PR targets:
1. **Director (ESP32-S3):** ESPHome-based logic and UI.
2. **Radio (ESP32-WROOM-32D):** Bluetooth A2DP Sink.
3. **DSP (RP2354A):** Core audio math, PIO, and hardware control.

**PR Guidelines:**
- Follow the existing C++/Arduino or YAML coding styles.
- Double-check for "Anti-Pop" logic when touching power sequences.
- Ensure that any changes to the UART protocol are documented in the `Pin_Mapping.md`.
- **Test your code!** Since this is hardware-related, ensure your changes don't "blow up" any amplifiers.

## Development Environment
- **Firmware (WROOM/RP):** We use **PlatformIO**.
- **Director (S3):** We use **ESPHome** (command line or dashboard).
- **GUI tools:** Python 3.11+ for the InsaneFlasher logic.

## Code of Conduct
By participating in this project, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md).
