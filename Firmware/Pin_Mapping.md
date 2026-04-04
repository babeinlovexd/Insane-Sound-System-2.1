# Insane Sound System 2.1 - Pin Mapping

This document outlines the pin mapping for all 3 microcontrollers in the 2.1 system.
The mapping is derived from the module diagrams, the system architecture flow, and ensures logical separation of buses to avoid clashes.

## 1. ESP32-S3 (Main Controller & HA Bridge)

**Roles:** Media Player (HA), Display Driver, UI Polling, Encoder Reading, Proxy Flasher.

| Component / Function | S3 Pin | Notes |
| :--- | :--- | :--- |
| **I2S (HA Audio out)** | | -> to RP2354 |
| - BCLK | GPIO 4 | |
| - LRCK (WS) | GPIO 5 | |
| - DOUT | GPIO 6 | |
| **SPI (ST7789 Display)** | | |
| - MOSI | GPIO 11 | |
| - SCLK | GPIO 12 | |
| - CS | GPIO 10 | |
| - DC | GPIO 9 | |
| - RES | GPIO 40 | Changed from 46 (Strapping Pin) |
| **I2C Bus** | | -> to PCF8574T (UI) & LM75AD (Temp) |
| - SDA | GPIO 8 | |
| - SCL | GPIO 18 | |
| **Rotary Encoder** | | Direct inputs |
| - CLK | GPIO 15 | |
| - DT | GPIO 16 | |
| - SW (Button) | GPIO 17 | |
| **Native USB (Console/Log)** | | USB CDC (No UART needed) |
| - D- / D+ | GPIO 19, 20 | Internal USB PHY |
| **UART 0 (Proxy Flash WROOM)** | | -> to ESP32-32D (Bootloader UART) |
| - TX (to 32D RX0) | GPIO 42 | |
| - RX (from 32D TX0)| GPIO 41 | |
| - EN (32D Reset) | GPIO 39 | |
| - IO0 (32D Boot) | GPIO 38 | |
| **UART 1 (Metadata & Control)**| | <-> with ESP32-32D (Secondary UART) |
| - TX (to 32D RX2) | GPIO 44 | Sends Play/Pause/Override Commands |
| - RX (from 32D TX2)| GPIO 43 | Receives Metadata (Title/Artist) |
| **UART 2 (Proxy Flash & DSP Control)**| | -> to RP2354 |
| - TX (to RP RX) | GPIO 2 | |
| - RX (from RP TX) | GPIO 1 | |
| - RP Reset (`RUN`) | GPIO 47 | Pulls DSP to Reset |
| - RP Boot (`QSPI_CS`) | GPIO 48 | Forces DSP into BOOTSEL mode |

---

## 2. ESP32-WROOM-32D (Bluetooth Receiver)

**Roles:** A2DP Bluetooth Sink, Metadata extraction.

| Component / Function | 32D Pin | Notes |
| :--- | :--- | :--- |
| **I2S (BT Audio out)** | | -> to RP2354 |
| - BCLK | GPIO 26 | |
| - LRCK (WS) | GPIO 25 | |
| - DOUT | GPIO 22 | |
| **Status Signal** | | -> to RP2354 |
| - BT Active | GPIO 18 | High when BT Audio is playing |
| **UART 2 (Metadata & Control)**| | <-> with ESP32-S3 |
| - TX2 (to S3 RX) | GPIO 17 | Sends Metadata (Title/Artist) |
| - RX2 (from S3 TX)| GPIO 16 | Receives Play/Pause/Override Commands |
| **UART 0 (Flashing)** | | <- from ESP32-S3 |
| - RX0 / TX0 | GPIO 3 / 1 | Standard esptool serial |
| - EN / IO0 | EN / IO0 | Controlled by S3 |

---

## 3. RP2354A (DSP, Routing & Power Logic)

**Roles:** I2S Input mixing/switching, I2S Output to Amps, Amp config (I2C), Fan PWM, Display Backlight PWM, LED 1-Wire Data.

| Component / Function | RP2354 Pin | Notes |
| :--- | :--- | :--- |
| **I2S Input 1 (HA)** | | <- from ESP32-S3 |
| - BCLK | GP0 | |
| - LRCK | GP1 | |
| - DIN | GP2 | |
| **I2S Input 2 (BT)** | | <- from ESP32-32D |
| - BCLK | GP3 | |
| - LRCK | GP4 | |
| - DIN | GP5 | |
| **I2S Output (Dual Master PIO)** | | -> to MA12070P (Amp 1 & 2) |
| - BCLK (Amp 1 & 2) | GP6 | Clocks are shared from a single PIO generator |
| - LRCK (Amp 1 & 2) | GP7 | |
| - DOUT 1 (Amp 1) | GP8 | Data outputs must be physically contiguous |
| - DOUT 2 (Amp 2) | GP9 | for the PIO `out pins, 2` instruction |
| **Clock Requirements** | | |
| - XIN / XOUT | Pin 20 / Pin 21 | Must use 12MHz Crystal with 12-22pF caps to GND (Physical QFN80 pins, NOT GPIOs) |
| **I2C Bus** | | -> Config for MA12070P (Amp 1 & 2) |
| - SDA | GP12 | |
| - SCL | GP13 | |
| **S/PDIF Input (TOSLINK)** | | <- from DLR1160 (or similar) optical receiver |
| - Data In | GP28 | Decoded via custom PIO State Machine (Zero-Click Routing Prio 2) |
| **Amp Status (Fault)** | | <- from MA12070P (Amp 1 & 2) |
| - Fault 1 | GP14 | |
| - Fault 2 | GP15 | |
| **Amp Mute / Enable**| | -> to MA12070P (Amp 1 & 2) |
| - Enable 1 | GP16 | |
| - Enable 2 | GP17 | |
| **Status Signal** | | <- from ESP32-32D |
| - BT Active | GP26 | |
| **PWM Outputs** | | |
| - Fan Control | GP18 | -> to Fan circuit |
| - Display Backlight| GP19 | -> to ST7789 BL pin |
| **LED Control** | | |
| - 1-Wire Data | GP20 | -> SN74AHCT125D -> WS2805 |
| **UART (Flashing)** | | <- from ESP32-S3 |
| - RX / TX | GP21 / GP24 | |
