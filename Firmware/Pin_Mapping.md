# Insane Sound System V1.0.0 (2.1) - Pin Mapping

This document outlines the complete and final pin mapping for all 3 microcontrollers in the V1.0.0 2.1 system.
The mapping guarantees logical separation of buses, handles the advanced 2.1 audio routing, and integrates all components including I2C telemetry, dual-core DSP, and custom LED protocols.

---

## 1. ESP32-S3 (Master Controller & HA Bridge)

**Roles:** Media Player (HA), Display Driver, UI Polling, Encoder Reading, Flashing Proxy, Fan Curve Logic, Master EQ/Crossover Control.

| Component / Function | S3 Pin | Notes |
| :--- | :--- | :--- |
| **I2S (HA Audio out)** | | -> to RP2354A |
| - BCLK | GPIO 4 | |
| - LRCK (WS) | GPIO 5 | |
| - DOUT | GPIO 6 | |
| **SPI (ST7789 Display)** | | -> to ST7789 |
| - MOSI | GPIO 11 | |
| - SCLK | GPIO 12 | |
| - CS | GPIO 10 | |
| - DC | GPIO 9 | |
| - RES | GPIO 40 | Changed from 46 (Strapping Pin) |
| **I2C Bus** | | -> to PCF8574T (UI) & LM75AD (Temp) |
| - SDA | GPIO 8 | LM75AD Sensors: 0x48 (Env), 0x49 (Amp), 0x4A (PSU) |
| - SCL | GPIO 18 | PCF8574T: 0x20 (Buttons/LEDs) |
| **Rotary Encoder** | | Direct inputs |
| - CLK | GPIO 15 | |
| - DT | GPIO 16 | |
| - SW (Button) | GPIO 17 | |
| **Native USB (Console/Log)** | | USB CDC (No UART needed) |
| - D- / D+ | GPIO 19 / 20 | Internal USB PHY |
| **UART 0 (Proxy Flash WROOM)** | | -> to ESP32-WROOM-32D |
| - TX (to 32D RX0) | GPIO 42 | Bootloader UART |
| - RX (from 32D TX0)| GPIO 41 | Bootloader UART |
| - EN (32D Reset) | GPIO 39 | |
| - IO0 (32D Boot) | GPIO 38 | |
| **UART 1 (Metadata WROOM)**| | <-> with ESP32-WROOM-32D |
| - TX (to 32D RX2) | GPIO 44 | Sends Play/Pause/Vol Commands |
| - RX (from 32D TX2)| GPIO 43 | Receives Metadata (Title/Artist) |
| **UART 2 (Proxy Flash & DSP Control)**| | <-> with RP2354A |
| - TX (to RP RX) | GPIO 2 | Sends Vol, Crossover, EQ, Fan, LED |
| - RX (from RP TX) | GPIO 1 | Receives Temp, Faults, Status |
| - RP Reset (`RUN`) | GPIO 47 | Pulls DSP to Reset |
| - RP Boot (`QSPI_CS`) | GPIO 48 | Forces DSP into BOOTSEL mode |

---

## 2. ESP32-WROOM-32D (Bluetooth Receiver)

**Roles:** A2DP Bluetooth Sink, Metadata Extraction.

| Component / Function | 32D Pin | Notes |
| :--- | :--- | :--- |
| **I2S (BT Audio out)** | | -> to RP2354A |
| - BCLK | GPIO 26 | Master Mode |
| - LRCK (WS) | GPIO 25 | Master Mode |
| - DOUT | GPIO 22 | |
| **Status Signal** | | -> to RP2354A |
| - BT Active | GPIO 18 | HIGH when BT Audio is actively streaming |
| **UART 2 (Metadata & Control)**| | <-> with ESP32-S3 |
| - TX2 (to S3 RX) | GPIO 17 | Sends Metadata (Title/Artist) |
| - RX2 (from S3 TX)| GPIO 16 | Receives Play/Pause/Vol Commands |
| **UART 0 (Flashing)** | | <- from ESP32-S3 |
| - RX0 / TX0 | GPIO 3 / 1 | Standard esptool serial |
| - EN / IO0 | EN / IO0 | Controlled by S3 |

---

## 3. RP2354A (Core 1 DSP, Audio Routing & Power Logic)

**Roles:** I2S Slave Input mixing, "Last Active Wins" routing, Core 1 Biquad DSP (5-band EQ & Subwoofer Low-Pass), Dual-Master PIO Output, Amp config (I2C), PWM (Fan, Display), WS2805 LED Data.

| Component / Function | RP2354 Pin | Notes |
| :--- | :--- | :--- |
| **I2S Input 1 (HA)** | | <- from ESP32-S3 |
| - BCLK | GP0 | PIO Slave Mode |
| - LRCK | GP1 | PIO Slave Mode |
| - DIN | GP2 | |
| **I2S Input 2 (BT)** | | <- from ESP32-WROOM-32D |
| - BCLK | GP3 | PIO Slave Mode |
| - LRCK | GP4 | PIO Slave Mode |
| - DIN | GP5 | |
| **I2S Output (Dual Master PIO)** | | -> to MA12070P (Amp 1 & Amp 2) |
| - BCLK (Amp 1 & 2) | GP6 | Shared PIO Clock Generator |
| - LRCK (Amp 1 & 2) | GP7 | Shared PIO Clock Generator |
| - DOUT 1 (Amp 1) | GP8 | Output for Front L/R (Stereo 5-Band EQ) |
| - DOUT 2 (Amp 2) | GP9 | Output for Subwoofer (Mono summed + LPF) |
| **Clock Requirements** | | |
| - XIN / XOUT | Pin 20 / 21 | 12MHz Crystal (Physical QFN80 pins, NOT GPIOs) |
| **I2C Bus (Amp Config)** | | -> to MA12070P |
| - SDA | GP12 | Amp 1 (0x20, Stereo), Amp 2 (0x21, PBTL) |
| - SCL | GP13 | |
| **S/PDIF Input (TOSLINK)** | | <- from DLR1160 |
| - Data In | GP28 | Decoded via custom PIO State Machine |
| **Amp Status (Fault)** | | <- from MA12070P |
| - Fault 1 | GP14 | |
| - Fault 2 | GP15 | |
| **Amp Enable** | | -> to MA12070P |
| - Enable 2 | GP17 | Subwoofer Enable |
| - Enable 1 | GP20 | Front L/R Enable |
| **Status Signal** | | <- from ESP32-WROOM-32D |
| - BT Active | GP26 | |
| **PWM Outputs** | | |
| - Fan Control | GP18 | 100Hz PWM (30% min power, "0 means 0") |
| - Display Backlight| GP19 | 8-Bit PWM (DIM:x) |
| **LED Control** | | -> to WS2805 |
| - 1-Wire Data | GP16 | 40-bit MSB-first protocol with >280us Reset |
| **UART (Flashing & Telemetry)**| | <-> with ESP32-S3 |
| - RX (from S3 TX) | GP21 | |
| - TX (to S3 RX) | GP24 | |

---

## 4. UI Extender (PCF8574T via I2C on ESP32-S3)

**Roles:** Front panel buttons and status LEDs.

| Component | PCF Pin | Notes |
| :--- | :--- | :--- |
| Button Prev | P0 | INPUT_PULLUP (Inverted) |
| Button Play/Pause | P1 | INPUT_PULLUP (Inverted) |
| Button Next | P2 | INPUT_PULLUP (Inverted) |
| Red LED | P3 | OUTPUT (Inverted) - Faults & Warnings |
| White LED | P4 | OUTPUT (Inverted) - HA Audio Source Active |
| Blue LED | P5 | OUTPUT (Inverted) - BT Audio Source Active |
| Green LED | P6 | OUTPUT (Inverted) - System Ready / WLAN OK |
| Yellow LED | P7 | OUTPUT (Inverted) - TOSLINK Source Active |
