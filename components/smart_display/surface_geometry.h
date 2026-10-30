#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ui {
enum class SurfaceShape : uint8_t { rectangular, round };
struct SurfaceBounds {
  int x = 0, y = 0, width = 0, height = 0;
  bool empty() const { return width <= 0 || height <= 0; }
};

// Coordinates are native canvas pixels. Bounds are half-open; their last pixel is
// x + width - 1, y + height - 1. Insets arrive from the caller's ui::px/mm scale.
struct SurfaceGeometry {
  int width, height;
  SurfaceShape shape = SurfaceShape::rectangular;

  bool contains(int x, int y, int inset = 0) const {
    inset = std::max(0, inset);
    if (width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height) return false;
    if (shape == SurfaceShape::rectangular)
      return x >= inset && y >= inset && x < width - inset && y < height - inset;
    const int64_t radius = int64_t(std::min(width, height)) - 1 - int64_t(inset) * 2;
    if (radius < 0) return false;
    // Doubled coordinates preserve the half-pixel centre of an even-sized panel.
    const int64_t dx = int64_t(x) * 2 - (width - 1), dy = int64_t(y) * 2 - (height - 1);
    return dx * dx + dy * dy <= radius * radius;
  }

  bool contains(const SurfaceBounds &bounds, int inset = 0) const {
    if (bounds.empty() || bounds.x < 0 || bounds.y < 0 ||
        int64_t(bounds.x) + bounds.width > width || int64_t(bounds.y) + bounds.height > height) return false;
    const int right = bounds.x + bounds.width - 1, bottom = bounds.y + bounds.height - 1;
    return contains(bounds.x, bounds.y, inset) && contains(right, bounds.y, inset) &&
           contains(bounds.x, bottom, inset) && contains(right, bottom, inset);
  }

  // A conservative central rectangle for existing rectangular cards. All four
  // corners fit, rather than relying on the physical bezel to hide their controls.
  SurfaceBounds content_bounds(int inset = 0) const {
    inset = std::max(0, inset);
    if (width <= 0 || height <= 0 || int64_t(inset) * 2 >= std::min(width, height)) return {};
    if (shape == SurfaceShape::rectangular)
      return {inset, inset, width - 2 * inset, height - 2 * inset};
    int low = 0, high = std::min(width, height);
    while (low < high) {
      const int side = low + static_cast<int>((int64_t(high) - low + 1) / 2);
      const SurfaceBounds candidate{(width - side) / 2, (height - side) / 2, side, side};
      if (contains(candidate, inset)) low = side;
      else high = side - 1;
    }
    return low > 0 ? SurfaceBounds{(width - low) / 2, (height - low) / 2, low, low} : SurfaceBounds{};
  }

  // Widest rectangle that fits an entire horizontal band, for future round bars.
  // The farther band edge sets the chord; text and touch areas must both fit it.
  SurfaceBounds band_bounds(int top, int bottom, int inset = 0) const {
    inset = std::max(0, inset);
    if (width <= 0 || height <= 0) return {};
    top = std::max(top, shape == SurfaceShape::rectangular ? inset : 0);
    bottom = std::min(bottom, shape == SurfaceShape::rectangular ? height - inset : height);
    if (bottom <= top) return {};
    if (shape == SurfaceShape::rectangular)
      return width > int64_t(inset) * 2 ? SurfaceBounds{inset, top, width - 2 * inset, bottom - top} : SurfaceBounds{};
    const int64_t radius = int64_t(std::min(width, height)) - 1 - int64_t(inset) * 2;
    const int64_t dy = std::max(std::abs(int64_t(top) * 2 - (height - 1)),
                              std::abs(int64_t(bottom - 1) * 2 - (height - 1)));
    if (radius < 0 || dy > radius) return {};
    const double reach = std::sqrt(double(radius * radius - dy * dy));
    int left = std::max(0, int(std::ceil((width - 1 - reach) / 2)));
    int right = std::min(width - 1, int(std::floor((width - 1 + reach) / 2)));
    // Keep square-root rounding conservative at the pixel boundary.
    while (left <= right && (!contains(left, top, inset) || !contains(left, bottom - 1, inset))) ++left;
    while (left <= right && (!contains(right, top, inset) || !contains(right, bottom - 1, inset))) --right;
    return left <= right ? SurfaceBounds{left, top, right - left + 1, bottom - top} : SurfaceBounds{};
  }
};
}  // namespace ui
