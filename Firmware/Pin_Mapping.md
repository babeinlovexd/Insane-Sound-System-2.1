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

### 2. ESP32-WROOM-32D (Bluetooth Receiver)

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
| - Fan Control | GP18 | -> to Fan circuit |
| - Display Backlight| GP19 | -> to ST7789 BL pin |
| **LED Control** | | |
| - 1-Wire Data | GP16 | -> SN74AHCT125D -> WS2805 |
| **UART (Flashing)** | | <- from ESP32-S3 |
| - RX / TX | GP21 / GP24 | |

## PART 2: PERIPHERAL COMPONENTS (Bottom-Up View)

### 4.1. PCF8574T (I2C I/O Expander)
**Function:** Collects front panel button inputs and controls UI LEDs. **I2C Address:** `0x20`

| PCF Pin | Function | Target / Note |
| :--- | :--- | :--- |
| **SDA / SCL** | I2C Bus | <-> ESP32-S3 (GPIO 8 / GPIO 18) |
| **P0** | Input | Button "Prev" |
| **P1** | Input | Button "Play/Pause" |
| **P2** | Input | Button "Next" |
| **P3** | Output | UI LED: Red (Warning) |
| **P4** | Output | UI LED: White (HA active) |
| **P5** | Output | UI LED: Blue (BT active) |
| **P6** | Output | UI LED: Green (System Ready) |
| **P7** | Output | UI LED: Yellow (TV active) |

### 4.2. LM75AD x3 (I2C Digital Temperature Sensors)
**Funktion:** Kontinuierliche Temperaturüberwachung für das thermische Management des S3.
**Bus:** Gesteuert über ESP32-S3 (SDA: GPIO 8, SCL: GPIO 18).

#### Pinout des LM75AD (SOIC-8 / TSSOP-8)
| Pin | Name | Funktion | Ziel / Anmerkung |
| :--- | :--- | :--- | :--- |
| 1 | **SDA** | I2C Data | <-> ESP32-S3 (GPIO 8) |
| 2 | **SCL** | I2C Clock | <- ESP32-S3 (GPIO 18) |
| 3 | **OS** | Over-temp Output | **NC** (Offen lassen oder Pull-Up - wird via Software gelöst) |
| 4 | **GND** | Ground | Gemeinsame System-Masse |
| 5 | **A2** | Address Bit 2 | Hardwired (MSB der Adresse) |
| 6 | **A1** | Address Bit 1 | Hardwired |
| 7 | **A0** | Address Bit 0 | Hardwired (LSB der Adresse) |
| 8 | **VCC** | Power | **+3.3V** (Vom S3 Spannungsregler) |

#### Adress-Konfiguration (Hardware-Strapping)
Die Basis-Adresse des LM75AD ist binär `1001` (0x48). Die Pins A2, A1 und A0 füllen die letzten 3 Bits auf.

| Sensor-Instanz | A2 (Pin 5) | A1 (Pin 6) | A0 (Pin 7) | I2C Adresse |
| :--- | :--- | :--- | :--- | :--- |
| **LM75AD #1 (Env)** | **GND** (0) | **GND** (0) | **GND** (0) | `0x48` |
| **LM75AD #2 (Amp)** | **GND** (0) | **GND** (0) | **VCC** (1) | `0x49` |
| **LM75AD #3 (PSU)** | **GND** (0) | **VCC** (1) | **GND** (0) | `0x4A` |

### 4.3. ST7789 (Widescreen LCD Display)
**Function:** Visual interface. Controlled by S3, backlight by RP2354.

| Display Pin | Function | Target / Note |
| :--- | :--- | :--- |
| **SDA (MOSI)** | SPI Data | <- ESP32-S3 (GPIO 11) |
| **SCL (SCLK)** | SPI Clock | <- ESP32-S3 (GPIO 12) |
| **CS** | Chip Select | <- ESP32-S3 (GPIO 10) |
| **DC** | Data/Command | <- ESP32-S3 (GPIO 9) |
| **RES** | Reset | <- ESP32-S3 (GPIO 40) |
| **BLK** | Backlight PWM | <- RP2354 (GP19) |

### 4.4. Infineon MA12070P x2 (Class-D Amplifiers)
**Function:** The audio muscles. Amp 1 runs in BTL stereo mode (Front L/R), Amp 2 strictly in PBTL mono mode (Subwoofer).

| Amp Pin | Amp 1 (Front L/R) | Amp 2 (Subwoofer) | Target / Note |
| :--- | :--- | :--- | :--- |
| **I2C Address**| `0x20` | `0x21` | Hardware strapping of address pins |
| **I2C Bus** | SDA / SCL | SDA / SCL | <-> RP2354 (GP12 / GP13) |
| **I2S BCLK** | <- RP2354 (GP6) | <- RP2354 (GP6) | Shared clock bus |
| **I2S LRCK** | <- RP2354 (GP7) | <- RP2354 (GP7) | Shared clock bus |
| **I2S DIN** | <- RP2354 (GP8) | <- RP2354 (GP9) | Discrete audio paths from DSP |
| **/ENABLE** | <- RP2354 (GP20) | <- RP2354 (GP17) | Anti-pop control |
| **/ERROR** | -> RP2354 (GP14) | -> RP2354 (GP15) | Fault feedback to DSP |

### 4.5. Native USB-C Interface
**Function:** Flashing the S3 and serial console via USB-JTAG.

| USB Pin | Function | Target / Note |
| :--- | :--- | :--- |
| **D-** | Data Minus | <-> ESP32-S3 (GPIO 19) |
| **D+** | Data Plus | <-> ESP32-S3 (GPIO 20) |
| **VBUS** | 5V Power | -> 5V rail / Voltage regulator |

### 4.6. SN74AHCT125D (Level Shifter)
**Funktion:** Wandelt das 3,3V Logiksignal des RP2354 in ein sauberes 5V Signal für die WS2805 LEDs um. Der Baustein fungiert als Quad-Buffer, wobei nur der erste Kanal aktiv genutzt wird.

| Pin | Name | Funktion | Ziel / Anmerkung |
| :--- | :--- | :--- | :--- |
| 1 | **1OE** | Output Enable 1 | **GND** (Aktiviert Kanal 1) |
| 2 | **1A** | Input 1 | <- RP2354 (GP16) |
| 3 | **1Y** | Output 1 | -> **WS2805 Data IN** (5V Pegel) |
| 4 | **2OE** | Output Enable 2 | **GND** (Terminierung gegen Floating) |
| 5 | **2A** | Input 2 | **GND** (Terminierung gegen Floating) |
| 6 | **2Y** | Output 2 | **NC** (Offen lassen!) |
| 7 | **GND** | Ground | Gemeinsame System-Masse |
| 8 | **3Y** | Output 3 | **NC** (Offen lassen!) |
| 9 | **3A** | Input 3 | **GND** (Terminierung gegen Floating) |
| 10 | **3OE** | Output Enable 3 | **GND** (Terminierung gegen Floating) |
| 11 | **4Y** | Output 4 | **NC** (Offen lassen!) |
| 12 | **4A** | Input 4 | **GND** (Terminierung gegen Floating) |
| 13 | **4OE** | Output Enable 4 | **GND** (Terminierung gegen Floating) |
| 14 | **VCC** | Power | **+5V** Systemspannung |
