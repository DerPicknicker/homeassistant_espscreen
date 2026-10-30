#include "chsc5816_touchscreen.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::chsc5816 {
static const char *const TAG = "chsc5816";

void CHSC5816Touchscreen::setup() {
  reset_pin_->setup();
  reset_pin_->digital_write(false);
  delay(10);
  reset_pin_->digital_write(true);
  delay(5);
  interrupt_pin_->setup();
  attach_interrupt_(interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  // Capacitive coordinates are native panel pixels; ESPHome applies the transform once.
  x_raw_min_ = 0;
  y_raw_min_ = 0;
  x_raw_max_ = display_->get_native_width();
  y_raw_max_ = display_->get_native_height();
}

void CHSC5816Touchscreen::update_touches() {
  // CHSC5816 point report at 0x2000002C, as specified in LilyGO's SensorLib example.
  const uint8_t address[] = {0x20, 0x00, 0x00, 0x2C};
  uint8_t report[8];
  if (write_read(address, sizeof(address), report, sizeof(report)) != i2c::ERROR_OK) {
    status_set_warning();
    skip_update_ = true;
    return;
  }
  if (report[0] != 0xFF || report[1] > 2) {
    status_set_warning();
    skip_update_ = true;
    return;
  }
  status_clear_warning();
  if (report[1] == 0) return;
  const uint16_t x = report[2] | ((report[5] & 0x0F) << 8);
  const uint16_t y = report[3] | ((report[5] & 0xF0) << 4);
  if (x >= display_->get_native_width() || y >= display_->get_native_height()) {
    skip_update_ = true;
    return;
  }
  add_raw_touch_position_(0, x, y);
}

void CHSC5816Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CHSC5816 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt: ", interrupt_pin_);
  LOG_PIN("  Reset: ", reset_pin_);
}
}  // namespace esphome::chsc5816
