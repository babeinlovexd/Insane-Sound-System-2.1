#pragma once
#include "esphome.h"

namespace custom_uart_reader {

class CustomUARTReader : public esphome::Component, public esphome::uart::UARTDevice {
 protected:
  esphome::text_sensor::TextSensor *track_sensor_;
  esphome::text_sensor::TextSensor *artist_sensor_;
  esphome::text_sensor::TextSensor *status_sensor_;
  esphome::sensor::Sensor *temp_sensor_;
  bool *is_flashing_;

  static const int max_line_length = 80;
  char buffer[max_line_length];
  int head = 0;

 public:
  CustomUARTReader(esphome::uart::UARTComponent *parent, 
                   esphome::text_sensor::TextSensor *track, 
                   esphome::text_sensor::TextSensor *artist, 
                   esphome::text_sensor::TextSensor *status,
                   esphome::sensor::Sensor *temp,
                   bool *is_flashing) 
      : UARTDevice(parent), track_sensor_(track), artist_sensor_(artist), status_sensor_(status), temp_sensor_(temp), is_flashing_(is_flashing) {}

  void setup() override {
    // nothing to do here
  }

  void loop() override {
    // If we are actively proxy flashing the target chip, do not consume bytes from its UART.
    // This ensures the `stream_server` receives the full binary payload untouched.
    if (is_flashing_ != nullptr && *is_flashing_) {
        return;
    }

    while (available()) {
      char c = read();
      if (c == '\n' || c == '\r') {
        buffer[head] = '\0';
        if (head > 0) {
          process_line(buffer);
        }
        head = 0;
      } else if (head < max_line_length - 1) {
        buffer[head++] = c;
      }
    }
  }

  void process_line(char *line) {
    // Check for DSP Status Format: "SOURCE_CHANGED:<name>"
    std::string s_line(line);
    if (s_line.find("SOURCE_CHANGED:") == 0) {
      if (status_sensor_) {
        status_sensor_->publish_state(s_line.substr(15));
      }
      return;
    }

    // Format 3: RP2354 Temp -> "TEMP:42.5"
    if (s_line.find("TEMP:") == 0) {
      if (temp_sensor_) {
        try {
          float temp_val = std::stof(s_line.substr(5));
          temp_sensor_->publish_state(temp_val);
        } catch (...) {
          // ignore parsing errors
        }
      }
      return;
    }

    // Check for WROOM Metadata Format: "ID:Text" (ID 1: Title, ID 2: Artist)
    if (strlen(line) >= 3 && line[1] == ':') {
      char msg_id = line[0];
      char *text = &line[2];
      if (msg_id == '1' && track_sensor_) {
        track_sensor_->publish_state(text);
      } else if (msg_id == '2' && artist_sensor_) {
        artist_sensor_->publish_state(text);
      }
    }
  }
};

} // namespace custom_uart_reader
