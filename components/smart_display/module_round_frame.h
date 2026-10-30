#pragma once
#ifdef SD_MOD_ROUND_FRAME
#include "surface_geometry.h"
#include "ui_scale.h"
#include "lvgl.h"
#include "dial.h"
#include "round_input.h"
#include "header_bar.h"
#include "theme.h"
#include <vector>

namespace round_frame {
inline bool active = false;
// True while a contact is undecided or belongs to paging, not a level change.
inline bool edge_owned = false;
struct Layout {
  ui::SurfaceBounds title, body, previous, next, dots;
};
inline Layout layout(int width, int height, int text_h, int band) {
  const ui::SurfaceGeometry glass{width, height, ui::SurfaceShape::round};
  Layout out;
  const int size = std::min(width, height);
  (void)band;
  const int inset = std::min(ui::px(2), std::max(0, (size - 1) / 2));
  const int body = size - inset * 2;
  out.body = {(width - body) / 2, (height - body) / 2, body, body};
  const int top = inset + ui::px(20);
  out.title = glass.band_bounds(top, top + text_h + ui::px(10), inset);
  return out;
}
inline void place(lv_obj_t *object, const ui::SurfaceBounds &box) {
  if (!object) return;
  if (box.empty()) { lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_set_style_align(object, LV_ALIGN_TOP_LEFT, 0);
  lv_obj_set_pos(object, box.x, box.y);
  lv_obj_set_size(object, box.width, box.height);
}
inline Layout current;
inline lv_obj_t *lettering = nullptr;
inline std::vector<lv_obj_t *> letters;
inline void hide_header() { if (lettering) lv_obj_add_flag(lettering, LV_OBJ_FLAG_HIDDEN); }
inline void header_ink(theme::Role role) {
  for (auto *letter : letters) lv_obj_set_style_text_color(letter, theme::color(role), 0);
}

inline void grid(lv_obj_t *object, const lv_font_t *font) {
  if (!object) return;
  // Only full-disc pages opt in. Save the inherited grid styles so a page of
  // unsupported cards can still use the ordinary renderer during development.
  struct Original {
    lv_obj_t *object = nullptr;
    int x = 0, y = 0, width = 0, left = 0, right = 0, top = 0, bottom = 0, row = 0, column = 0;
    bool applied = false;
  };
  static Original old;
  if (old.object != object) {
    old = {}; old.object = object;
    old.x = lv_obj_get_style_x(object, LV_PART_MAIN); old.y = lv_obj_get_style_y(object, LV_PART_MAIN);
    old.width = lv_obj_get_style_width(object, LV_PART_MAIN);
    old.left = lv_obj_get_style_pad_left(object, LV_PART_MAIN); old.right = lv_obj_get_style_pad_right(object, LV_PART_MAIN);
    old.top = lv_obj_get_style_pad_top(object, LV_PART_MAIN); old.bottom = lv_obj_get_style_pad_bottom(object, LV_PART_MAIN);
    old.row = lv_obj_get_style_pad_row(object, LV_PART_MAIN); old.column = lv_obj_get_style_pad_column(object, LV_PART_MAIN);
  }
  if (!active) {
    hide_header();
    if (!old.applied) return;
    lv_obj_set_pos(object, old.x, old.y); lv_obj_set_width(object, old.width);
    lv_obj_set_style_pad_left(object, old.left, 0); lv_obj_set_style_pad_right(object, old.right, 0);
    lv_obj_set_style_pad_top(object, old.top, 0); lv_obj_set_style_pad_bottom(object, old.bottom, 0);
    lv_obj_set_style_pad_row(object, old.row, 0); lv_obj_set_style_pad_column(object, old.column, 0);
    old.applied = false;
    return;
  }
  auto *display = lv_display_get_default();
  const int text_h = font ? lv_font_get_line_height(font) : ui::px(16);
  current = layout(lv_display_get_horizontal_resolution(display), lv_display_get_vertical_resolution(display),
                   text_h, std::max(ui::px(18), text_h + ui::px(4)));
  place(object, current.body);
  lv_obj_set_style_pad_all(object, 0, 0);
  lv_obj_set_style_pad_row(object, 0, 0); lv_obj_set_style_pad_column(object, 0, 0);
  old.applied = true;
}
// Reuse the compiled font, rotating each UTF-8 glyph around its own centre.
// Only text/layout changes reposition it; value refreshes leave the lettering intact.
inline void header(lv_obj_t *title, lv_obj_t *clock, lv_obj_t *hold,
                   const std::string &name, const lv_font_t *font, theme::Role ink = theme::INK) {
  if (clock) lv_obj_add_flag(clock, LV_OBJ_FLAG_HIDDEN);
  if (!title || !font) return;
  lv_obj_add_flag(title, LV_OBJ_FLAG_HIDDEN);
  if (!lettering) {
    lettering = lv_obj_create(lv_obj_get_parent(title));
    lv_obj_remove_style_all(lettering);
    lv_obj_remove_flag(lettering, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_flag(lettering, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_event_cb(lettering, [](lv_event_t *) { lettering = nullptr; letters.clear(); }, LV_EVENT_DELETE, nullptr);
  }
  auto *display = lv_display_get_default();
  const int width = lv_display_get_horizontal_resolution(display), height = lv_display_get_vertical_resolution(display);
  place(lettering, {0, 0, width, height});
  lv_obj_remove_flag(lettering, LV_OBJ_FLAG_HIDDEN);
  // The header belongs immediately below its hold target, which is already
  // below detail cards and settings. A value/theme refresh must not raise it
  // above an open overlay. Account for the index shift when moving forward.
  if (hold && lv_obj_get_parent(hold) == lv_obj_get_parent(lettering)) {
    const int at = lv_obj_get_index(lettering), anchor = lv_obj_get_index(hold);
    const int below = anchor - (at < anchor ? 1 : 0);
    if (at != below) lv_obj_move_to_index(lettering, below);
  }
  static std::string previous;
  static const lv_font_t *previous_font = nullptr;
  static int previous_width = 0, previous_height = 0;
  if (previous != name || previous_font != font || previous_width != width || previous_height != height || letters.empty()) {
    previous = name; previous_font = font; previous_width = width; previous_height = height;
    const int line = lv_font_get_line_height(font);
    const double radius = std::max(1, std::min(width, height) / 2 - ui::px(18) - line / 2);
    std::vector<std::string> glyphs;
    std::vector<int> advances;
    int total = 0;
    for (size_t at = 0; at < name.size() && glyphs.size() < 64;) {
      const size_t begin = at; header_bar::next_codepoint(name, at);
      glyphs.push_back(name.substr(begin, at - begin));
      lv_point_t measured;
      lv_text_get_size(&measured, glyphs.back().c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      advances.push_back(std::max(1, static_cast<int>(measured.x))); total += advances.back();
    }
    lv_point_t measured;
    lv_text_get_size(&measured, ".", font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int period = std::max(1, static_cast<int>(measured.x));
    if (total > radius * dial::PI / 2 || (!name.empty() && glyphs.size() == 64)) {
      while (!glyphs.empty() && total + 3 * period > radius * dial::PI / 2) {
        total -= advances.back(); advances.pop_back(); glyphs.pop_back();
      }
      for (int i = 0; i < 3; ++i) { glyphs.push_back("."); advances.push_back(period); total += period; }
    }
    double travelled = -total / 2.0;
    for (size_t i = 0; i < glyphs.size(); ++i) {
      if (i == letters.size()) {
        auto *letter = lv_label_create(lettering);
        lv_obj_remove_flag(letter, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        letters.push_back(letter);
      }
      auto *letter = letters[i];
      const double angle = (travelled + advances[i] / 2.0) / radius;
      const auto point = dial::orbit(width / 2.0, height / 2.0, radius, angle);
      lv_label_set_text(letter, glyphs[i].c_str());
      lv_obj_set_style_text_font(letter, font, 0);
      lv_obj_set_size(letter, advances[i], line);
      lv_obj_set_pos(letter, std::lround(point.x - advances[i] / 2.0), std::lround(point.y - line / 2.0));
      lv_obj_set_style_transform_pivot_x(letter, advances[i] / 2, 0);
      lv_obj_set_style_transform_pivot_y(letter, line / 2, 0);
      lv_obj_set_style_transform_rotation(letter, (static_cast<int>(std::lround(angle * 1800 / dial::PI)) + 3600) % 3600, 0);
      lv_obj_remove_flag(letter, LV_OBJ_FLAG_HIDDEN);
      travelled += advances[i];
    }
    for (size_t i = glyphs.size(); i < letters.size(); ++i) lv_obj_add_flag(letters[i], LV_OBJ_FLAG_HIDDEN);
  }
  header_ink(ink);
  if (hold) place(hold, current.title);
}
// No connecting track. Even counts straddle the centre; odd counts include it.
// Tight spacing expands symmetrically, then contracts to keep many pages in the gap.
inline void dots(lv_obj_t *row, int selected, int total) {
  if (!row) return;
  auto *display = lv_display_get_default();
  const int width = lv_display_get_horizontal_resolution(display), height = lv_display_get_vertical_resolution(display);
  const auto plan = dial::dots(width, height, total, ui::px(8), ui::px(2), ui::px(5));
  const int box_w = std::max(1, static_cast<int>(std::ceil(2 * plan.radius * std::sin(dial::PI / 8))) + plan.diameter + 2);
  const int box_h = std::max(1, static_cast<int>(std::ceil(plan.radius * (1 - std::cos(dial::PI / 8)))) + plan.diameter + 2);
  const int x = (width - box_w) / 2, y = std::lround(height / 2.0 + plan.radius - box_h + plan.diameter / 2.0 + 1);
  place(row, {x, y, box_w, box_h});
  int count = 0;
  for (uint32_t i = 0; i < lv_obj_get_child_count(row); ++i) {
    auto *child = lv_obj_get_child(row, i);
    if (lv_obj_check_type(child, &lv_label_class)) { lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN); continue; }
    ++count;
  }
  while (count < plan.count) {
    auto *dot = lv_obj_create(row); lv_obj_remove_style_all(dot);
    lv_obj_remove_flag(dot, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    ++count;
  }
  int i = 0;
  for (uint32_t n = 0; n < lv_obj_get_child_count(row); ++n) {
    auto *dot = lv_obj_get_child(row, n);
    if (lv_obj_check_type(dot, &lv_label_class)) continue;
    if (i >= plan.count) { lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN); continue; }
    const auto point = plan.point(i, width / 2.0, height / 2.0);
    place(dot, {static_cast<int>(std::lround(point.x - plan.diameter / 2.0)) - x,
                static_cast<int>(std::lround(point.y - plan.diameter / 2.0)) - y, plan.diameter, plan.diameter});
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, theme::color(i == selected ? theme::INK : theme::MUTED), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    ++i;
  }
}
inline void navigation(lv_obj_t *previous, lv_obj_t *next, lv_obj_t *dots) {
  struct Saved {
    lv_obj_t *object = nullptr, *glyph = nullptr;
    int x = 0, y = 0, width = 0, height = 0, glyph_x = 0, glyph_y = 0;
    lv_align_t align = LV_ALIGN_DEFAULT, glyph_align = LV_ALIGN_DEFAULT;
    bool applied = false;
  };
  static Saved saved[3];
  lv_obj_t *objects[] = {previous, next, dots};
  for (unsigned i = 0; i < 3; ++i) {
    auto *o = objects[i]; auto &s = saved[i];
    if (!o) continue;
    if (s.object != o) {
      s = {}; s.object = o;
      s.x = lv_obj_get_style_x(o, LV_PART_MAIN); s.y = lv_obj_get_style_y(o, LV_PART_MAIN);
      s.width = lv_obj_get_style_width(o, LV_PART_MAIN); s.height = lv_obj_get_style_height(o, LV_PART_MAIN);
      s.align = lv_obj_get_style_align(o, LV_PART_MAIN);
      if (i < 2) for (uint32_t n = 0; n < lv_obj_get_child_count(o); ++n) {
        auto *child = lv_obj_get_child(o, n);
        if (!lv_obj_check_type(child, &lv_label_class)) continue;
        s.glyph = child; s.glyph_x = lv_obj_get_style_x(child, LV_PART_MAIN); s.glyph_y = lv_obj_get_style_y(child, LV_PART_MAIN);
        s.glyph_align = lv_obj_get_style_align(child, LV_PART_MAIN); break;
      }
    }
    if (active) {
      if (i < 2) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
      s.applied = true;
    } else if (s.applied) {
      lv_obj_set_style_align(o, s.align, 0); lv_obj_set_pos(o, s.x, s.y); lv_obj_set_size(o, s.width, s.height);
      if (s.glyph) { lv_obj_set_style_align(s.glyph, s.glyph_align, 0); lv_obj_set_pos(s.glyph, s.glyph_x, s.glyph_y); }
      s.applied = false;
    }
  }
}
}  // namespace round_frame
#endif
