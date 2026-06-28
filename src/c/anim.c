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

void ui_draw_now(GContext *ctx, GRect hdr) {
  char hm[8];
  time_t t = time(NULL);
  strftime(hm, sizeof(hm), "%H:%M", localtime(&t));

  int ph = 18;
  int x = hdr.size.w - UI_NOW_W + 2;
  int y = (hdr.size.h - ph) / 2;
  if (y < 1) y = 1;
  GRect chip = GRect(x, y, UI_NOW_W - 5, ph);
  // White outlined chip = "this is the live clock", distinct from plain times.
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_round_rect(ctx, chip, 4);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, hm, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(chip.origin.x, chip.origin.y - 1, chip.size.w, chip.size.h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
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
