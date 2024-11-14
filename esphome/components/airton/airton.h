#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome {
namespace airton {

// Values for Airton SMVH09B-2A2A3NH IR Controllers
// Temperature
const uint8_t AIRTON_TEMP_MIN = 16;  // Celsius
const uint8_t AIRTON_TEMP_MAX = 31;  // Celsius

// Modes
const uint8_t AIRTON_MODE_AUTO = 0b000;
const uint8_t AIRTON_MODE_COOL = 0b001;
const uint8_t AIRTON_MODE_HEAT = 0b100;
const uint8_t AIRTON_MODE_DRY = 0b010;
const uint8_t AIRTON_MODE_FAN = 0b011;

// Fan Speed
const uint8_t AIRTON_FAN_AUTO = 0b000;
const uint8_t AIRTON_FAN_1 = 0b001;
const uint8_t AIRTON_FAN_2 = 0b010;
const uint8_t AIRTON_FAN_3 = 0b011;
const uint8_t AIRTON_FAN_4 = 0b100;
const uint8_t AIRTON_FAN_5 = 0b101;

// IR Transmission
const uint32_t AIRTON_IR_FREQUENCY = 38000;
const uint32_t AIRTON_HEADER_MARK = 6630;
const uint32_t AIRTON_HEADER_SPACE = 3350;
const uint32_t AIRTON_BIT_MARK = 400;
const uint32_t AIRTON_ONE_SPACE = 1260;
const uint32_t AIRTON_ZERO_SPACE = 430;
const uint32_t AIRTON_MESSAGE_SPACE = 100000;

// State Frame size
const uint8_t AIRTON_STATE_FRAME_SIZE = 7;

// Specific internal unit settings
struct AirtonSettings {
  bool sleep_state;
  bool display_state;
};

class AirtonClimate : public climate_ir::ClimateIR {
#ifdef USE_SWITCH
 public:
  void set_sleep_mode_switch(switch_::Switch *sw);
  void set_display_switch(switch_::Switch *sw);

 protected:
  switch_::Switch *sleep_mode_switch_{nullptr};
  switch_::Switch *display_switch_{nullptr};
#endif
 public:
  AirtonClimate()
      : climate_ir::ClimateIR(AIRTON_TEMP_MIN, AIRTON_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}
  void set_sleep_mode_state(bool state);
  bool get_sleep_mode_state() const;
  void set_display_state(bool state);
  bool get_display_state() const;

 private:
  // Save the previous operation mode inside instance
  uint8_t previous_mode_;

 protected:
  uint8_t get_previous_mode_();
  void set_previous_mode_(uint8_t mode);
  AirtonSettings settings_;
  ESPPreferenceObject airton_rtc_;

  // IR transmission payload builder
  void transmit_state() override;

  uint8_t operation_mode_();
  uint16_t fan_speed_();
  bool turbo_control_();
  uint8_t temperature_();
  uint8_t swing_mode_();
  uint8_t operation_settings_();

  uint8_t sum_bytes_(const uint8_t *start, uint16_t length);
  uint8_t checksum_(const uint8_t *r_state);

  // IR receiver buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(uint8_t const frame[]);
};

}  // namespace airton
}  // namespace esphome
