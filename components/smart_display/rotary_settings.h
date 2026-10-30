// Included inside settings_screen. All edits use the existing table and setters.
#if defined(SD_INPUT_ROTARY) && defined(SMART_DISPLAY_ROTARY_SETTINGS_SITE) && !defined(SMART_DISPLAY_ROTARY_SETTINGS)
#define SMART_DISPLAY_ROTARY_SETTINGS
inline const Row *knob_selected() {
  if(!root || !drawn_count)return nullptr;
  bool shown=false;
  for(uint8_t i=0;i<drawn_count;++i)shown |= drawn[i].row==knob_row;
  if(knob_page!=current_page || !shown){knob_page=current_page;knob_row=drawn[0].row;knob_editing=false;}
  const auto &page=page_at(current_page);
  return knob_row>=0 && knob_row<page.count?&page.rows[knob_row]:nullptr;
}
inline bool knob_numeric(const Row &row) {
  return row.kind==Kind::number || row.kind==Kind::duration || row.kind==Kind::moment;
}
inline void knob_turn(int steps,bool browse) {
  const auto *selected=knob_selected();if(!selected)return;
  if(knob_editing && !browse && knob_numeric(*selected)) {
    if(!readable(*selected) || !writable(*selected) || !live_row(*selected))return;
    int32_t value=get(*selected),next=value;
    for(int i=0;i<std::abs(steps);++i)next=stepped(*selected,next,steps>0?1:-1,false);
    if(next!=value)put(*selected,next);
    refresh();return;
  }
  knob_editing=false;
  const auto &page=page_at(current_page);
  int next=knob_row;
  do {next+=steps>0?1:-1;} while(next>=0 && next<page.count && !visible_row(page.rows[next]));
  if(next<0 || next>=page.count){refresh();return;}
  knob_row=next;
  int position=0;
  for(int i=0;i<next;++i)position+=visible_row(page.rows[i]);
  first_row=(position/std::max<int>(1,rows_per_page))*std::max<int>(1,rows_per_page);
  forget_confirm();draw();
}
inline void knob_click() {
  const auto *row=knob_selected();if(!row || !live_row(*row))return;
  if(knob_numeric(*row)) {
    if(readable(*row) && writable(*row)){knob_editing=!knob_editing;refresh();}
  }else activate_row(knob_row);
}
#endif
