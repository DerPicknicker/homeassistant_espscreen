#pragma once
#include <algorithm>
#include <cmath>

namespace round_input {
// Resolve intent before sending a level. Radial/inward travel pages; tangential
// travel adjusts the ring. A decision stays with this contact until release.
class EdgeIntent {
 public:
  enum class Mode { pending, page, gauge };
  void begin(int x, int y, int cx, int cy, int threshold, bool gauge) {
    x_ = last_x_ = x; y_ = last_y_ = y; nx_ = x - cx; ny_ = y - cy;
    threshold_ = std::max(1, threshold); distance_ = 0; gauge_ = gauge;
    mode_ = gauge ? Mode::pending : Mode::page;
  }
  Mode move(int x, int y) {
    last_x_ = x; last_y_ = y;
    const double dx = x - x_, dy = y - y_;
    distance_ = std::max(distance_, std::hypot(dx, dy));
    if (mode_ != Mode::pending || distance_ < threshold_) return mode_;
    const double radial = std::abs(dx * nx_ + dy * ny_);
    const double tangent = std::abs(-dx * ny_ + dy * nx_);
    mode_ = tangent > radial * 1.35 ? Mode::gauge : Mode::page;
    return mode_;
  }
  bool tap() const { return gauge_ && mode_ == Mode::pending && distance_ < std::max(2, threshold_ / 3); }
  Mode mode() const { return mode_; }
  int x() const { return last_x_; }
  int y() const { return last_y_; }
 private:
  int x_ = 0, y_ = 0, last_x_ = 0, last_y_ = 0, nx_ = 0, ny_ = 0, threshold_ = 1;
  double distance_ = 0;
  bool gauge_ = false;
  Mode mode_ = Mode::pending;
};
// Reuse Tessera's inclusion rules. A detail page is never pulled into the loop.
template<class Pages>
inline int wrapped_page(const Pages &pages, int index, int direction) {
  const int target = pages.step(index, direction);
  if (target != index || !pages.included(index) || (direction != -1 && direction != 1)) return target;
  for (int i = direction > 0 ? 0 : static_cast<int>(pages.records.size()) - 1;
       i >= 0 && i < static_cast<int>(pages.records.size()); i += direction)
    if (pages.included(i)) return i;
  return index;
}
}  // namespace round_input
