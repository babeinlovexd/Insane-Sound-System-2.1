#include <Arduino.h>
#include <Wire.h>
#include <I2S.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "i2s_slave.h"
#include "i2s_dual_master.h"
#include "ws2805_pio.h"
#include "spdif_decoder.h"

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
#define AMP1_EN    20 // Swapped with LED_DATA
#define AMP2_EN    17

// PWM Outputs
#define FAN_PWM    18
#define BL_PWM     19

// WS2805 LED Data (1-Wire)
#define LED_DATA   16 // Swapped with AMP1_EN as requested

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

// DMA Channels & Double Buffering for DSP
int dma_in_chan;
int dma_out_chan;

#define BUF_SIZE 256
// 4 distinct buffers to prevent race conditions during DSP processing
// Since we are interleaving Amp 1 and Amp 2 for a dual-output PIO,
// 1 RX word (32-bit) generates 2 TX words (64-bit).
// So TX buffers must be twice the size.
uint32_t rx_ping_buffer[BUF_SIZE] __attribute__((aligned(1024)));
uint32_t rx_pong_buffer[BUF_SIZE] __attribute__((aligned(1024)));
uint32_t tx_ping_buffer[BUF_SIZE * 2] __attribute__((aligned(2048)));
uint32_t tx_pong_buffer[BUF_SIZE * 2] __attribute__((aligned(2048)));

volatile bool using_ping_for_rx = true;
volatile bool process_ping_buffer = false;
volatile bool process_pong_buffer = false;

// Biquad Filter Coefficients for Amp 2 (Subwoofer Low-Pass)
float sub_b0 = 1.0, sub_b1 = 0.0, sub_b2 = 0.0, sub_a1 = 0.0, sub_a2 = 0.0;
float sub_z1 = 0.0, sub_z2 = 0.0; // Mono subwoofer
float sub_gain = 1.0;

// 5-Band EQ (Peaking Biquads) for Amp 1 (Front L/R)
struct Biquad {
    float b0, b1, b2, a1, a2;
    float z1_l, z2_l; // Stereo left
    float z1_r, z2_r; // Stereo right
};

Biquad eq_filters[5] = {
    {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

void update_eq_band(int band_idx, float gain_db) {
    if (band_idx < 0 || band_idx > 4) return;

    float freqs[5] = {60.0, 230.0, 910.0, 3600.0, 14000.0};
    float w0 = 2.0 * 3.14159265 * freqs[band_idx] / 44100.0;
    float A = pow(10, gain_db / 40.0);
    float alpha = sin(w0) / (2.0 * 1.414); // Q approx 1.414

    float a0 = 1.0 + alpha / A;
    eq_filters[band_idx].b0 = (1.0 + alpha * A) / a0;
    eq_filters[band_idx].b1 = (-2.0 * cos(w0)) / a0;
    eq_filters[band_idx].b2 = (1.0 - alpha * A) / a0;
    eq_filters[band_idx].a1 = (-2.0 * cos(w0)) / a0;
    eq_filters[band_idx].a2 = (1.0 - alpha / A) / a0;
}

bool dsp_active = false;

// Fast bit interleaver: interleave Amp 1 bit and Amp 2 bit for the I2S dual master PIO.
// Input format is 32-bit sample from Amp 1 and 32-bit sample from Amp 2.
// Outputs two 32-bit words that will be fed sequentially to PIO TX FIFO.

inline uint32_t spread_bits_16(uint16_t x) {
    uint32_t res = x;
    res = (res ^ (res << 8)) & 0x00ff00ff;
    res = (res ^ (res << 4)) & 0x0f0f0f0f;
    res = (res ^ (res << 2)) & 0x33333333;
    res = (res ^ (res << 1)) & 0x55555555;
    return res;
}

void interleave_32bit(uint32_t a1, uint32_t a2, uint32_t &out1, uint32_t &out2) {
    // Magic number bit-twiddling avoids loops and saves millions of CPU cycles per second
    uint16_t a1_hi = (a1 >> 16) & 0xFFFF;
    uint16_t a2_hi = (a2 >> 16) & 0xFFFF;
    uint16_t a1_lo = a1 & 0xFFFF;
    uint16_t a2_lo = a2 & 0xFFFF;

    // First 16 bits -> out1
    out1 = (spread_bits_16(a1_hi) << 1) | spread_bits_16(a2_hi);
    // Last 16 bits -> out2
    out2 = (spread_bits_16(a1_lo) << 1) | spread_bits_16(a2_lo);
}

// DMA IRQ ONLY sets pointers and flags. Zero Math.
void __isr dma_handler() {
    dma_hw->ints0 = 1u << dma_in_chan; // Clear IRQ

    // 1. Identify which buffers were just completed
    uint32_t* next_rx_buffer = using_ping_for_rx ? rx_pong_buffer : rx_ping_buffer;
    uint32_t* next_tx_buffer = using_ping_for_rx ? tx_pong_buffer : tx_ping_buffer;

    // 2. Set up next DMA transfers IMMEDIATELY
    // We must reset the transfer count, otherwise the channel halts when reaching 0
    dma_channel_set_write_addr(dma_in_chan, next_rx_buffer, false);
    dma_channel_set_trans_count(dma_in_chan, BUF_SIZE, true);

    dma_channel_set_read_addr(dma_out_chan, next_tx_buffer, false);
    dma_channel_set_trans_count(dma_out_chan, BUF_SIZE * 2, true);

    // 3. Flag Core 1 to process the just-filled RX buffer into the just-read TX buffer
    if (using_ping_for_rx) {
        process_ping_buffer = true;
    } else {
        process_pong_buffer = true;
    }

    // Toggle the flag for the next cycle
    using_ping_for_rx = !using_ping_for_rx;
}

// Audio Math Loop running exclusively on Core 1
void setup1() {
    // Core 1 Initialization if needed
}

void loop1() {
    if (process_ping_buffer || process_pong_buffer) {
        uint32_t* rx_buf = process_ping_buffer ? rx_ping_buffer : rx_pong_buffer;
        uint32_t* tx_buf = process_ping_buffer ? tx_ping_buffer : tx_pong_buffer;

        // Process audio
        if (dsp_active) {
            for (int i = 0; i < BUF_SIZE; i += 2) {
                // Read Stereo L/R
                int32_t in_val_l = (int32_t)rx_buf[i];
                int32_t in_val_r = (int32_t)rx_buf[i+1];

                float out_f_l_a1 = (float)in_val_l;
                float out_f_r_a1 = (float)in_val_r;

                // Stereo 5-Band EQ for Amp 1 (Front L/R)
                for (int f = 0; f < 5; f++) {
                    // Left Channel
                    float temp_l = out_f_l_a1 * eq_filters[f].b0 + eq_filters[f].z1_l;
                    eq_filters[f].z1_l = out_f_l_a1 * eq_filters[f].b1 + eq_filters[f].z2_l - eq_filters[f].a1 * temp_l;
                    eq_filters[f].z2_l = out_f_l_a1 * eq_filters[f].b2 - eq_filters[f].a2 * temp_l;
                    out_f_l_a1 = temp_l;

                    // Right Channel
                    float temp_r = out_f_r_a1 * eq_filters[f].b0 + eq_filters[f].z1_r;
                    eq_filters[f].z1_r = out_f_r_a1 * eq_filters[f].b1 + eq_filters[f].z2_r - eq_filters[f].a1 * temp_r;
                    eq_filters[f].z2_r = out_f_r_a1 * eq_filters[f].b2 - eq_filters[f].a2 * temp_r;
                    out_f_r_a1 = temp_r;
                }

                // Low-Pass Filter for Amp 2 (Subwoofer) - Mono summed
                // 1. Mix Front-Stereo-Signal to Mono
                float in_mono = ((float)in_val_l + (float)in_val_r) * 0.5f;

                // 2. Pass Mono mix through Low-Pass Filter
                float out_f_mono_a2 = in_mono * sub_b0 + sub_z1;
                sub_z1 = in_mono * sub_b1 + sub_z2 - sub_a1 * out_f_mono_a2;
                sub_z2 = in_mono * sub_b2 - sub_a2 * out_f_mono_a2;

                // Apply Subwoofer Gain
                out_f_mono_a2 *= sub_gain;

                // Cast back to 32-bit integers
                uint32_t amp1_l = (uint32_t)((int32_t)out_f_l_a1);
                uint32_t amp1_r = (uint32_t)((int32_t)out_f_r_a1);
                uint32_t amp2_mono = (uint32_t)((int32_t)out_f_mono_a2);

                // Interleave bits for DOUT1 and DOUT2 output PIO (out pins, 2)
                uint32_t tx_l1, tx_l2, tx_r1, tx_r2;
                interleave_32bit(amp1_l, amp2_mono, tx_l1, tx_l2); // L channel: Amp 1 L, Amp 2 Mono
                interleave_32bit(amp1_r, amp2_mono, tx_r1, tx_r2); // R channel: Amp 1 R, Amp 2 Mono

                // The TX buffer needs to be twice as big (BUF_SIZE * 2)
                // because 32 bits from Amp1 + 32 bits from Amp2 = 64 bits interleaved.
                tx_buf[i * 2] = tx_l1;
                tx_buf[i * 2 + 1] = tx_l2;
                tx_buf[i * 2 + 2] = tx_r1;
                tx_buf[i * 2 + 3] = tx_r2;
            }
        } else {
            // No DSP, pass through but still must interleave identical signals to support Dual Master PIO
            for (int i = 0; i < BUF_SIZE; i += 2) {
                uint32_t in_l = rx_buf[i];
                uint32_t in_r = rx_buf[i+1];

                uint32_t tx_l1, tx_l2, tx_r1, tx_r2;
                interleave_32bit(in_l, in_l, tx_l1, tx_l2);
                interleave_32bit(in_r, in_r, tx_r1, tx_r2);

                tx_buf[i * 2] = tx_l1;
                tx_buf[i * 2 + 1] = tx_l2;
                tx_buf[i * 2 + 2] = tx_r1;
                tx_buf[i * 2 + 3] = tx_r2;
            }
        }

        // Clear flag
        if (process_ping_buffer) process_ping_buffer = false;
        else process_pong_buffer = false;
    }
}

void switch_audio_dma_source(volatile void* pio_sm_rx_fifo_addr, uint dreq) {
  dma_channel_abort(dma_in_chan);
  
  dma_channel_config in_config = dma_channel_get_default_config(dma_in_chan);
  channel_config_set_transfer_data_size(&in_config, DMA_SIZE_32);
  channel_config_set_read_increment(&in_config, false);
  channel_config_set_write_increment(&in_config, true);
  channel_config_set_dreq(&in_config, dreq);
  dma_channel_set_config(dma_in_chan, &in_config, false);
  
  dma_channel_set_read_addr(dma_in_chan, pio_sm_rx_fifo_addr, true);
}

void setAudioSource(AudioSource newSource) {
  if (currentSource == newSource) return;
  currentSource = newSource;
  
  String sourceName = "";
  switch (newSource) {
    case SRC_WLAN:    
      sourceName = "WLAN"; 
      // Route I2S_IN1 (S3) to DMA
      switch_audio_dma_source((volatile void*)&pio0->rxf[sm_in_wlan], pio_get_dreq(pio0, sm_in_wlan, false));
      break;
    case SRC_BT:      
      sourceName = "Bluetooth"; 
      // Route I2S_IN2 (WROOM) to DMA
      switch_audio_dma_source((volatile void*)&pio0->rxf[sm_in_bt], pio_get_dreq(pio0, sm_in_bt, false));
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
      switch_audio_dma_source((volatile void*)&pio0->rxf[sm_in_wlan], pio_get_dreq(pio0, sm_in_wlan, false));
      break;
    default: return;
  }
  
  // Send status update to ESP32-S3 over Hardware UART (GP22 TX / GP21 RX)
  Serial1.printf("SOURCE_CHANGED:%s\n", sourceName.c_str());
  
  // Also log to USB console
  Serial.printf("SOURCE_CHANGED:%s\n", sourceName.c_str());
}

void setup_audio_dma() {
  // Claim DMA channels
  dma_in_chan = dma_claim_unused_channel(true);
  dma_out_chan = dma_claim_unused_channel(true);
  
  // Setup Input DMA
  dma_channel_config in_config = dma_channel_get_default_config(dma_in_chan);
  channel_config_set_transfer_data_size(&in_config, DMA_SIZE_32);
  channel_config_set_read_increment(&in_config, false);
  channel_config_set_write_increment(&in_config, true);

  dma_channel_set_irq0_enabled(dma_in_chan, true);
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
  irq_set_enabled(DMA_IRQ_0, true);

  // Setup Output DMA
  dma_channel_config out_config = dma_channel_get_default_config(dma_out_chan);
  channel_config_set_transfer_data_size(&out_config, DMA_SIZE_32);
  channel_config_set_read_increment(&out_config, true);
  channel_config_set_write_increment(&out_config, false);
  
  // The Output DMA needs to transfer BUF_SIZE * 2 words because we expanded the 32-bit stream
  // into an interleaved 64-bit dual-channel stream for the PIO 'out pins, 2' instruction.
  dma_channel_configure(dma_out_chan, &out_config, 
      NULL, // Destination (Set later)
      tx_ping_buffer,
      BUF_SIZE * 2, false);
      
  dma_channel_configure(dma_in_chan, &in_config, 
      rx_ping_buffer,
      NULL, // Source (Set dynamically)
      BUF_SIZE, false);
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
  analogWriteFreq(100); // 100 Hz for FAN PWM
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
    // PBTL is configured via hardware pins MSE0/MSE1, NOT via I2C Reg 0x25.
    // Both amps must explicitly read a 24-bit stream to prevent heavy distortion.
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

    // Volume initial -100dB (Reg 0x40)
    Wire.beginTransmission(addr);
    Wire.write(0x40);
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
  i2s_slave_program_init(pio0, sm_in_wlan, offset_in_wlan, I2S_IN1_DIN);
  
  sm_in_bt = pio_claim_unused_sm(pio0, true);
  uint offset_in_bt = pio_add_program(pio0, &bt_i2s_slave);
  i2s_slave_program_init(pio0, sm_in_bt, offset_in_bt, I2S_IN2_DIN);

  // Note: For TV, claim a third state machine (sm_in_tv) and load the S/PDIF PIO program.

  // Initialize DMA channels linking the Input FIFOs to the Output FIFO
  setup_audio_dma();
  
  // Point output DMA to the Dual Master TX FIFO
  dma_channel_set_write_addr(dma_out_chan, (volatile void*)&pio1->txf[sm_out], false);
  dma_channel_set_read_addr(dma_out_chan, tx_ping_buffer, true);

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
    Wire.write(0x40);
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
  
  static String uart_buffer = "";
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n') {
        String cmd = uart_buffer;
        cmd.trim();
        uart_buffer = "";

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
        uint8_t regVol = (uint8_t)((vol / 100.0) * 0x18);
        for (int i = 0; i < 2; i++) {
            Wire.beginTransmission(amp_addresses[i]);
            Wire.write(0x40);
            Wire.write(regVol);
            Wire.endTransmission();
        }
    }
    // Check for master volume limit (Format: "VOL_LIMIT:x")
    else if (cmd.startsWith("VOL_LIMIT:")) {
        int limit = cmd.substring(10).toInt();
        uint8_t amp_addresses[2] = {0x20, 0x21};
        uint8_t limitVol = (uint8_t)((limit / 100.0) * 0x18);
        for (int i = 0; i < 2; i++) {
            Wire.beginTransmission(amp_addresses[i]);
            Wire.write(0x40);
            Wire.write(limitVol);
            Wire.endTransmission();
        }
        Serial.printf("Volume Limit set to %d%%\n", limit);
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
    // Check for LED color from UI (Format: "LED: r:g:b:w1:w2" or "LED:r:g:b:w1:w2")
    // The explicit user request format: z. B. LED: 255:0:0:100:100
    else if (cmd.startsWith("LED:")) {
        int r, g, b, w1, w2;
        // Check for both with and without space
        if (sscanf(cmd.c_str(), "LED: %d:%d:%d:%d:%d", &r, &g, &b, &w1, &w2) == 5 ||
            sscanf(cmd.c_str(), "LED:%d:%d:%d:%d:%d", &r, &g, &b, &w1, &w2) == 5) {
            if (leds) {
                leds->show(r, g, b, w1, w2);
            }
        }
    }
    // Check for fan control from UI (Format: "FAN:x")
    else if (cmd.startsWith("FAN:")) {
        int fanCmd = cmd.substring(4).toInt(); // 0 to 100
        int pwmValue = 0;
        if (fanCmd > 0) {
            // Min Power: 0.30 (Lüfter braucht mindestens 30% PWM-Signal zum Anlaufen).
            if (fanCmd < 30) fanCmd = 30;
            pwmValue = (fanCmd * 255) / 100;
        } else {
            // Zero means Zero: Bei einem Wert von 0 muss das PWM-Signal komplett abgeschaltet werden
            pwmValue = 0;
        }
        analogWrite(FAN_PWM, pwmValue);
    }
    // Check for Display Backlight (DIM) control (Format: "DIM:x")
    else if (cmd.startsWith("DIM:")) {
        int dimCmd = cmd.substring(4).toInt(); // 0 to 100
        // Map 0-100 to 8-bit PWM (0-255)
        int pwmValue = (dimCmd * 255) / 100;
        analogWrite(BL_PWM, pwmValue);
        Serial.printf("Backlight set to %d%%\n", dimCmd);
    }
    // Check for DSP crossover logic (Format: "CROSSOVER:x")
    else if (cmd.startsWith("CROSSOVER:")) {
        int crossoverFreq = cmd.substring(10).toInt();
        // The S3 calculates exact coefficients, but if it only sends the frequency,
        // the RP calculates simple low-pass coefficients for Subwoofer (Amp 2).
        float w0 = 2.0 * 3.14159265 * crossoverFreq / 44100.0;
        float alpha = sin(w0) / (2.0 * 0.707);
        float a0 = 1.0 + alpha;
        sub_b0 = (1.0 - cos(w0)) / 2.0 / a0;
        sub_b1 = (1.0 - cos(w0)) / a0;
        sub_b2 = (1.0 - cos(w0)) / 2.0 / a0;
        sub_a1 = -2.0 * cos(w0) / a0;
        sub_a2 = (1.0 - alpha) / a0;
        dsp_active = true;
        Serial.printf("Crossover active: %d Hz\n", crossoverFreq);
    }
    // Check for EQ logic (Format: "EQ_BASS:x")
    else if (cmd.startsWith("EQ_BASS:")) {
        int eqBass = cmd.substring(8).toInt();
        // Adjust bass gain dynamically for Subwoofer
        sub_gain = 1.0 + (eqBass * 0.1); // +1 unit = +10% gain
        dsp_active = true;
        Serial.printf("EQ_BASS active: %d\n", eqBass);
    }
    // Check for 5-Band EQ logic for Amp 1 (Format: "EQ_BAND:band_index:gain_db")
    else if (cmd.startsWith("EQ_BAND:")) {
        int band;
        float gain_db; // -10 to +10
        if (sscanf(cmd.c_str(), "EQ_BAND:%d:%f", &band, &gain_db) == 2) {
            if (band >= 1 && band <= 5) {
                update_eq_band(band - 1, gain_db);
                dsp_active = true;
                Serial.printf("EQ_BAND %d set to %.2f dB\n", band, gain_db);
            }
        }
    }
        // Direct Biquad coefficients from S3 (Format: "BIQUAD:b0:b1:b2:a1:a2")
        else if (cmd.startsWith("BIQUAD:")) {
            sscanf(cmd.c_str(), "BIQUAD:%f:%f:%f:%f:%f", &sub_b0, &sub_b1, &sub_b2, &sub_a1, &sub_a2);
            dsp_active = true;
            Serial.println("Biquad coefficients updated from S3");
        }
    } else {
        uart_buffer += c;
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
  
  // Last Active Wins Logic for I2S/BT and SPDIF
  // We keep track of what was playing before BT or TV interrupted
  static AudioSource previousSource = SRC_WLAN;
  static bool wasBtActive = false;
  static bool wasTvActive = false;

  // Detect falling edges to revert source
  if (wasBtActive && !btActive && currentSource == SRC_BT) {
      setAudioSource(previousSource);
  }
  if (wasTvActive && !tvActive && currentSource == SRC_TV) {
      setAudioSource(previousSource);
  }

  // Detect rising edges to jump to new source
  if (!wasBtActive && btActive) {
      if (currentSource != SRC_BT) previousSource = currentSource;
      setAudioSource(SRC_BT);
  } else if (!wasTvActive && tvActive) {
      if (currentSource != SRC_TV) previousSource = currentSource;
      setAudioSource(SRC_TV);
  }

  wasBtActive = btActive;
  wasTvActive = tvActive;

  // Resolve Override (Highest Prio)
  if (haOverrideActive) {
    setAudioSource(SRC_OVERRIDE);
  } else if (currentSource == SRC_OVERRIDE) {
      // If override is lifted, revert to the active media source
      if (btActive) {
          setAudioSource(SRC_BT);
      } else if (tvActive) {
          setAudioSource(SRC_TV);
      } else {
          setAudioSource(SRC_WLAN);
      }
  }

  // Active Audio Passthrough
  // Audio routing and passthrough is handled natively by the DMA/PIO blocks 
  // required to support I2S Slave mode, and therefore does not require polling here.

  // ---------------------------------------------------------
  // Background Tasks (Faults & Thermal Management)
  // ---------------------------------------------------------
  static unsigned long lastBackgroundTask = 0;
  if (millis() - lastBackgroundTask > 2000) {
    // Read RP2354 Internal Temperature
    float internalTemp = analogReadTemp();
    
    // I2C Fault & Status Polling from MA12070P (Address 0x20 and 0x21)
    uint8_t reg71 = 0; // error_now
    uint8_t reg1B = 0; // monitor_clip
    float maxAmpTemp = 40.0;

    uint8_t amp_addresses[2] = {0x20, 0x21};
    for (int i = 0; i < 2; i++) {
        uint8_t addr = amp_addresses[i];

        // Read Reg 0x71
        Wire.beginTransmission(addr);
        Wire.write(0x71);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)addr, (size_t)1) == 1) {
            reg71 |= Wire.read();
        }

        // Read Reg 0x1B
        Wire.beginTransmission(addr);
        Wire.write(0x1B);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)addr, (size_t)1) == 1) {
            reg1B |= Wire.read();
        }

        // Read Reg 0x60
        Wire.beginTransmission(addr);
        Wire.write(0x60);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)addr, (size_t)1) == 1) {
            if (Wire.read() & 0x40) {
                maxAmpTemp = 85.0; // Overrides if any amp is hot
            }
        }
    }
    
    // Determine System Status String based on priorities
    String sysStatus = "ok";

    if (reg71 & 0x01) sysStatus = "OCP";
    else if (reg71 & 0x02) sysStatus = "OTP";
    else if (reg71 & 0x04) sysStatus = "UVLO";
    else if (reg71 & 0x08) sysStatus = "OVLO";
    else if (reg71 & 0x10) sysStatus = "VCF";
    else if (reg71 & 0x20) sysStatus = "DC-PROT";
    else if (reg71 & 0x40) sysStatus = "MCLK-ERR";
    else if (reg1B & 0x03) sysStatus = "CLIPPING";

    // Send to S3 (Bidirectional telemetry)
    Serial1.printf("TEMP_RP:%.1f\n", internalTemp);
    Serial1.printf("TEMP_AMPS:%.1f\n", maxAmpTemp);
    Serial1.printf("SYS_STAT:%s\n", sysStatus.c_str());

    bool fault = (digitalRead(AMP1_FAULT) == LOW || digitalRead(AMP2_FAULT) == LOW);
    Serial1.printf("FAULT:%d\n", fault ? 1 : 0);

    lastBackgroundTask = millis();
  }
}
