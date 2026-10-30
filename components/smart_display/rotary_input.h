#pragma once
#include <cstdint>

namespace screen_input {

// Value adjustment only: the ESPHome rotary_encoder component owns quadrature decoding.
// Call reset() when changing the focused control, waking, or replacing a layout.
// The caller applies the entity's step and limits, then uses its normal action path.
class RotaryAcceleration {
 public:
  void reset() { have_previous_ = false; }

  int step(int direction, uint32_t now, bool adjusting_value) {
    if (direction == 0) return 0;
    const int sign = direction > 0 ? 1 : -1;
    if (!adjusting_value) {
      reset();
      return sign;
    }
    int multiplier = 1;
    const uint32_t elapsed = now - previous_ms_;
    if (have_previous_ && sign == previous_direction_ && elapsed > 0) {
      if (elapsed <= FAST_MS) multiplier = 5;
      else if (elapsed <= QUICK_MS) multiplier = 2;
    }
    // Events delivered together have no reliable physical timing. Keep those precise.
    previous_ms_ = now;
    previous_direction_ = sign;
    have_previous_ = true;
    return sign * multiplier;
  }

 private:
  // Initial commissioning thresholds, in milliseconds between decoded steps.
  static constexpr uint32_t FAST_MS = 60;
  static constexpr uint32_t QUICK_MS = 120;
  uint32_t previous_ms_ = 0;
  int previous_direction_ = 0;
  bool have_previous_ = false;
};

// The GPIO filter owns electrical debounce. This state only distinguishes a
// click, a hold and a press used as a page-navigation modifier.
class RotaryButton {
 public:
  enum class Release { none, click, hold };
  void press(uint32_t now, bool accept) { down_=true; used_=blocked_=!accept; since_=now; }
  bool down() const { return down_; }
  bool suppressed() const { return down_ && blocked_; }
  void turn() { if(down_)used_=true; }
  void cancel() { if(down_)used_=blocked_=true; }
  Release release(uint32_t now) {
    if(!down_)return Release::none;
    down_=false;
    if(used_)return Release::none;
    return now-since_>=800?Release::hold:Release::click;
  }
 private:
  bool down_=false,used_=false,blocked_=false;
  uint32_t since_=0;
};

}  // namespace screen_input
