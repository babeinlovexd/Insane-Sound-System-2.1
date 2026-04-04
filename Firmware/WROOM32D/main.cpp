#include <Arduino.h>
#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;

// I2S Pins
#define I2S_BCLK 26
#define I2S_LRCK 25
#define I2S_DOUT 22

// UART Pins for Metadata (to S3)
#define UART_TX 17
#define UART_RX 16 

// Status Pin (to RP2354)
#define BT_ACTIVE_PIN 18

void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
  if (state == ESP_A2D_AUDIO_STATE_STARTED) {
    digitalWrite(BT_ACTIVE_PIN, HIGH);
    Serial.println("BT Audio Playing -> Active Signal HIGH");
  } else if (state == ESP_A2D_AUDIO_STATE_STOPPED || state == ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND) {
    digitalWrite(BT_ACTIVE_PIN, LOW);
    Serial.println("BT Audio Stopped -> Active Signal LOW");
  }
}

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  // Send metadata over UART1 to the S3
  // Format: ID:Text\n
  // ID 1: Title, ID 2: Artist, ID 4: Album
  Serial2.printf("%d:%s\n", id, text);
}

void setup() {
  Serial.begin(115200); // Used for proxy flashing and debugging
  Serial2.begin(115200, SERIAL_8N1, UART_RX, UART_TX); // Used purely for Metadata

  Serial.println("Starting Insane Sound System V6 - BT Receiver");

  // Configure I2S
  // Set WROOM to I2S Master Mode (RP2354 acts as Slave via PIO)
  i2s_pin_config_t my_pin_config = {
      .bck_io_num = I2S_BCLK,
      .ws_io_num = I2S_LRCK,
      .data_out_num = I2S_DOUT,
      .data_in_num = I2S_PIN_NO_CHANGE
  };
  a2dp_sink.set_pin_config(my_pin_config);

  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 44100,
      .bits_per_sample = (i2s_bits_per_sample_t)16,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
      .intr_alloc_flags = 0, // default interrupt priority
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false
  };
  a2dp_sink.set_i2s_config(i2s_config);

  // Setup Status Pin
  pinMode(BT_ACTIVE_PIN, OUTPUT);
  digitalWrite(BT_ACTIVE_PIN, LOW);

  // Setup callbacks
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_on_audio_state_changed(audio_state_changed);

  // Start Bluetooth A2DP Sink
  a2dp_sink.start("Insane_Audio_V6");
}

void loop() {
  // A2DP sink handles everything in the background
  delay(1000);
}
