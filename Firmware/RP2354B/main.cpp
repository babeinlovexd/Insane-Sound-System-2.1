#include <Arduino.h>
#include <Wire.h>
#include <I2S.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "i2s_slave.h"
#include "i2s_dual_master.h"
#include "ws2805_pio.h"

// Note: Standard Arduino-Pico I2S Objects removed. 
// We exclusively use custom PIO state machines for routing.

// ---------------------------------------------------------
// Pin Definitions based on Pin_Mapping.md
// ---------------------------------------------------------

// I2S Input 1 (from S3 / HA)
#define I2S_IN1_BCLK 0
#define I2S_IN1_LRCK 1
#define I2S_IN1_DIN  2

// I2S Input 2 (from 32D / BT)
#define I2S_IN2_BCLK 3
#define I2S_IN2_LRCK 4
#define I2S_IN2_DIN  5

// I2S Output (Dual Master PIO for both MA12070P Amps)
// The PIO `out pins, 2` instruction requires contiguous pins.
#define I2S_OUT_BCLK  6
#define I2S_OUT_LRCK  7
#define I2S_OUT1_DOUT 8
#define I2S_OUT2_DOUT 9

// I2C (Amp Config)
#define I2C_SDA 12
#define I2C_SCL 13

// Amp Control/Status
#define AMP1_FAULT 14
#define AMP2_FAULT 15
#define AMP1_EN    16
#define AMP2_EN    17

// PWM Outputs
#define FAN_PWM    18
#define BL_PWM     19

// WS2805 LED Data (1-Wire)
#define LED_DATA   20

// Inputs & Status
#define BT_ACTIVE_IN 26
#define TOSLINK_IN   28

SPDIF_Decoder* tvDecoder;
WS2805* leds;

// ---------------------------------------------------------
// Zero-Click Routing Logic
// ---------------------------------------------------------
enum AudioSource {
  SRC_UNKNOWN,
  SRC_WLAN,    // Prio 4
  SRC_BT,      // Prio 3
  SRC_TV,      // Prio 2
  SRC_OVERRIDE // Prio 1
};

AudioSource currentSource = SRC_UNKNOWN;

// Global references to PIO State Machines for DMA routing
uint sm_in_wlan;
uint sm_in_bt;
// uint sm_in_tv; // Uncomment when S/PDIF PIO assembly is integrated

void setAudioSource(AudioSource newSource) {
  if (currentSource == newSource) return;
  currentSource = newSource;
  
  String sourceName = "";
  switch (newSource) {
    case SRC_WLAN:    
      sourceName = "WLAN"; 
      // Route I2S_IN1 (S3) to DMA
      switch_audio_dma_source(&pio0->rxf[sm_in_wlan], pio_get_dreq(pio0, sm_in_wlan, false));
      break;
    case SRC_BT:      
      sourceName = "Bluetooth"; 
      // Route I2S_IN2 (WROOM) to DMA
      switch_audio_dma_source(&pio0->rxf[sm_in_bt], pio_get_dreq(pio0, sm_in_bt, false));
      break;
    case SRC_TV:      
      sourceName = "TOSLINK (TV)"; 
      if (tvDecoder) {
          switch_audio_dma_source((volatile void*)tvDecoder->get_rx_fifo_addr(), tvDecoder->get_dreq());
      }
      break;
    case SRC_OVERRIDE:
      sourceName = "HA Override (TTS)"; 
      // Route I2S_IN1 (S3) to DMA for Override
      switch_audio_dma_source(&pio0->rxf[sm_in_wlan], pio_get_dreq(pio0, sm_in_wlan, false));
      break;
    default: return;
  }
  
  // Send status update to ESP32-S3 over Hardware UART (GP22 TX / GP21 RX)
  Serial1.printf("SOURCE_CHANGED:%s\n", sourceName.c_str());
  
  // Also log to USB console
  Serial.printf("SOURCE_CHANGED:%s\n", sourceName.c_str());
}

// DMA Channels
int dma_in_chan;
int dma_out_chan;
uint32_t audio_buffer[256] __attribute__((aligned(1024))); // 256 * 4 = 1024 bytes

void setup_audio_dma() {
  // Claim DMA channels
  dma_in_chan = dma_claim_unused_channel(true);
  dma_out_chan = dma_claim_unused_channel(true);
  
  // Setup Input DMA (From active PIO RX FIFO to RAM buffer)
  dma_channel_config in_config = dma_channel_get_default_config(dma_in_chan);
  channel_config_set_transfer_data_size(&in_config, DMA_SIZE_32);
  channel_config_set_read_increment(&in_config, false);
  channel_config_set_write_increment(&in_config, true);
  channel_config_set_ring(&in_config, true, 10); // 1024 bytes ring buffer
  
  // Setup Output DMA (From RAM buffer to Dual Master PIO TX FIFO)
  dma_channel_config out_config = dma_channel_get_default_config(dma_out_chan);
  channel_config_set_transfer_data_size(&out_config, DMA_SIZE_32);
  channel_config_set_read_increment(&out_config, true);
  channel_config_set_write_increment(&out_config, false);
  channel_config_set_ring(&out_config, false, 10); // 1024 bytes ring buffer
  
  // In order to not stop after the first 1024 words, set transfer count to a huge number,
  // or set up a control block to loop forever. The simplest fix for free-running I2S
  // without interrupts is setting a practically infinite transfer count (e.g. 0xFFFFFFFF).
  dma_channel_configure(dma_out_chan, &out_config, 
      NULL, // Destination (Set later in main)
      audio_buffer, 
      0xFFFFFFFF, false);
      
  dma_channel_configure(dma_in_chan, &in_config, 
      audio_buffer, 
      NULL, // Source (Set later dynamically)
      0xFFFFFFFF, false);
}

void switch_audio_dma_source(volatile void* pio_sm_rx_fifo_addr, uint dreq) {
  // Stop current input DMA
  dma_channel_abort(dma_in_chan);
  
  // Reconfigure pacing to match the new PIO SM
  dma_channel_config in_config = dma_channel_get_default_config(dma_in_chan);
  channel_config_set_transfer_data_size(&in_config, DMA_SIZE_32);
  channel_config_set_read_increment(&in_config, false);
  channel_config_set_write_increment(&in_config, true);
  channel_config_set_ring(&in_config, true, 10);
  channel_config_set_dreq(&in_config, dreq); // Set correct DREQ for pacing

  dma_channel_set_config(dma_in_chan, &in_config, false);
  
  // Restart input DMA to point to the new PIO SM FIFO
  dma_channel_set_read_addr(dma_in_chan, pio_sm_rx_fifo_addr, true);
}

void setup() {
  Serial.begin(115200); // Native USB
  
  // Setup UART for S3 Communication (TX=GP24, RX=GP21)
  Serial1.setTX(24);
  Serial1.setRX(21);
  Serial1.begin(115200);
  
  // Setup PWM Pins
  pinMode(FAN_PWM, OUTPUT);
  pinMode(BL_PWM, OUTPUT);
  analogWrite(FAN_PWM, 0); // Off initially
  analogWrite(BL_PWM, 255); // Full brightness initially

  // Initialize WS2805 LEDs on GP20
  // PIO1 SM1 is used since PIO1 SM0 is for I2S Out, and PIO0 is full.
  leds = new WS2805(pio1, 1, LED_DATA, 10); // Assume 10 LEDs for now, but colors sent to all
  leds->show(0, 0, 0, 0, 0); // Turn off initially

  // Setup Amp Control Pins
  pinMode(AMP1_FAULT, INPUT);
  pinMode(AMP2_FAULT, INPUT);
  pinMode(AMP1_EN, OUTPUT);
  pinMode(AMP2_EN, OUTPUT);
  
  // Enable both amps
  digitalWrite(AMP1_EN, HIGH);
  digitalWrite(AMP2_EN, HIGH);

  // Setup I2C for Amp Configuration
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  // ---------------------------------------------------------
  // "Kein-Plopp" Boot-Sequenz (Anti-Pop)
  // ---------------------------------------------------------
  // 1. AMP_EN auf LOW halten (bereits durch pinMode LOW geschehen)
  digitalWrite(AMP1_EN, LOW);
  digitalWrite(AMP2_EN, LOW);

  // 2. I2C-Konfiguration an die Amps senden (Volume auf Mute/-100dB, I2S 24-Bit)
  uint8_t amp_addresses[2] = {0x20, 0x21};
  for (int i = 0; i < 2; i++) {
    uint8_t addr = amp_addresses[i];
    
    // Set Audio Format to I2S 24-Bit (Reg 0x25 = 0x00 for standard I2S 24-bit on MA12070P)
    Wire.beginTransmission(addr);
    Wire.write(0x25); 
    Wire.write(0x00); // 00000000: I2S, 24-bit
    Wire.endTransmission();

    // Set Power Mode Profile (Reg 0x2D)
    Wire.beginTransmission(addr);
    Wire.write(0x2D); 
    Wire.write(0x01); // Standard Profile
    Wire.endTransmission();

    // Assert Soft-Mute (Reg 0x2E)
    Wire.beginTransmission(addr);
    Wire.write(0x2E); 
    Wire.write(0x01); // Enable Soft Mute
    Wire.endTransmission();

    // Volume initial -100dB (Reg 0x2C)
    Wire.beginTransmission(addr);
    Wire.write(0x2C); 
    Wire.write(0x00); // Lowest volume
    Wire.endTransmission();
  }

  // Initialize I2S Routing Pins (We use basic digital tracking for the clock logic)
  pinMode(I2S_IN1_BCLK, INPUT);
  pinMode(I2S_IN2_BCLK, INPUT);

  // Setup Status monitoring pins
  pinMode(BT_ACTIVE_IN, INPUT_PULLDOWN);
  
  // Initialize PIO S/PDIF state machine for TOSLINK (Prio 2)
  tvDecoder = new SPDIF_Decoder(pio0, TOSLINK_IN);

  // Initialize Output PIO (Dual I2S Master) driving BOTH Amplifiers synchronously
  // DOUT1=GP8 and DOUT2=GP9 are physically contiguous to allow the `out pins, 2` assembly.
  // BCLK=GP6 and LRCK=GP7 are also contiguous to allow `set pins, 2` clock generation.
  uint sm_out = pio_claim_unused_sm(pio1, true);
  uint offset_out = pio_add_program(pio1, &i2s_dual_master_program);
  i2s_dual_master_program_init(pio1, sm_out, offset_out, I2S_OUT1_DOUT, I2S_OUT_BCLK);
  
  // Note: To make the PIO structurally sound for non-contiguous pins, we dynamically
  // patch the WAIT GPIO instruction with the correct absolute pin numbers.
  uint16_t wlan_prog[7] = {
      (uint16_t)(0x2080 | I2S_IN1_LRCK), // Wait LRCK High
      (uint16_t)(0x2000 | I2S_IN1_LRCK), // Wait LRCK Low
      0xe03f,                            // set x, 63
      (uint16_t)(0x2000 | I2S_IN1_BCLK), // Wait BCLK Low
      (uint16_t)(0x2080 | I2S_IN1_BCLK), // Wait BCLK High
      0x4001,                            // in pins, 1
      0x0043                             // jmp x--, loop
  };
  pio_program wlan_i2s_slave = { .instructions = wlan_prog, .length = 7, .origin = -1 };
  
  uint16_t bt_prog[7] = {
      (uint16_t)(0x2080 | I2S_IN2_LRCK),
      (uint16_t)(0x2000 | I2S_IN2_LRCK),
      0xe03f,
      (uint16_t)(0x2000 | I2S_IN2_BCLK),
      (uint16_t)(0x2080 | I2S_IN2_BCLK),
      0x4001,
      0x0043
  };
  pio_program bt_i2s_slave = { .instructions = bt_prog, .length = 7, .origin = -1 };

  // Initialize Input PIOs (I2S Slaves)
  sm_in_wlan = pio_claim_unused_sm(pio0, true);
  uint offset_in_wlan = pio_add_program(pio0, &wlan_i2s_slave);
  i2s_slave_program_init(pio0, sm_in_wlan, offset_in_wlan, I2S_IN1_DIN, I2S_IN1_BCLK);
  
  sm_in_bt = pio_claim_unused_sm(pio0, true);
  uint offset_in_bt = pio_add_program(pio0, &bt_i2s_slave);
  i2s_slave_program_init(pio0, sm_in_bt, offset_in_bt, I2S_IN2_DIN, I2S_IN2_BCLK);

  // Note: For TV, claim a third state machine (sm_in_tv) and load the S/PDIF PIO program.

  // Initialize DMA channels linking the Input FIFOs to the Output FIFO
  setup_audio_dma();
  
  // Point output DMA to the Dual Master TX FIFO
  dma_channel_set_write_addr(dma_out_chan, (volatile void*)&pio1->txf[sm_out], false);
  dma_channel_set_read_addr(dma_out_chan, audio_buffer, true);

  // 3. I2S-PIO-Clocks starten und stabilisieren lassen (passiert durch pio_sm_set_enabled)
  delay(50); // Stabilisierungs-Delay

  // 4. AMP_EN auf HIGH ziehen
  digitalWrite(AMP1_EN, HIGH);
  digitalWrite(AMP2_EN, HIGH);
  
  // 5. Volume langsam per I2C-Rampe hochfahren (Soft-Mute aufheben)
  for (int i = 0; i < 2; i++) {
    uint8_t addr = amp_addresses[i];
    // Remove Soft Mute
    Wire.beginTransmission(addr);
    Wire.write(0x2E); 
    Wire.write(0x00); // Disable Soft Mute
    Wire.endTransmission();
    
    // Ramp volume up to 0dB (0x18 is roughly 0dB or default max depending on specific register map)
    // We will set it to a safe default.
    Wire.beginTransmission(addr);
    Wire.write(0x2C); 
    Wire.write(0x18); // Default active volume
    Wire.endTransmission();
  }

  Serial.println("RP2354 DSP Booted - Kein-Plopp Sequence Complete - PIO DMA Routing Active");
}

void loop() {
  // ---------------------------------------------------------
  // UART Handshake & Priority Matrix Evaluation
  // ---------------------------------------------------------
  static bool haOverrideActive = false; 
  static float tempAmb = 0.0;
  static float tempAmp = 0.0;
  static float tempPsu = 0.0;
  
  if (Serial1.available()) {
    String cmd = Serial1.readStringUntil('\n');
    cmd.trim(); 
    
    // Check for routing overrides
    if (cmd == "OVERRIDE_START") haOverrideActive = true;
    else if (cmd == "OVERRIDE_STOP")  haOverrideActive = false;
    
    // Check for thermal telemetry from S3 (Format: "THERMAL:amb_temp:amp_temp:psu_temp")
    else if (cmd.startsWith("THERMAL:")) {
        int firstColon = cmd.indexOf(':', 8);
        int secondColon = cmd.indexOf(':', firstColon + 1);
        if (firstColon > 0 && secondColon > 0) {
            tempAmb = cmd.substring(8, firstColon).toFloat();
            tempAmp = cmd.substring(firstColon + 1, secondColon).toFloat();
            tempPsu = cmd.substring(secondColon + 1).toFloat();
        }
    }
    // Check for volume override from UI (Format: "VOL:x")
    else if (cmd.startsWith("VOL:")) {
        int vol = cmd.substring(4).toInt();
        uint8_t amp_addresses[2] = {0x20, 0x21};
        // Map 0-100 to volume register
        // On MA12070P, 0x18 is 0dB (max), 0x00 is -100dB (min)
        // Adjust the scaling to match exactly your desired register mappings.
        // For example, mapping 0-100 linearly to 0x00 to 0x18:
        uint8_t regVol = (uint8_t)((vol / 100.0) * 0x18);
        for (int i = 0; i < 2; i++) {
            Wire.beginTransmission(amp_addresses[i]);
            Wire.write(0x2C);
            Wire.write(regVol);
            Wire.endTransmission();
        }
    }
    // Check for mute toggle from UI (Format: "MUTE:x")
    else if (cmd.startsWith("MUTE:")) {
        int mute = cmd.substring(5).toInt();
        uint8_t amp_addresses[2] = {0x20, 0x21};
        for (int i = 0; i < 2; i++) {
            Wire.beginTransmission(amp_addresses[i]);
            Wire.write(0x2E);
            Wire.write(mute ? 0x01 : 0x00); // 0x01 = Soft Mute Enabled
            Wire.endTransmission();
        }
    }
    // Check for LED color from UI (Format: "LED:r:g:b:w1:w2")
    else if (cmd.startsWith("LED:")) {
        int r, g, b, w1, w2;
        if (sscanf(cmd.c_str(), "LED:%d:%d:%d:%d:%d", &r, &g, &b, &w1, &w2) == 5) {
            if (leds) {
                leds->show(r, g, b, w1, w2);
            }
        }
    }
  }
  
  // Prio 2 (S/PDIF detection)
  // Query the hardware PIO/DMA flow to verify if valid TOSLINK packets are being parsed.
  static unsigned long lastSpdifCheck = 0;
  static bool tvActive = false;
  if (millis() - lastSpdifCheck > 50) {
      if (tvDecoder) {
          // If we are currently active on TV, the DMA count will advance.
          // If we are not active, we test the FIFO level to see if it's bursting.
          if (currentSource == SRC_TV) {
              tvActive = tvDecoder->has_active_clock(dma_in_chan);
          } else {
              uint tv_sm = tvDecoder->get_sm();
              // A single rogue bit could push 1 frame. To be robust, we only consider
              // TV active if the FIFO is completely full (stalled because of continuous data).
              // The RP2040 PIO RX FIFO has a depth of 4 (or 8 if joined, our decoder joins RX).
              tvActive = (pio_sm_get_rx_fifo_level(pio0, tv_sm) >= 4);
              
              // CRITICAL: If the clock is active but we are NOT routed to TV,
              // the DMA is not draining this FIFO. We MUST manually clear it here
              // so we don't latch onto a stale signal forever.
              pio_sm_clear_fifos(pio0, tv_sm);
          }
      }
      lastSpdifCheck = millis();
  }
  
  // Prio 3 (BT Active Hardware Pin)
  bool btActive = digitalRead(BT_ACTIVE_IN) == HIGH;
  
  // Resolve Priority
  if (haOverrideActive) {
    setAudioSource(SRC_OVERRIDE);
  } else if (tvActive) {
    setAudioSource(SRC_TV);
  } else if (btActive) {
    setAudioSource(SRC_BT);
  } else {
    setAudioSource(SRC_WLAN); // Default
  }

  // Active Audio Passthrough
  // Audio routing and passthrough is handled natively by the DMA/PIO blocks 
  // required to support I2S Slave mode, and therefore does not require polling here.

  // ---------------------------------------------------------
  // Background Tasks (Faults & Thermal Management)
  // ---------------------------------------------------------
  static unsigned long lastBackgroundTask = 0;
  if (millis() - lastBackgroundTask > 2000) {
    // Monitor Amp Faults
    if (digitalRead(AMP1_FAULT) == LOW) {
      Serial.println("Amp 1 Fault detected!");
    }
    if (digitalRead(AMP2_FAULT) == LOW) {
      Serial.println("Amp 2 Fault detected!");
    }
    
    // Read RP2354 Internal Temperature
    float internalTemp = analogReadTemp();
    
    // MA12070P Temperature Check via I2C (Reg 0x60 status flags)
    // 0x60 bit 6 is temperature warning. We assign a pseudo-temperature if it's running hot,
    // but the primary thermal data comes from the external LM75 sensors passed over UART.
    float internalAmpTemp = 40.0;
    Wire.beginTransmission(0x20);
    Wire.write(0x60);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(0x20, 1) == 1) {
        uint8_t status = Wire.read();
        if (status & 0x40) internalAmpTemp = 85.0; // Temperature warning flag active!
    }
    
    // Hottest-Spot Thermal Commander Logic
    float hottest = internalTemp;
    if (internalAmpTemp > hottest) hottest = internalAmpTemp;
    if (tempAmp > hottest) hottest = tempAmp;
    if (tempAmb > hottest) hottest = tempAmb;
    if (tempPsu > hottest) hottest = tempPsu;
    
    // Send hottest/internal to S3
    Serial1.printf("TEMP:%.1f\n", internalTemp);
    
    bool fault = (digitalRead(AMP1_FAULT) == LOW || digitalRead(AMP2_FAULT) == LOW);
    Serial1.printf("FAULT:%d\n", fault ? 1 : 0);

    // Dynamic Fan Curve with Hysteresis
    static int currentFanSpeed = 0;
    
    if (hottest > 70.0) {
      currentFanSpeed = 255; // 100%
    } else if (hottest > 60.0) {
      if (currentFanSpeed < 153) currentFanSpeed = 153; // 60%
    } else if (hottest > 50.0) {
      if (currentFanSpeed < 76) currentFanSpeed = 76; // 30%
    } else if (hottest < 48.0) {
      // Hysteresis: only turn off if everything cools completely down
      currentFanSpeed = 0;
    }
    
    analogWrite(FAN_PWM, currentFanSpeed);
    
    float fanPercent = (currentFanSpeed / 255.0) * 100.0;
    Serial1.printf("FAN:%.0f\n", fanPercent);

    lastBackgroundTask = millis();
  }
}
