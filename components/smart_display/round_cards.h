// Included inside runtime_tiles, after the shared tile part and action helpers.
#ifdef SD_MOD_ROUND_FRAME
#if defined(SMART_DISPLAY_ROUND_CARDS_SITE) && !defined(SMART_DISPLAY_ROUND_CARDS)
#define SMART_DISPLAY_ROUND_CARDS
namespace round_cards {
inline bool supported(const Tile &t) {
  return t.full && ((t.domain() == "light" && tile_controls::light_dims(t)) ||
         (t.domain() == "media_player" && (t.supported & tile_controls::feature::MEDIA_VOLUME_SET)) ||
         (t.domain() == "sensor" && t.display == "graph"));
}
// Resolve the owner from the LVGL object, never a pointer to a slot: kept pages
// exchange whole Widgets sets without rebuilding their controls.
inline Widgets *owner(lv_obj_t *control) {
  auto *w = extra_owner(lv_obj_get_parent(control));
  return w && w->extra_mode == "round_disc" && w->index < model.count ? w : nullptr;
}
inline lv_obj_t *edge_arc = nullptr;
#ifdef SD_INPUT_ROTARY
// A rotary burst previews on the same arc as a finger, until its quiet commit.
inline lv_obj_t *rotary_preview = nullptr;
#endif
inline round_input::EdgeIntent edge_intent;
inline bool edge_contact = false;
inline void edge_cancel() { edge_arc = nullptr; edge_contact = false; }
inline void edge_begin(int x, int y) {
  edge_cancel(); edge_contact = true;
  // The only card in a full-disc page owns this arc. Keep its object, not a slot
  // pointer: kept pages exchange their Widgets sets whole.
  auto &w = widgets[0];
  auto *arc = w.extra_mode == "round_disc" ? w.parts[0] : nullptr;
  int cx = x, cy = y;
  if (arc && !lv_obj_has_state(arc, LV_STATE_DISABLED)) {
    lv_area_t box; lv_obj_get_coords(arc, &box);
    cx = (box.x1 + box.x2 + 1) / 2; cy = (box.y1 + box.y2 + 1) / 2;
    const int radius = std::min(lv_area_get_width(&box), lv_area_get_height(&box)) / 2;
    const double distance = std::hypot(x - cx, y - cy);
    const int track = lv_obj_get_style_arc_width(arc, LV_PART_MAIN);
    if (distance >= radius - track - ui::px(5) && distance <= radius + ui::px(2)) edge_arc = arc;
  }
  edge_intent.begin(x, y, cx, cy, ui::px(5), edge_arc != nullptr);
}
inline void edge_preview(int x, int y) {
  if (!edge_arc || !owner(edge_arc)) return;
  lv_area_t box; lv_obj_get_coords(edge_arc, &box);
  const double cx = (box.x1 + box.x2 + 1) / 2.0, cy = (box.y1 + box.y2 + 1) / 2.0;
  lv_arc_set_value(edge_arc, dial::value_at(x - cx, y - cy, lv_arc_get_value(edge_arc)));
  lv_obj_send_event(edge_arc, LV_EVENT_VALUE_CHANGED, nullptr);
}
inline bool edge_move(int x, int y) {
  if (!edge_contact) return false;
  if (edge_intent.move(x, y) == round_input::EdgeIntent::Mode::gauge) {
    round_frame::edge_owned = false;
    edge_preview(x, y);
    return true;
  }
  if (edge_intent.mode() == round_input::EdgeIntent::Mode::page) screen_input::touch_guard.consume();
  return false;
}
inline void edge_release() {
  if (!edge_contact) return;
  if (edge_arc && (edge_intent.mode() == round_input::EdgeIntent::Mode::gauge || edge_intent.tap())) {
    round_frame::edge_owned = false;
    edge_preview(edge_intent.x(), edge_intent.y());
    lv_obj_send_event(edge_arc, LV_EVENT_RELEASED, nullptr);
  } else screen_input::touch_guard.consume();
  edge_cancel();
}
inline void arc_event(lv_event_t *event) {
  auto *arc = static_cast<lv_obj_t *>(lv_event_get_current_target(event));
  if (lv_event_get_code(event) == LV_EVENT_DELETE) {
    if (edge_arc == arc) edge_cancel();
#ifdef SD_INPUT_ROTARY
    if (rotary_preview == arc) rotary_preview = nullptr;
#endif
    return;
  }
  auto *w = owner(arc);
  if (!w || !supported(model.tiles[w->index])) return;
  const auto code = lv_event_get_code(event);
  if (round_frame::edge_owned) {
    // Raw touch reserves navigation before LVGL reads the pointer. Also guard a
    // queued arc event, so a swipe never previews or sends a brightness/volume.
    const auto &t = model.tiles[w->index];
    lv_arc_set_value(arc, t.domain() == "light" && t.state != "on" ? 0 : slider_value(t));
    return;
  }
  if (code == LV_EVENT_VALUE_CHANGED) {
    // A finger's preview belongs to this control, as on the existing slider.
    // The shared commit path owns the held level once the finger lets go.
    label(w->parts[1], screen_text::percent((lv_arc_get_value(arc) + 5) / 10));
  } else if (code == LV_EVENT_RELEASED &&
             screen_input::touch_guard.accept_slider(esphome::millis(), 380)) {
    commit_slider(w->index, lv_arc_get_value(arc));
    refresh_tile(w->index);
  } else if (code == LV_EVENT_PRESS_LOST) refresh_tile(w->index);
}
inline void primary_action(Widgets &w) {
  if (w.index>=model.count || !fresh())return;
  auto &t=model.tiles[w.index];
  if(!supported(t) || !t.available() || t.waiting(esphome::millis()))return;
  if(t.domain()=="light")toggle_wish(t);
  else if(tile_controls::media_play_action(t).valid())media_action(t,20);
  refresh_tile(w.index);
}
inline void primary_event(lv_event_t *event) {
  auto *w=owner(static_cast<lv_obj_t *>(lv_event_get_current_target(event)));
  if(!w || !allowed_wish(esphome::millis(),TILE_TOUCH+w->index,model.tiles[w->index].entity))return;
  primary_action(*w);
}
inline void track_event(lv_event_t *event) {
  auto *w=owner(static_cast<lv_obj_t *>(lv_event_get_current_target(event)));
  if(!w || !enabled || !fresh())return;
  auto &t=model.tiles[w->index];
  const bool next=reinterpret_cast<intptr_t>(lv_event_get_user_data(event))!=0;
  const auto feature=next?tile_controls::feature::MEDIA_NEXT:tile_controls::feature::MEDIA_PREVIOUS;
  if(t.domain()!="media_player" || !(t.supported & feature) || !t.available() || t.waiting(esphome::millis()) ||
     !allowed_wish(esphome::millis(),TILE_TOUCH+w->index,t.entity))return;
  media_action(t,next?22:21);refresh_tile(w->index);
}
inline void track_key(Widgets &w,unsigned part,const ui::SurfaceBounds &box,bool next,bool live,const Tile &t) {
  auto *&button=w.parts[part];
  if(!button) {
    button=lv_obj_create(w.extra);lv_obj_remove_style_all(button);
    lv_obj_add_flag(button,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(button,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button,track_event,LV_EVENT_CLICKED,reinterpret_cast<void *>(static_cast<intptr_t>(next)));
    lv_obj_set_style_radius(button,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_opa(button,LV_OPA_COVER,0);
    w.parts[part+1]=lv_label_create(button);lv_obj_remove_flag(w.parts[part+1],LV_OBJ_FLAG_CLICKABLE);
  }
  round_frame::place(button,box);
  set_color(button,LV_STYLE_BG_COLOR,theme::color(theme::KEY));
  lv_obj_set_style_bg_color(button,theme::color(theme::TRACK),LV_STATE_PRESSED);
  const bool can=t.supported & (next?tile_controls::feature::MEDIA_NEXT:tile_controls::feature::MEDIA_PREVIOUS);
  set_hidden(button,!can);
  paint_state(button,LV_STATE_DISABLED,!live || !can);
  label(w.parts[part+1],next?tile_controls::glyph::NEXT:tile_controls::glyph::PREVIOUS);
  set_font(w.parts[part+1],mini_icon_font?mini_icon_font:w.icon_font);
  set_color(w.parts[part+1],LV_STYLE_TEXT_COLOR,theme::color(theme::INK));
  lv_obj_center(w.parts[part+1]);
}
inline bool album_art(Widgets &w,const Tile &t,const ui::SurfaceBounds &box) {
  const bool pictured=t.domain()=="media_player" && media_track(t) && camera_supported() && !t.extra().media_picture.empty();
  w.cover_entity=pictured?t.entity:std::string();w.cover_mark=pictured?t.extra().media_picture:std::string();
  w.cover_size=box.width;w.cover_ground=theme::hex(theme::CARD);w.cover_rect={box.x,box.y,box.width,box.height};
  auto *src=pictured?picture_of(cover_key(w.cover_entity,w.cover_mark,w.cover_size,w.cover_ground)):nullptr;
  if(src) {
    w.parts[MEDIA_PICTURE]=media_picture_show(w.extra,w.parts[MEDIA_PICTURE],w.cover_rect,src);
    w.cover_shown=cover_key(w.cover_entity,w.cover_mark,w.cover_size,w.cover_ground);w.cover_hold=0;
  }else if(w.parts[MEDIA_PICTURE]) {
    // Keep the previous track's art briefly, using the existing media hold policy.
    const uint32_t now=esphome::millis();
    const bool keep=pictured && pictures_kept() && !warming && w.cover_shown.rfind("cover|"+t.entity+"|",0)==0;
    if(keep && !w.cover_hold)w.cover_hold=(now+MEDIA_FACE_WAIT_MS)|1;
    if(!keep || static_cast<int32_t>(now-w.cover_hold)>=0) {
      lv_obj_delete(w.parts[MEDIA_PICTURE]);w.parts[MEDIA_PICTURE]=nullptr;w.cover_shown.clear();w.cover_hold=0;
    }
  }
  const bool shown=w.parts[MEDIA_PICTURE]!=nullptr;
  // An arriving cover can repaint the tile after the header was drawn.
  if(!warming)round_frame::header_ink(shown?theme::CAMERA_INK:theme::INK);
  auto *&shade=w.parts[15];
  if(shown) {
    if(!shade){shade=lv_obj_create(w.extra);lv_obj_remove_style_all(shade);lv_obj_remove_flag(shade,LV_OBJ_FLAG_CLICKABLE);}
    lv_obj_set_pos(shade,0,0);lv_obj_set_size(shade,lv_obj_get_style_width(w.extra,LV_PART_MAIN),lv_obj_get_style_height(w.extra,LV_PART_MAIN));
    lv_obj_set_style_radius(shade,LV_RADIUS_CIRCLE,0);
    set_color(shade,LV_STYLE_BG_COLOR,theme::color(theme::CAMERA_PAGE));
    set_number(shade,LV_STYLE_BG_OPA,LV_OPA_60);
    lv_obj_remove_flag(shade,LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(w.parts[MEDIA_PICTURE],0);lv_obj_move_to_index(shade,1);
  }else if(shade)lv_obj_add_flag(shade,LV_OBJ_FLAG_HIDDEN);
  return shown;
}
inline void render_graph(Widgets &w,const Tile &t,int width,int height,const lv_font_t *small,const lv_font_t *number) {
  begin_extra(w,"round_graph",width,height);
  const int line=lv_font_get_line_height(small),gap=ui::px(3);
  const auto l=dial::graph_layout(width,height,line,lv_font_get_line_height(number),line,gap,ui::px(10));
  const bool live=fresh()&&t.available();
  const std::string value=live?screen_text::with_unit(screen_text::localize(t.state),t.unit):std::string(tr(txt::ha_unavailable));
  auto *title=part_label(w,1,small,l.title.x,l.title.y,l.title.width,LV_TEXT_ALIGN_CENTER,t.name);
  auto *reading=part_label(w,2,live?number:small,l.value.x,l.value.y,l.value.width,LV_TEXT_ALIGN_CENTER,value);
  auto *caption=part_label(w,3,small,l.caption.x,l.caption.y,l.caption.width,LV_TEXT_ALIGN_CENTER,
                          tr(t.history_hours==1?txt::history_range_hour:txt::history_range_day));
  for(auto *part:{title,reading,caption}) { lv_label_set_long_mode(part,LV_LABEL_LONG_DOT);lv_obj_remove_flag(part,LV_OBJ_FLAG_HIDDEN); }
  set_color(title,LV_STYLE_TEXT_COLOR,theme::color(theme::SUBTLE));
  set_color(reading,LV_STYLE_TEXT_COLOR,theme::color(live?theme::INK:theme::SUBTLE));
  set_color(caption,LV_STYLE_TEXT_COLOR,theme::color(theme::MUTED));
  float low=INFINITY,high=-INFINITY;
  for(float sample:t.history)if(std::isfinite(sample)){low=std::min(low,sample);high=std::max(high,sample);}
  lv_point_precise_t raw[24];unsigned count=0;
  const int stroke=ui::px(2),plot_w=std::max(1,l.plot.width),plot_h=std::max(1,l.plot.height);
  for(unsigned i=0;i<t.history.size()&&i<24;++i) {
    if(!std::isfinite(t.history[i]))continue;
    const float level=high>low?(t.history[i]-low)/(high-low):0.5f;
    raw[count++]={(lv_value_precise_t)(stroke/2+i*(plot_w-stroke-1)/23),
                  (lv_value_precise_t)(plot_h-stroke/2-1-level*(plot_h-stroke-1))};
  }
  if(count==1){raw[1]=raw[0];raw[1].x=plot_w-1;count=2;}
  if(count>=2) {
    const unsigned smooth_count=smooth(raw,count,w.points,POINT_BUFFER,plot_w,plot_h);
    auto *curve=part_line(w,0,w.points,smooth_count,stroke,l.plot.x,l.plot.y);
    const auto accent=lv_color_hex(tile_controls::accent(t));
    set_color(curve,LV_STYLE_LINE_COLOR,accent);
    w.fill_points=w.points;w.fill_count=smooth_count;w.fill_x=l.plot.x;w.fill_y=l.plot.y;w.fill_base=plot_h;
    w.fill_color=accent;w.fill_opa=theme::fill_opacity();
    lv_obj_remove_flag(curve,LV_OBJ_FLAG_HIDDEN);
  } else if(w.parts[0])lv_obj_add_flag(w.parts[0],LV_OBJ_FLAG_HIDDEN);
}
inline bool render(Widgets &w, const Tile &t) {
  if (ui::surface_shape != ui::SurfaceShape::round || !supported(t)) return false;
  for (auto *part : {w.title, w.value, w.unit, w.circle, w.slider, w.progress, w.picture})
    if (part) lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
  hide_panel(w);
  lv_obj_remove_flag(w.tile, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_pad_all(w.tile, 0, 0);
  lv_obj_set_style_radius(w.tile, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(w.tile, 0, 0);
  lv_obj_set_style_bg_color(w.tile, theme::color(theme::CARD), 0);
  lv_obj_set_style_bg_opa(w.tile, t.transparent ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
  lv_obj_update_layout(w.tile);
  const int width = lv_obj_get_width(w.tile), height = lv_obj_get_height(w.tile);
  const auto *small = small_font ? small_font : control_font ? control_font : w.title_font;
  const auto *number = watch_font ? watch_font : w.value_font;
  if(t.domain()=="sensor") { render_graph(w,t,width,height,small,number);return true; }
  begin_extra(w, "round_disc", width, height);
  const bool media = t.domain() == "media_player";
  const auto *heading = media ? small : watch_icon_font ? watch_icon_font : w.icon_font;
  // Scale the existing icon bitmap, without adding another compiled font.
  constexpr int LIGHT_ICON_SCALE = 294;  // 115% in LVGL's 256-based scale.
  const int heading_line = lv_font_get_line_height(heading);
  const int heading_h = media ? heading_line : (heading_line * LIGHT_ICON_SCALE + 255) / 256;
  auto l = dial::layout(width, height, ui::px(5), ui::px(7), heading_h,
                            lv_font_get_line_height(number), 0, ui::touch_min());
  const auto media_layout=dial::media_layout(width,height,l,lv_font_get_line_height(small),ui::px(2),ui::touch_min(),ui::px(5));
  // Reuse the existing square-cover request and crop it through the round glass.
  // A full-disc, dimmed cover gives the player depth without a second image
  // variant or a costly runtime blur buffer.
  const bool art=album_art(w,t,{0,0,width,height});
  if(media)l.value=media_layout.value;
  const bool live = fresh() && t.available();
  const int raw = !media && t.state != "on" ? 0 : slider_value(t);
  const auto accent = lv_color_hex(tile_controls::accent(t));
  auto *&arc = w.parts[0];
  if (!arc) {
    arc = lv_arc_create(w.extra);
    lv_arc_set_range(arc, dial::MIN, dial::MAX);
    lv_arc_set_bg_angles(arc, dial::START, (dial::START + dial::SWEEP) % 360);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_EVENT_BUBBLE);
    // Match Tessera sliders: a dial drag is not the page's no-tap flick.
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(arc, arc_event, LV_EVENT_ALL, nullptr);
  }
  round_frame::place(arc, l.arc);
  lv_obj_set_style_arc_width(arc, l.track, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, l.track, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, theme::color(theme::TRACK), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, live ? accent : theme::color(theme::ICON_OFF), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(arc, accent, LV_PART_KNOB);
  lv_obj_set_style_pad_all(arc, ui::px(2), LV_PART_KNOB);
  paint_state(arc, LV_STATE_DISABLED, !live);
  bool preview=edge_contact && edge_arc==arc;
#ifdef SD_INPUT_ROTARY
  preview=preview || rotary_preview==arc;
#endif
  if (!lv_obj_has_state(arc, LV_STATE_PRESSED) && !preview) lv_arc_set_value(arc, dial::clamp(raw));
  const auto value = live ? screen_text::percent((lv_arc_get_value(arc) + 5) / 10) : std::string(tr(txt::ha_unavailable));
  const auto *value_font = live ? number : small;
  auto *level = part_label(w, 1, value_font, l.value.x, l.value.y, l.value.width, LV_TEXT_ALIGN_CENTER, value);
  auto *name = part_label(w, 2, heading, l.heading.x, l.heading.y + (heading_h - heading_line) / 2, l.heading.width, LV_TEXT_ALIGN_CENTER,
                          media ? (!t.extra().media_title.empty() ? t.extra().media_title : t.name) : icon_for(t));
  lv_label_set_long_mode(level, LV_LABEL_LONG_DOT);
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
  lv_obj_set_style_transform_pivot_x(name, l.heading.width / 2, 0);
  lv_obj_set_style_transform_pivot_y(name, heading_line / 2, 0);
  lv_obj_set_style_transform_scale(name, media ? 256 : LIGHT_ICON_SCALE, 0);
  set_color(level, LV_STYLE_TEXT_COLOR, theme::color(art?theme::CAMERA_INK:theme::INK));
  set_color(name, LV_STYLE_TEXT_COLOR, media ? theme::color(art?theme::CAMERA_INK:theme::SUBTLE) : accent);
  if(media) {
    auto *artist=part_label(w,3,small,media_layout.artist.x,media_layout.artist.y,media_layout.artist.width,
                           LV_TEXT_ALIGN_CENTER,t.extra().media_artist);
    lv_obj_remove_flag(artist,LV_OBJ_FLAG_HIDDEN);
    set_color(artist,LV_STYLE_TEXT_COLOR,theme::color(art?theme::CAMERA_INK:theme::SUBTLE));
    track_key(w,8,media_layout.previous,false,live,t);track_key(w,10,media_layout.next,true,live,t);
  }else for(unsigned part:{3u,8u,10u})if(w.parts[part])lv_obj_add_flag(w.parts[part],LV_OBJ_FLAG_HIDDEN);
  auto *&button = w.parts[4];
  if (!button) {
    button = lv_obj_create(w.extra); lv_obj_remove_style_all(button);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(button, primary_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    w.parts[5] = lv_label_create(button);
    lv_obj_remove_flag(w.parts[5], LV_OBJ_FLAG_CLICKABLE);
  }
  round_frame::place(button, l.action);
  lv_obj_set_style_bg_color(button, theme::color(theme::KEY), 0);
  lv_obj_set_style_bg_color(button, theme::color(theme::TRACK), LV_STATE_PRESSED);
  paint_state(button, LV_STATE_DISABLED, !live || (media && !tile_controls::media_play_action(t).valid()));
  label(w.parts[5], media ? (t.state == "playing" ? "\U000F03E4" : "\U000F040A") : tile_controls::glyph::POWER);
  set_font(w.parts[5], mini_icon_font ? mini_icon_font : w.icon_font);
  set_color(w.parts[5], LV_STYLE_TEXT_COLOR, accent);
  lv_obj_center(w.parts[5]);
  return true;
}
inline void leave(Widgets &w) {
  if (w.extra_mode != "round_disc" && w.extra_mode != "round_graph") return;
  end_extra(w);
  lv_obj_add_flag(w.tile, LV_OBJ_FLAG_CLICKABLE);
  for (auto prop : {LV_STYLE_PAD_LEFT, LV_STYLE_PAD_RIGHT, LV_STYLE_PAD_TOP, LV_STYLE_PAD_BOTTOM,
                    LV_STYLE_RADIUS, LV_STYLE_BORDER_WIDTH, LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA})
    lv_obj_remove_local_style_prop(w.tile, prop, 0);
  for (auto *part : {w.title, w.value, w.circle}) if (part) lv_obj_remove_flag(part, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace round_cards
#endif

#endif
