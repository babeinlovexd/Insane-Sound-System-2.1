<div align="center">
  
# 🔊 Insane Sound System 2.1
### The Ultimate Triple-Brain Open-Source DSP Audio Matrix

<img src="https://img.shields.io/github/v/release/babeinlovexd/Insane-Sound-System-2.1?style=for-the-badge&color=2ecc71" alt="Latest Release">
<img src="https://img.shields.io/badge/Hardware-V0.0.0-f39c12?style=for-the-badge&logo=pcb" alt="Hardware Version">
<img src="https://img.shields.io/badge/Status-BETA-ff9800?style=for-the-badge&logo=test-tube" alt="Status: Beta">
<img src="https://img.shields.io/github/actions/workflow/status/babeinlovexd/Insane-Sound-System-2.1/release-firmware.yml?style=for-the-badge&logo=githubactions&logoColor=white" alt="CI/CD Build Status">
<br><br>

<img src="https://img.shields.io/badge/ESPHome-Ready-03A9F4?style=for-the-badge&logo=esphome" alt="ESPHome">
<img src="https://img.shields.io/badge/Home_Assistant-Native-41BDF5?style=for-the-badge&logo=home-assistant" alt="Home Assistant">
<img src="https://img.shields.io/badge/PlatformIO-RP2354%20%26%20WROOM--32D-F6822A?style=for-the-badge&logo=platformio&logoColor=white" alt="PlatformIO">
<br><br>

<img src="https://img.shields.io/badge/Main_Brain-ESP32--S3-E7352C?style=for-the-badge&logo=espressif" alt="Main: ESP32-S3">
<img src="https://img.shields.io/badge/BT_Radio-WROOM--32D-0052CC?style=for-the-badge&logo=bluetooth" alt="BT: WROOM-32D">
<img src="https://img.shields.io/badge/DSP-RP2354A-8A2BE2?style=for-the-badge&logo=raspberrypi" alt="DSP: RP2354A">
<img src="https://img.shields.io/badge/Amps-Dual_MA12070P-e74c3c?style=for-the-badge&logo=stmicroelectronics" alt="Amps: MA12070P">
<br><br>

<img src="https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey?style=for-the-badge&logo=creative-commons" alt="License: CC BY-NC-SA 4.0">
<br><br>

> **⚠️ WICHTIGER HINWEIS:** Das Projekt befindet sich aktuell im **BETA-Status**. Die Hardware-Architektur steht, aber die Firmware wird aktiv weiterentwickelt. Expect some noise, expect some bugs – but mostly, expect insane audio!

---

## 🧠 Die Architektur: The Triple-Brain Matrix

Um Management, Konnektivität und High-End-Audioverarbeitung kompromisslos voneinander zu trennen, nutzt das Insane Sound System 2.1 eine dedizierte Drei-Prozessor-Architektur:

### 1. ESP32-S3 (The Director)
**Das absolute Kontrollzentrum.** Er kommuniziert mit der Außenwelt und dirigiert als souveräner Master die anderen Chips. 
Als **Home Assistant Boss** stellt er via ESPHome alle Regler (EQ, Crossover, Volume) bereit und übersetzt sie in Echtzeit-UART-Befehle. Er übernimmt das **thermisches Management**, sammelt sämtliche Systemtemperaturen und berechnet daraus die perfekte PWM-Lüfterkurve. Alle Systeminfos pusht er gestochen scharf auf das **Widescreen LCD Display**, während er gleichzeitig die haptischen Eingaben am **Frontpanel** (über PCF8574T und Encoder) auswertet. Firmware-Updates verteilt er als intelligenter **Proxy-Flasher** Over-The-Air an seine Co-Prozessoren, bietet aber für den allerersten Setup-Prozess eine komfortable **native USB-Schnittstelle**.

### 2. ESP32-WROOM-32D (The Radio)
**Der Spezialist für drahtloses Audio.** Er ist mit vollem **Fokus** exklusiv für den Bluetooth A2DP Sink zuständig. Weil er sich durch nichts anderes ablenken lässt, bricht der Stream niemals ab – selbst wenn der S3 gerade schwer beschäftigt ist. Durch das **direkte Routing** reicht er den I2S-Audiostream und die Play/Pause-Zustände ohne Umwege und Verzögerungen direkt an die DSP-Matrix weiter.

### 3. RP2354A (The DSP & Audio-Matrix)
**Das audiophile Herzstück.** Dieser Chip nutzt seinen dedizierten zweiten Kern (Core 1) und die integrierte Hardware-FPU ausschließlich für die Signalverarbeitung. Als **Hardware-DSP** berechnet er völlig latenzfrei einen Stereo-5-Band-EQ für die Front-Lautsprecher sowie einen dynamischen Low-Pass-Filter für den Subwoofer. Seine **I2C Amp-Control** konfiguriert die Dual-MA12070P Endstufen beim Booten exakt für das 2.1 Setup und fesselt die Hardware-Lautstärke an das vorgegebene Limit. Er übernimmt das **Zero-Latency Switching** zwischen WLAN, Bluetooth und S/PDIF (TV) per DMA/PIO und steuert als visuelles Highlight das exakte 40-Bit-Timing der **WS2805 RGBWW-LEDs**.

---

[![Pin Mapping](https://img.shields.io/badge/Hardware-Pin_Mapping_Reference-f39c12?style=for-the-badge&logo=microchip)](Firmware/Pin_Mapping.md)

---

## ⚖️ Lizenz
Dieses komplette Projekt (Hardware und Software) steht unter der [CC BY-NC-SA 4.0 Lizenz](https://creativecommons.org/licenses/by-nc-sa/4.0/). 
Das bedeutet: Nachbauen und Anpassen für private Zwecke ist ausdrücklich erwünscht, jede kommerzielle Nutzung oder der Verkauf sind strikt verboten!

---

## ☕ Support dieses Projekts
2.1 hat extrem viel Zeit, Nerven und Kaffee gekostet. Wenn dir das System gefällt und du meine Arbeit unterstützen möchtest, freue ich mich riesig über einen virtuellen Kaffee!

<a href="https://www.paypal.me/babeinlovexd">
  <img src="https://img.shields.io/badge/Donate-PayPal-blue.svg?style=for-the-badge&logo=paypal" alt="Donate mit PayPal">
</a>

Jeder Cent fließt direkt in die Entwicklung von 2.1 und neue Prototypen! 🚀
---

## 👨‍💻 Entwickelt von

| [<img src="https://avatars.githubusercontent.com/u/43302033?v=4" width="100"><br><sub>**Christopher**</sub>](https://github.com/babeinlovexd) |
| :---: |

---

</div>



