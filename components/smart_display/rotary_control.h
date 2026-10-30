// Included inside runtime_tiles after the shared navigation and action helpers.
#if defined(SD_INPUT_ROTARY) && defined(SMART_DISPLAY_ROTARY_CONTROL_SITE) && !defined(SMART_DISPLAY_ROTARY_CONTROL)
#define SMART_DISPLAY_ROTARY_CONTROL
namespace rotary {
inline std::function<bool()> activity;  // wakes and consumes the first input; keeps idle timers alive
inline std::function<bool()> blocked;   // calibration, standby and alerts own their input
inline screen_input::RotaryAcceleration acceleration;
inline screen_input::RotaryButton button;
inline std::string context, press_target;
inline lv_timer_t *quiet_timer=nullptr;
inline uint32_t last_turn=0;
inline int preview_page=-1;
inline void cancel_preview() {
  if(quiet_timer){lv_timer_delete(quiet_timer);quiet_timer=nullptr;}
#ifdef SD_MOD_ROUND_FRAME
  auto *arc=round_cards::rotary_preview;
  auto *w=arc?round_cards::owner(arc):nullptr;
  round_cards::rotary_preview=nullptr;
  if(w)refresh_tile(w->index);
#endif
}
inline void reset() { cancel_preview();acceleration.reset();context.clear();button.cancel(); }
inline bool allowed_input() {
  if(!blocked || blocked())return false;
  for(auto *indev=lv_indev_get_next(nullptr);indev;indev=lv_indev_get_next(indev))
    if(lv_indev_get_state(indev)==LV_INDEV_STATE_PRESSED)return false;
  return true;
}
inline bool overview() {
  if(settings_screen::visible() || card_open() || camera_visible() || media_library::visible() || media_library::menu_visible())return false;
#ifdef SD_MOD_DOCK
  if(dock::open())return false;
#endif
  return true;
}
inline int steps(int direction,const std::string &next,bool adjusting) {
  if(context!=next){acceleration.reset();context=next;}
  return acceleration.step(direction,esphome::millis(),adjusting);
}
#ifdef SD_MOD_ROUND_FRAME
inline Widgets *dial_control() {
  if(!round_frame::active || !overview() || !navigation_ready() || !fresh())return nullptr;
  auto &w=widgets[0];
  if(w.index>=model.count || w.extra_mode!="round_disc" || !w.parts[0] ||
     !round_cards::supported(model.tiles[w.index]) || !model.tiles[w.index].available())return nullptr;
  return &w;
}
inline void commit_preview(lv_timer_t *) {
  auto *arc=round_cards::rotary_preview;
  auto *w=dial_control();
  if(!allowed_input() || !w || w->parts[0]!=arc || !shown_page || *shown_page!=preview_page) {cancel_preview();return;}
  const auto idle=esphome::millis()-last_turn;
  if(idle<120)return;
  if(model.tiles[w->index].waiting(esphome::millis())) {
    if(idle>=1000)cancel_preview();
    return;
  }
  const auto index=w->index;const int raw=lv_arc_get_value(arc);
  // Let the normal slider/action/report path own the sent value from here on.
  round_cards::rotary_preview=nullptr;
  lv_timer_delete(quiet_timer);quiet_timer=nullptr;
  commit_slider(index,raw);refresh_tile(index);
}
#endif
inline void turn(int direction) {
  if(!direction || button.suppressed())return;
  button.turn();
  if(!activity || !activity() || !allowed_input()){reset();return;}
  if(settings_screen::visible()) {
    cancel_preview();
    const auto *row=settings_screen::knob_selected();if(!row)return;
    const bool edit=settings_screen::knob_editing && !button.down() && settings_screen::knob_numeric(*row);
    const auto key=std::string("settings/")+std::to_string(settings_screen::current_page)+"/"+
                   std::to_string(settings_screen::knob_row)+(edit?"/edit":"/browse");
    settings_screen::knob_turn(steps(direction,key,edit),button.down());return;
  }
  if(!overview()){reset();return;}
  if(button.down()) {
    cancel_preview();steps(direction,"pages",false);
    touch_input::step_page(direction>0?1:-1);return;
  }
#ifdef SD_MOD_ROUND_FRAME
  if(auto *w=dial_control()) {
    const int delta=steps(direction,"dial/"+std::to_string(*shown_page)+"/"+model.tiles[w->index].entity,true)*10;
    auto *arc=w->parts[0];
    const int before=lv_arc_get_value(arc),next=std::clamp(before+delta,dial::MIN,dial::MAX);
    if(next==before)return;
    round_cards::rotary_preview=arc;preview_page=*shown_page;last_turn=esphome::millis();
    lv_arc_set_value(arc,next);lv_obj_send_event(arc,LV_EVENT_VALUE_CHANGED,nullptr);
    if(!quiet_timer)quiet_timer=lv_timer_create(commit_preview,40,nullptr);
    return;
  }
#endif
  cancel_preview();steps(direction,"pages",false);touch_input::step_page(direction>0?1:-1);
}
// Releasing a button held across a touch page turn or an externally opened
// screen must not activate a control on the newly shown page.
inline std::string target() {
  if(settings_screen::visible())return "settings/"+std::to_string(settings_screen::current_page)+"/"+std::to_string(settings_screen::first_row);
  if(!overview() || !shown_page)return "overlay";
  const auto index=widgets[0].index;
  return "page/"+std::to_string(*shown_page)+"/"+(index<model.count?model.tiles[index].entity:std::string());
}
inline void press() {
  acceleration.reset();press_target=target();
  button.press(esphome::millis(),activity && activity() && allowed_input());
}
inline void release() {
  const auto kind=button.release(esphome::millis());
  if(kind==screen_input::RotaryButton::Release::none)return;
  if(press_target!=target()){reset();return;}
  if(!activity || !activity() || !allowed_input()){reset();return;}
  cancel_preview();acceleration.reset();context.clear();
  if(settings_screen::visible()) {
    if(kind==screen_input::RotaryButton::Release::hold)settings_screen::back_event(nullptr);
    else settings_screen::knob_click();
    return;
  }
  if(!overview())return;
  if(kind==screen_input::RotaryButton::Release::hold) {
    if(!settings_screen::may_open || settings_screen::may_open())settings_screen::open();
    return;
  }
#ifdef SD_MOD_ROUND_FRAME
  if(auto *w=dial_control())round_cards::primary_action(*w);
#endif
}
}  // namespace rotary
#endif
