#pragma once
#include "surface_geometry.h"
namespace round_settings_layout {
struct Plan { ui::SurfaceBounds back, title, previous, next, body, name, value, minus, plus, footer; };
inline Plan make(int width, int height, int line, int value_h, int touch, int inset) {
  Plan out;
  // Keep full touch targets, but reserve the central chord for the setting.
  // Navigation paints only a glyph inside these transparent targets.
  const int key = std::min(touch, std::min(width,height)*19/100);
  const int centre=width/2, header_y=height*22/100;
  out.back={width*20/100,header_y-key/2,key,key};
  out.title={width*30/100,header_y-line/2,width*40/100,line};
  out.previous={inset*3,(height-key)/2,key,key};
  out.next={width-inset*3-key,(height-key)/2,key,key};
  const ui::SurfaceGeometry glass{width,height,ui::SurfaceShape::round};
  const int left=key+inset*4;
  out.body={left,height*33/100,std::max(0,width-left*2),height*87/100-height*33/100};
  out.name={out.body.x,out.body.y,out.body.width,line*2};
  out.value={out.body.x,height*53/100,out.body.width,std::max(value_h,line*3)};
  out.minus={centre-key-inset*2,height*68/100,key,key};
  out.plus={centre+inset*2,height*68/100,key,key};
  out.footer=glass.band_bounds(height-inset*3-line,height-inset*3,inset);
  return out;
}
}  // namespace round_settings_layout
