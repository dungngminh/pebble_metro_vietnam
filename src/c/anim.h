#pragma once
#include <pebble.h>

// Slide a layer from `from` to `to` using an ease-out curve. Takes ownership of
// scheduling; the animation auto-destroys on completion.
void anim_slide(Layer *layer, GRect from, GRect to, uint32_t duration_ms, uint32_t delay_ms);

// Draw `text` in `rect`. If it fits, draw normally with `align`. If it is wider
// than `rect`, scroll it horizontally (looping) by `offset` pixels. Relies on the
// caller's layer/cell bounds to clip. Returns true when it is scrolling.
bool marquee_draw(GContext *ctx, const char *text, GFont font, GRect rect,
                  GTextAlignment align, int offset);

// Width reserved on the right of a header for the live-clock chip. Lay out the
// header title with `bounds.size.w - UI_NOW_W` so it never collides with the clock.
#define UI_NOW_W 50

// Draw the current time (HH:MM) as a white outlined "chip" at the top-right of a
// header bar. The outline frames it so it reads as the live clock and is NOT
// confused with the schedule/title times printed elsewhere. `hdr` = header bounds.
void ui_draw_now(GContext *ctx, GRect hdr);
