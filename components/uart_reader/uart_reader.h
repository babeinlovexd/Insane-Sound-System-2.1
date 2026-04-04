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
  esphome::sensor::Sensor *fan_sensor_;
  esphome::binary_sensor::BinarySensor *fault_sensor_;
  esphome::text_sensor::TextSensor *stream_status_;
  esphome::text_sensor::TextSensor *sys_stat_sensor_;

  CustomUARTReader(esphome::uart::UARTComponent *parent,
                   esphome::text_sensor::TextSensor *track,
                   esphome::text_sensor::TextSensor *artist,
                   esphome::text_sensor::TextSensor *status,
                   esphome::sensor::Sensor *temp,
                   esphome::sensor::Sensor *fan,
                   esphome::binary_sensor::BinarySensor *fault,
                   esphome::text_sensor::TextSensor *stream_status,
                   esphome::text_sensor::TextSensor *sys_stat,
                   bool *is_flashing)
      : UARTDevice(parent), track_sensor_(track), artist_sensor_(artist), status_sensor_(status), temp_sensor_(temp), fan_sensor_(fan), fault_sensor_(fault), stream_status_(stream_status), sys_stat_sensor_(sys_stat), is_flashing_(is_flashing) {}

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

    // Format 6: RP2354 System Status -> "SYS_STAT:ok"
    if (s_line.find("SYS_STAT:") == 0) {
      if (sys_stat_sensor_) {
        sys_stat_sensor_->publish_state(s_line.substr(9));
      }
      return;
    }

    // Format 4: RP2354 Fan -> "FAN:50"
    if (s_line.find("FAN:") == 0) {
      if (fan_sensor_) {
        try {
          float fan_val = std::stof(s_line.substr(4));
          fan_sensor_->publish_state(fan_val);
        } catch (...) {
          // ignore parsing errors
        }
      }
      return;
    }

    // Format 5: RP2354 Fault -> "FAULT:1"
    if (s_line.find("FAULT:") == 0) {
      if (fault_sensor_) {
        try {
          int fault_val = std::stoi(s_line.substr(6));
          fault_sensor_->publish_state(fault_val > 0);
        } catch (...) {
          // ignore parsing errors
        }
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

    // Check for stream status from WROOM
    if (s_line.find("STATUS:PLAY") == 0) {
      if (stream_status_) {
        stream_status_->publish_state("playing");
      }
      return;
    } else if (s_line.find("STATUS:PAUSE") == 0) {
      if (stream_status_) {
        stream_status_->publish_state("paused");
      }
      return;
    } else if (s_line.find("STATUS:OFFLINE") == 0) {
      if (stream_status_) {
        stream_status_->publish_state("idle");
      }
      return;
    }

    // Parse BT_VOL to sync the volume slider with phone changes
    if (s_line.find("BT_VOL:") == 0) {
      // In ESPHome, we must use id(bl_volume_slider).publish_state() to update.
      // But we don't have a direct reference to it in this component.
      // To keep it simple, we don't update the slider directly from this component,
      // the user might need a number pointer, or we just rely on HA.
      return;
    }

    // Parse Track/Artist/Album using the updated exact strings from WROOM
    if (s_line.find("TITLE:") == 0) {
      if (track_sensor_) {
        track_sensor_->publish_state(s_line.substr(6));
      }
      return;
    }

    if (s_line.find("ARTIST:") == 0) {
      if (artist_sensor_) {
        artist_sensor_->publish_state(s_line.substr(7));
      }
      return;
    }
  }
};

} // namespace custom_uart_reader
