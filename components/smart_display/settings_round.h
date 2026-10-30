// Included inside settings_screen after its shared table, setters and callbacks.
#ifdef SD_MOD_ROUND_FRAME
#if defined(SMART_DISPLAY_ROUND_SETTINGS_SITE) && !defined(SMART_DISPLAY_ROUND_SETTINGS)
#define SMART_DISPLAY_ROUND_SETTINGS
namespace round_view {
inline void page_event(lv_event_t *event) { pager_step(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)))); }
inline lv_obj_t *nav_key(const ui::SurfaceBounds &box,const char *glyph,lv_event_cb_t callback,int data=0,bool repeat=false) {
  auto *button=plain(root,box.x,box.y,box.width,box.height);
  lv_obj_add_flag(button,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(button,LV_RADIUS_CIRCLE,0);
  lv_obj_set_style_bg_opa(button,LV_OPA_TRANSP,0);
  lv_obj_set_style_bg_opa(button,LV_OPA_20,LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(button,theme::color(theme::SETTING_KEY),0);
  lv_obj_set_style_bg_color(button,theme::color(theme::SETTING_KEY_PRESSED),LV_STATE_PRESSED);
  lv_obj_set_style_opa(button,LV_OPA_40,LV_STATE_DISABLED);
  auto *label=text(button,glyph,icon_font?icon_font:row_font,theme::INK);
  lv_obj_set_width(label,LV_SIZE_CONTENT);lv_obj_center(label);
  // Reuse the compiled icon font; reduce only the painted navigation glyph.
  lv_obj_set_style_transform_pivot_x(label,lv_pct(50),0);
  lv_obj_set_style_transform_pivot_y(label,lv_pct(50),0);
  lv_obj_set_style_transform_scale_x(label,180,0);
  lv_obj_set_style_transform_scale_y(label,180,0);
  lv_obj_add_event_cb(button,callback,LV_EVENT_SHORT_CLICKED,reinterpret_cast<void *>(static_cast<intptr_t>(data)));
  if(repeat)lv_obj_add_event_cb(button,callback,LV_EVENT_LONG_PRESSED_REPEAT,reinterpret_cast<void *>(static_cast<intptr_t>(data)));
  return button;
}
inline void draw() {
  lv_obj_clean(root);drawn_count=0;
  const auto &page=page_at(current_page);
  std::array<uint8_t,12> shown{};uint8_t count=0;
  for(uint8_t i=0;i<page.count && count<shown.size();++i)if(visible_row(page.rows[i]))shown[count++]=i;
  rows_per_page=1;
  first_row=count?std::min<int>(first_row,count-1):0;
  auto *display=lv_display_get_default();
  const auto *words=round_font?round_font:row_font;
  const auto *digits=title_font?title_font:words;
  const int line=lv_font_get_line_height(words),value_h=lv_font_get_line_height(digits);
  const auto p=round_settings_layout::make(lv_display_get_horizontal_resolution(display),
      lv_display_get_vertical_resolution(display),line,value_h,ui::touch_min(),ui::px(2));
  nav_key(p.back,"\U000F0141",back_event);
  auto *heading=text(root,page_title(page),words,theme::INK,LV_TEXT_ALIGN_CENTER);
  lv_obj_set_pos(heading,p.title.x,p.title.y);lv_obj_set_width(heading,p.title.width);
  if(!count)return;
  const uint8_t index=shown[first_row];const auto &row=page.rows[index];
  auto &d=drawn[0];d=Drawn{};d.row=index;drawn_count=1;
  const bool asking=row.kind==Kind::action && confirm_row==index;
  const bool tappable=row.kind==Kind::page || row.kind==Kind::toggle || row.kind==Kind::choice || row.kind==Kind::action;
  d.lit=asking || busy_row(row);
  d.card=plain(root,p.body.x,p.body.y,p.body.width,p.body.height);
  if(tappable){
    lv_obj_add_flag(d.card,LV_OBJ_FLAG_CLICKABLE);
    // The content is a touch surface, not a tall capsule behind the text.
    lv_obj_set_style_bg_opa(d.card,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_opa(d.card,LV_OPA_20,LV_STATE_PRESSED);
    lv_obj_set_style_radius(d.card,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(d.card,theme::color(d.lit?theme::ACCENT:theme::CARD),0);
    lv_obj_set_style_bg_color(d.card,theme::color(d.lit?theme::ACCENT_PRESSED:theme::CARD_PRESSED),LV_STATE_PRESSED);
    lv_obj_add_event_cb(d.card,row_event,LV_EVENT_SHORT_CLICKED,reinterpret_cast<void *>(static_cast<intptr_t>(index)));
  }
  auto label_at=[&](const std::string &value,const lv_font_t *font,theme::Role color,ui::SurfaceBounds box){
    auto *label=text(d.card,value,font,color,LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(label,box.x-p.body.x,box.y-p.body.y);
    lv_obj_set_size(label,box.width,box.height);
    return label;
  };
  auto name=p.name;
  if(row.kind==Kind::page || row.kind==Kind::action){
    if(row.icon && *row.icon){
      const int h=lv_font_get_line_height(icon_font?icon_font:words);
      label_at(row.icon,icon_font?icon_font:words,theme::ROW_ICON,{name.x,name.y,name.width,h});
      name.y+=h+ui::px(4);
    }else name.y+=line;
  }
  d.label=label_at(asking?confirm_text(row):label_text(row),words,d.lit?theme::ACCENT:theme::INK,name);
  if(row.kind==Kind::number || row.kind==Kind::duration || row.kind==Kind::moment){
    auto value=p.value;value.height=value_h;
    d.value=label_at(value_text(row),digits,theme::INK,value);
    d.minus=nav_key(p.minus,"\U000F0374",step_event,index*2,true);
    d.plus=nav_key(p.plus,"\U000F0415",step_event,index*2+1,true);
  }else if(row.kind==Kind::toggle){
    auto value=p.value;value.height=value_h;d.value=label_at(value_text(row),digits,theme::INK,value);
    const int h=std::min(p.minus.height,ui::px(16));
    const int x=(p.minus.x+p.plus.x+p.plus.width-p.minus.width)/2;
    d.knob=pill(d.card,x-p.body.x,p.minus.y+(p.minus.height-h)/2-p.body.y,p.minus.width,h,get(row)!=0);
  }else if(row.kind==Kind::choice || row.kind==Kind::info || (row.kind==Kind::action && row.own && row.own->text)){
    auto value=p.value;
    if(row.kind==Kind::action){value.y=name.y+name.height+ui::px(2);value.height=std::max(1,p.body.y+p.body.height-value.y);}
    d.value=label_at(value_text(row),words,d.lit?theme::ACCENT:theme::MUTED,value);
  }
  if(count>1){
    auto *previous=nav_key(p.previous,"\U000F0141",page_event,-1);
    auto *next=nav_key(p.next,"\U000F0142",page_event,1);
    page_bar::enable(previous,first_row>0);page_bar::enable(next,first_row+1<count);
    auto *number=text(root,std::to_string(first_row+1)+" / "+std::to_string(count),words,theme::MUTED,LV_TEXT_ALIGN_CENTER);
    lv_obj_set_pos(number,p.footer.x,p.footer.y);lv_obj_set_width(number,p.footer.width);
  }
  refresh();
}
}  // namespace round_view
#endif
#endif
