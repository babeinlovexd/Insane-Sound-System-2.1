#pragma once
#include "esphome.h"

namespace custom_uart_reader {

class CustomUARTReader : public esphome::Component, public esphome::uart::UARTDevice {
 protected:
  esphome::text_sensor::TextSensor *track_sensor_;
  esphome::text_sensor::TextSensor *artist_sensor_;
  esphome::text_sensor::TextSensor *status_sensor_;
  esphome::sensor::Sensor *temp_sensor_;
  esphome::globals::GlobalsComponent<bool> *is_flashing_;

  static const int max_line_length = 80;
  char buffer[max_line_length];
  int head = 0;

 public:
  esphome::sensor::Sensor *fan_sensor_;
  esphome::binary_sensor::BinarySensor *fault_sensor_;
  esphome::text_sensor::TextSensor *stream_status_;
  esphome::text_sensor::TextSensor *sys_stat_sensor_;
  esphome::text_sensor::TextSensor *album_sensor_;
  esphome::binary_sensor::BinarySensor *bt_conn_sensor_;
  esphome::sensor::Sensor *bt_vol_sensor_;

  CustomUARTReader(esphome::uart::UARTComponent *parent,
                   esphome::text_sensor::TextSensor *track,
                   esphome::text_sensor::TextSensor *artist,
                   esphome::text_sensor::TextSensor *album,
                   esphome::text_sensor::TextSensor *status,
                   esphome::sensor::Sensor *temp,
                   esphome::sensor::Sensor *fan,
                   esphome::binary_sensor::BinarySensor *fault,
                   esphome::text_sensor::TextSensor *stream_status,
                   esphome::text_sensor::TextSensor *sys_stat,
                   esphome::binary_sensor::BinarySensor *bt_conn,
                   esphome::sensor::Sensor *bt_vol,
                   esphome::globals::GlobalsComponent<bool> *is_flashing)
      : UARTDevice(parent), track_sensor_(track), artist_sensor_(artist), album_sensor_(album), status_sensor_(status), temp_sensor_(temp), fan_sensor_(fan), fault_sensor_(fault), stream_status_(stream_status), sys_stat_sensor_(sys_stat), bt_conn_sensor_(bt_conn), bt_vol_sensor_(bt_vol), is_flashing_(is_flashing) {}

  void setup() override {
    // nothing to do here
  }

  void loop() override {
    // If we are actively proxy flashing the target chip, do not consume bytes from its UART.
    // This ensures the `stream_server` receives the full binary payload untouched.
    if (is_flashing_ != nullptr && is_flashing_->value()) {
        return;
    }

    // Process a maximum of 256 bytes per loop to avoid watchdog timeouts
    // if the UART is flooded with continuous data without yields.
    int max_reads = 256;
    while (available() && max_reads-- > 0) {
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
    if (s_line.length() > 15 && s_line.find("SOURCE_CHANGED:") == 0) {
      if (status_sensor_) {
        status_sensor_->publish_state(s_line.substr(15));
      }
      return;
    }

    // Format 6: RP2354 System Status -> "SYS_STAT:ok"
    if (s_line.length() > 9 && s_line.find("SYS_STAT:") == 0) {
      if (sys_stat_sensor_) {
        sys_stat_sensor_->publish_state(s_line.substr(9));
      }
      return;
    }

    // Format 4: RP2354 Fan -> "FAN:50"
    if (s_line.length() > 4 && s_line.find("FAN:") == 0) {
      if (fan_sensor_) {
        float fan_val = atof(s_line.substr(4).c_str());
        fan_sensor_->publish_state(fan_val);
      }
      return;
    }

    // Format 5: RP2354 Fault -> "FAULT:1"
    if (s_line.length() > 6 && s_line.find("FAULT:") == 0) {
      if (fault_sensor_) {
        int fault_val = atoi(s_line.substr(6).c_str());
        fault_sensor_->publish_state(fault_val > 0);
      }
      return;
    }

    // Format 3: RP2354 Temp -> "TEMP:42.5" or "TEMP_RP:42.5"
    if (s_line.length() > 5 && s_line.find("TEMP:") == 0) {
      if (temp_sensor_) {
        float temp_val = atof(s_line.substr(5).c_str());
        temp_sensor_->publish_state(temp_val);
      }
      return;
    }

    // Support modern TEMP_RP format as well
    if (s_line.length() > 8 && s_line.find("TEMP_RP:") == 0) {
      if (temp_sensor_) {
        float temp_val = atof(s_line.substr(8).c_str());
        temp_sensor_->publish_state(temp_val);
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
    if (s_line.length() > 7 && s_line.find("BT_VOL:") == 0) {
      if (bt_vol_sensor_) {
        float vol = atof(s_line.substr(7).c_str());
        bt_vol_sensor_->publish_state(vol);
      }
      return;
    }

    // Parse BT_CONN to track connection status
    if (s_line.length() > 8 && s_line.find("BT_CONN:") == 0) {
      if (bt_conn_sensor_) {
        int conn = atoi(s_line.substr(8).c_str());
        bt_conn_sensor_->publish_state(conn > 0);
      }
      return;
    }

    // Parse Track/Artist/Album using the updated exact strings from WROOM
    if (s_line.length() > 6 && s_line.find("TITLE:") == 0) {
      if (track_sensor_) {
        track_sensor_->publish_state(s_line.substr(6));
      }
      return;
    }

    if (s_line.length() > 7 && s_line.find("ARTIST:") == 0) {
      if (artist_sensor_) {
        artist_sensor_->publish_state(s_line.substr(7));
      }
      return;
    }

    if (s_line.length() > 6 && s_line.find("ALBUM:") == 0) {
      if (album_sensor_) {
        album_sensor_->publish_state(s_line.substr(6));
      }
      return;
    }
  }
};

} // namespace custom_uart_reader
