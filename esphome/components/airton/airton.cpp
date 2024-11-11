#include "airton.h"
#include "esphome/core/log.h"

namespace esphome {
namespace airton {

static const char *const TAG = "airton.climate";
uint8_t previous_mode_ = 0;

void AirtonClimate::transmit_state() {
  // Sampled valid state
  // Power: On, Mode: 2 (Dry), Fan: 1 (Quiet), Temp: 20C, Swing(V): On, Econo: Off, Turbo: Off, Light: On, Health: On,
  // Sleep: Off. 0x74C461041A11D3
  uint8_t remote_state[AIRTON_STATE_FRAME_SIZE] = {0};

  // Header
  remote_state[0] = 0xD3;
  remote_state[1] = 0x11;

  remote_state[2] = 0;
  remote_state[2] |= this->operation_mode_();
  remote_state[2] |= (this->fan_speed_() << 4);
  remote_state[2] |= (this->turbo_control_() << 7);

  remote_state[3] = 0;
  remote_state[3] |= this->temperature_();

  remote_state[4] = 0;
  remote_state[4] |= this->swing_mode_();

  remote_state[5] = this->operation_settings_();

  remote_state[6] = 0;
  remote_state[6] |= this->checksum_(remote_state);

  ESP_LOGV(TAG, "Sending: %02X %02X %02X %02X %02X %02X %02X", remote_state[6], remote_state[5], remote_state[4],
           remote_state[3], remote_state[2], remote_state[1], remote_state[0]);

  // Build payload inside 'data'
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(AIRTON_IR_FREQUENCY);

  // Header
  data->mark(AIRTON_HEADER_MARK);
  data->space(AIRTON_HEADER_SPACE);

  // Data
  for (uint8_t payload_byte : remote_state) {
    for (uint8_t payload_bit_cursor = 0; payload_bit_cursor < 8; payload_bit_cursor++) {
      data->mark(AIRTON_BIT_MARK);
      bool bit = payload_byte & (1 << payload_bit_cursor);
      data->space(bit ? AIRTON_ONE_SPACE : AIRTON_ZERO_SPACE);
    }
  }

  // Footer
  data->mark(AIRTON_BIT_MARK);
  data->space(AIRTON_MESSAGE_SPACE);

  transmit.perform();
}

uint8_t AirtonClimate::operation_mode_() {
  uint8_t operating_mode = 0b1000;  // First bit is for power state
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= AIRTON_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= AIRTON_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= AIRTON_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode |= AIRTON_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= AIRTON_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = 0b0111 & this->previous_mode_;  // Set previous mode with power state bit off
  }
  this->previous_mode_ = operating_mode;
  return operating_mode;
}

uint16_t AirtonClimate::fan_speed_() {
  uint16_t fan_speed;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed = AIRTON_FAN_1;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = AIRTON_FAN_3;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = AIRTON_FAN_5;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = AIRTON_FAN_AUTO;
  }
  return fan_speed;
}

bool AirtonClimate::turbo_control_() {
  bool turbo_control = false;  // My remote seems to always have this set to 0
  return turbo_control;
}

uint8_t AirtonClimate::temperature_() {
  // Force 20C degrees in Fan only mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      // Fixed 25C setpoint in Auto mode
      return 9;
    default:
      uint8_t temperature = (uint8_t) roundf(clamp<float>(this->target_temperature, AIRTON_TEMP_MIN, AIRTON_TEMP_MAX));
      // Set correct temperature integer, offset by 16
      return temperature - 16;
  }
}

uint8_t AirtonClimate::swing_mode_() {
  uint8_t swing_control = 0b01100000;
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      swing_control |= 1;
      break;
    default:
      break;
  }
  return swing_control;
}

// The bits of this packet's byte have the following meanings (from MSB to LSB)
// Light, Health, Unknown, HeatOn, Unknown, NotAutoOn, Sleep, Econo
uint8_t AirtonClimate::operation_settings_() {
  uint8_t settings = 0;
  if (this->mode == climate::CLIMATE_MODE_HEAT) {  // Set heating bit if on the corresponding mode
    settings |= (1 << 4);
  }
  settings |= 0b11000100;  // Set Light, Health and NotAutoOn bits as per default
  return settings;
}

// From IRutils.h of IRremoteESP8266 library
uint8_t AirtonClimate::sum_bytes_(const uint8_t *const start, const uint16_t length) {
  uint8_t checksum = 0;
  const uint8_t *ptr;
  for (ptr = start; ptr - start < length; ptr++)
    checksum += *ptr;
  return checksum;
}
// From IRutils.h of IRremoteESP8266 library
uint8_t AirtonClimate::checksum_(const uint8_t *r_state) {
  uint8_t checksum = (uint8_t) (0x7F - this->sum_bytes_(r_state, 6)) ^ 0x2C;
  return checksum;
}

bool AirtonClimate::parse_state_frame_(uint8_t frame[]) {
  uint8_t mode = frame[2];
  if (mode & 0b00001000) {        // Check if power state bit is set
    switch (mode & 0b00000111) {  // Mask anything but the least significant 3 bits
      case AIRTON_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case AIRTON_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case AIRTON_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case AIRTON_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case AIRTON_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }
  uint8_t temperature = frame[3];
  this->target_temperature =
      (temperature & 0b00001111) + 16;  // Mask the higher half of the byte (unused), add back the offset

  uint8_t fan_mode = (frame[2] & 0b01110000) >> 4;  // Mask anything but bits 5-7, then shift them to the right

  uint8_t swing_mode = frame[4] & 0b00000001;  // Mask anything but the LSB
  if (swing_mode) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  switch (fan_mode) {
    case AIRTON_FAN_1:
    case AIRTON_FAN_2:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case AIRTON_FAN_3:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case AIRTON_FAN_4:
    case AIRTON_FAN_5:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case AIRTON_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  this->publish_state();
  return true;
}

bool AirtonClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t remote_state[AIRTON_STATE_FRAME_SIZE] = {};
  // Check header encoding
  if (!data.expect_item(AIRTON_HEADER_MARK, AIRTON_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Wrong header encoding detected!");
    return false;
  }

  // Build state bytes array from raw data received
  for (int i = 0; i < AIRTON_STATE_FRAME_SIZE; i++) {
    for (int j = 0; j < 8; j++) {
      if (data.expect_item(AIRTON_BIT_MARK, AIRTON_ONE_SPACE)) {
        remote_state[i] |= 1 << j;
      } else if (!data.expect_item(AIRTON_BIT_MARK, AIRTON_ZERO_SPACE)) {
        ESP_LOGV(TAG, "Wrong modulation encoding for: Byte %d, bit %d", i, j);
        return false;
      }
    }
  }

  // Check header contents
  if (remote_state[0] != 0xD3 || remote_state[1] != 0x11) {
    ESP_LOGV(TAG, "Wrong header contents: %02X %02X", remote_state[1], remote_state[0]);
    return false;
  }

  // Verify received packet checksum
  uint8_t checksum = this->checksum_(remote_state);
  if (remote_state[AIRTON_STATE_FRAME_SIZE - 1] != checksum) {
    ESP_LOGV(TAG, "Checksum error:\ncalculated - %02X\nreceived - %02X", checksum,
             remote_state[AIRTON_STATE_FRAME_SIZE - 1]);
    return false;
  }

  // Parse the payload
  ESP_LOGV(TAG, "Received: %02X %02X %02X %02X %02X %02X %02X", remote_state[6], remote_state[5], remote_state[4],
           remote_state[3], remote_state[2], remote_state[1], remote_state[0]);
  return this->parse_state_frame_(remote_state);
}

}  // namespace airton
}  // namespace esphome
