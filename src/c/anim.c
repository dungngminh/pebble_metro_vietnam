#include "anim.h"

void anim_slide(Layer *layer, GRect from, GRect to, uint32_t duration_ms, uint32_t delay_ms) {
  layer_set_frame(layer, from);
  PropertyAnimation *prop = property_animation_create_layer_frame(layer, &from, &to);
  Animation *anim = property_animation_get_animation(prop);
  animation_set_duration(anim, duration_ms);
  animation_set_delay(anim, delay_ms);
  animation_set_curve(anim, AnimationCurveEaseOut);
  animation_schedule(anim);
}

bool marquee_draw(GContext *ctx, const char *text, GFont font, GRect rect,
                  GTextAlignment align, int offset) {
  GSize sz = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 2000, rect.size.h), GTextOverflowModeWordWrap,
      GTextAlignmentLeft);
  if (sz.w <= rect.size.w) {
    graphics_draw_text(ctx, text, font, rect, GTextOverflowModeTrailingEllipsis, align, NULL);
    return false;
  }
  const int gap = 28;
  int span = sz.w + gap;
  int off = offset % span;
  GRect r = GRect(rect.origin.x - off, rect.origin.y, sz.w + 8, rect.size.h);
  graphics_draw_text(ctx, text, font, r, GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  r.origin.x += span; // trailing copy so the loop is seamless
  graphics_draw_text(ctx, text, font, r, GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  return true;
}
