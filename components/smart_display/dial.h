#pragma once
#include <algorithm>
#include <cmath>
#include "surface_geometry.h"

// Shared geometry for a full-disc level control. No entity state or actions live here.
namespace dial {
constexpr int MIN = 0, MAX = 1000;
constexpr int START = 135, SWEEP = 270;
inline int clamp(int value) { return std::clamp(value, MIN, MAX); }
inline int angle(int value) { return START + (clamp(value) * SWEEP + MAX / 2) / MAX; }
// LVGL angles run clockwise from three o'clock. A point in the bottom gap picks
// its nearest endpoint; the centre has no direction and keeps the previous value.
inline int value_at(double x, double y, int previous) {
  if (x == 0 && y == 0) return clamp(previous);
  double degrees = std::atan2(y, x) * 180.0 / 3.14159265358979323846;
  double offset = std::fmod(degrees - START + 720.0, 360.0);
  if (offset > SWEEP) return offset < (SWEEP + 360.0) / 2 ? MAX : MIN;
  return clamp(static_cast<int>(std::lround(offset * MAX / SWEEP)));
}
// Angles here are from twelve o'clock, clockwise. Both lettering and page dots
// use this circle instead of spacing themselves across a rectangular footer.
constexpr double PI = 3.14159265358979323846;
struct OrbitPoint { double x, y; };
inline OrbitPoint orbit(double cx, double cy, double radius, double radians) {
  return {cx + radius * std::sin(radians), cy - radius * std::cos(radians)};
}
struct Dots {
  double radius = 0, step = 0;
  int diameter = 0, count = 0;
  OrbitPoint point(int i, double cx, double cy) const {
    return orbit(cx, cy, radius, PI + ((count - 1) / 2.0 - i) * step);
  }
};
inline Dots dots(int width, int height, int count, int inset, int diameter, int spacing) {
  Dots out;
  out.count = std::max(0, count);
  out.radius = std::max(0, std::min(width, height) / 2 - std::max(0, inset));
  if (!out.count || out.radius <= 0) return out;
  out.step = out.count > 1 ? std::min(std::max(1, spacing) / out.radius,
                                    (PI / 4) / (out.count - 1)) : 0;
  const double pitch = out.count > 1 ? 2 * out.radius * std::sin(out.step / 2) : diameter * 2;
  out.diameter = std::max(1, std::min(diameter, static_cast<int>(pitch * 0.55)));
  return out;
}
struct Layout {
  ui::SurfaceBounds disc, arc, heading, value, caption, action;
  int track = 0;
};
// Insets and font heights are already in this board's pixels. Chords constrain
// the whole line box, including its corners, rather than just its centre point.
inline Layout layout(int width, int height, int inset, int track, int heading_h,
                     int value_h, int caption_h, int touch) {
  Layout out;
  const int size = std::min(width, height);
  if (size <= 0) return out;
  inset = std::clamp(inset, 0, std::max(0, (size - 1) / 2));
  const int diameter = size - 2 * inset;
  out.disc = {(width - diameter) / 2, (height - diameter) / 2, diameter, diameter};
  out.arc = out.disc;
  out.track = std::clamp(track, 1, std::max(1, diameter / 8));
  const ui::SurfaceGeometry surface{width, height, ui::SurfaceShape::round};
  const int text_inset = inset + out.track * 2;
  auto band = [&](int y, int h) { return surface.band_bounds(y, y + std::max(0, h), text_inset); };
  const int cy = height / 2;
  out.heading = band(cy - diameter / 4, heading_h);
  out.value = band(cy - value_h / 2, value_h);
  out.caption = band(cy + value_h / 2, caption_h);
  // The bottom gap houses one primary action. Its square hit area fits too.
  const int key = std::max(0, std::min(touch, diameter / 3));
  out.action = {(width - key) / 2, cy + diameter / 4 - key / 2, key, key};
  while (!out.action.empty() && !surface.contains(out.action, inset)) {
    --out.action.width; --out.action.height;
    out.action.x = (width - out.action.width) / 2;
    out.action.y = cy + diameter / 4 - out.action.height / 2;
  }
  return out;
}
// The media variant keeps the same volume ring and primary key. Its artwork
// fills the disc while text and controls use safe chords inside the ring.
struct MediaLayout { ui::SurfaceBounds value, artist, previous, next; };
inline MediaLayout media_layout(int width,int height,const Layout &base,int line,int gap,int touch,int inset) {
  const ui::SurfaceGeometry glass{width,height,ui::SurfaceShape::round};
  MediaLayout out;
  out.value=base.value;
  out.value.y=std::max(base.heading.y+base.heading.height+gap,base.value.y-line);
  out.artist=glass.band_bounds(out.value.y+out.value.height+gap,
                              out.value.y+out.value.height+gap+line,inset+base.track*2);
  const int key=std::min(touch*2/3,std::min(width,height)/6);
  const int offset=base.action.width/2+key/2+gap*2;
  const int y=base.action.y+(base.action.height-key)/2;
  out.previous={width/2-offset-key/2,y,key,key};
  out.next={width/2+offset-key/2,y,key,key};
  return out;
}
// A full-disc history card has one reading and one trend. The text uses chords
// above and below the plot, so it has the same safe margins as the dial.
struct GraphLayout { ui::SurfaceBounds title, value, plot, caption; };
inline GraphLayout graph_layout(int width,int height,int title_h,int value_h,int caption_h,int gap,int inset) {
  const ui::SurfaceGeometry glass{width,height,ui::SurfaceShape::round};
  GraphLayout out;
  const int top=height*23/100;
  out.title=glass.band_bounds(top,top+title_h,inset);
  out.value=glass.band_bounds(top+title_h+gap,top+title_h+gap+value_h,inset);
  const int plot_top=height*46/100,plot_bottom=height*73/100;
  out.plot=glass.band_bounds(plot_top,plot_bottom,inset);
  out.caption=glass.band_bounds(plot_bottom+gap,plot_bottom+gap+caption_h,inset);
  return out;
}
}  // namespace dial
