#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"

namespace esphome::chsc5816 {
class CHSC5816Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void set_interrupt_pin(InternalGPIOPin *pin) { interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { reset_pin_ = pin; }

 protected:
  void update_touches() override;
  InternalGPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
};
}  // namespace esphome::chsc5816
