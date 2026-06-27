#include "detail_window.h"
#include "data.h"
#include "anim.h"
#include "station_menu.h"
#include "route_window.h"

// Provided by the main module: ask the phone to recompute & resend departures.
extern void app_request_refresh(void);

static Window *s_window;
static Layer *s_canvas;
static uint8_t s_line_index;
static bool s_pulse_on;
static GBitmap *s_train;
static int32_t s_last_alert_arr;  // arrival epoch we last buzzed for

// Intro sweep progress, 0..1000 (drives ring fill + marker on entry / change).
static Animation *s_intro_anim;
static int s_intro;

// Marquee scroll for long header / sub-header text.
static AppTimer *s_marq_timer;
static int s_marq_off;

static void marq_tick(void *ctx) {
  s_marq_off += 3;
  if (s_canvas) layer_mark_dirty(s_canvas);
  s_marq_timer = app_timer_register(70, marq_tick, NULL);
}

// ---- intro animation ---------------------------------------------------------

static void intro_update(Animation *anim, const AnimationProgress progress) {
  s_intro = (1000 * progress) / ANIMATION_NORMALIZED_MAX;
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static const AnimationImplementation s_intro_impl = { .update = intro_update };

static void start_intro(void) {
  if (s_intro_anim) {
    animation_unschedule(s_intro_anim);
    animation_destroy(s_intro_anim);
  }
  s_intro = 0;
  s_intro_anim = animation_create();
  animation_set_implementation(s_intro_anim, &s_intro_impl);
  animation_set_duration(s_intro_anim, 650);
  animation_set_curve(s_intro_anim, AnimationCurveEaseOut);
  animation_schedule(s_intro_anim);
}

// ---- drawing helpers ---------------------------------------------------------

static void format_clock(int32_t epoch, char *buf, size_t len) {
  time_t t = (time_t)epoch;
  strftime(buf, len, "%H:%M", localtime(&t));
}

static void draw_header(GContext *ctx, GRect bounds, const LineData *line) {
  graphics_context_set_fill_color(ctx, line->color);
  graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, 30), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  marquee_draw(ctx, line->name, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
               GRect(4, 3, bounds.size.w - 8, 24), GTextAlignmentCenter, s_marq_off);

  // Sub-header: selected station and direction.
  char sub[52];
  LineStations *st = &g_stations[s_line_index];
  const char *station = (st->count > 0 && line->sel_station < st->count)
                            ? st->names[line->sel_station] : "origin";
  snprintf(sub, sizeof(sub), "%s  >> %s", station, data_heading(s_line_index));
  graphics_context_set_text_color(ctx, GColorLightGray);
  marquee_draw(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
               GRect(4, 31, bounds.size.w - 8, 18), GTextAlignmentCenter, s_marq_off);
}

static void draw_track_and_count(GContext *ctx, GRect bounds, const LineData *line, time_t now) {
  int idx = data_next_index(s_line_index, now);

  char big[12];
  char label[20];
  GColor num_color = GColorWhite;
  bool moving = false;
  int frac1000 = 0;             // progress along the A->B track, 0..1000
  int32_t a_time = 0, b_time = 0;
  bool have_times = false;

  if (idx < 0) {
    snprintf(big, sizeof(big), "--:--");
    snprintf(label, sizeof(label), "no data");
  } else {
    int32_t arr = data_arrival(s_line_index, idx);
    int32_t secs = arr - (int32_t)now;
    if (secs < 0) secs = 0;

    int32_t headway = 600;
    if (idx + 1 < line->dep_count) headway = line->deps[idx + 1] - line->deps[idx];
    else if (idx > 0) headway = line->deps[idx] - line->deps[idx - 1];
    if (headway <= 0) headway = 600;

    bool at_station = data_offset(s_line_index) > 0;
    a_time = arr;
    b_time = data_dest_arrival(s_line_index, idx);
    have_times = true;

    if (line->closed) {
      format_clock(arr, big, sizeof(big));
      snprintf(label, sizeof(label), "first train");
    } else if (secs < 3600) {
      snprintf(big, sizeof(big), "%d:%02d", (int)(secs / 60), (int)(secs % 60));
      snprintf(label, sizeof(label), at_station ? "until arrival" : "until next");
      int32_t elapsed = headway - secs;
      if (elapsed < 0) elapsed = 0;
      frac1000 = (int)((1000 * elapsed) / headway);
      frac1000 = frac1000 * s_intro / 1000; // intro sweep-in
      moving = true;
      if (secs < 60 && s_pulse_on) num_color = GColorRed;
    } else {
      format_clock(arr, big, sizeof(big));
      snprintf(label, sizeof(label), at_station ? "arrival" : "next train");
    }
  }

  // Countdown / clock, with its small caption above it.
  int16_t cx = bounds.size.w / 2;
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, 52, bounds.size.w, 18), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, num_color);
  graphics_draw_text(ctx, big, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS),
                     GRect(0, 64, bounds.size.w, 46), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);

  // Horizontal A -> B track.
  int16_t m = 18;
  int16_t x0 = m, x1 = bounds.size.w - m;
  int16_t ty = bounds.size.h - 76;
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(x0, ty - 1, x1 - x0, 3), 0, GCornerNone);
  int16_t tx = x0 + (int16_t)(((int32_t)(x1 - x0) * frac1000) / 1000);
  graphics_context_set_fill_color(ctx, line->color);
  graphics_fill_rect(ctx, GRect(x0, ty - 1, tx - x0, 3), 0, GCornerNone);
  graphics_fill_circle(ctx, GPoint(x0, ty), 4);
  graphics_fill_circle(ctx, GPoint(x1, ty), 4);

  // Train running along the track (sits on the rail at the progress head).
  if (moving && s_train) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_train, GRect(tx - 12, ty - 20, 25, 25));
  }

  // A (departure station) on the left, B (heading terminus) on the right.
  LineStations *st = &g_stations[s_line_index];
  const char *aname = (st->count > 0 && line->sel_station < st->count)
                          ? st->names[line->sel_station] : "origin";
  const char *bname = data_heading(s_line_index);
  int16_t half = bounds.size.w / 2;
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, aname, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(x0 - 4, ty + 5, half - x0 + 2, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, bname, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(half - 2, ty + 5, x1 - half + 6, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);
  if (have_times) {
    char at[8], bt[8];
    format_clock(a_time, at, sizeof(at));
    format_clock(b_time, bt, sizeof(bt));
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, at, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(x0 - 4, ty + 22, half - x0 + 2, 26), GTextOverflowModeFill,
                       GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, bt, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(half - 2, ty + 22, x1 - half + 6, 26), GTextOverflowModeFill,
                       GTextAlignmentRight, NULL);
  }
}

static void draw_footer(GContext *ctx, GRect bounds, const LineData *line, time_t now) {
  graphics_context_set_text_color(ctx, GColorLightGray);
  const char *hint = "SEL: pick   UP: line   DOWN: rev";
#if PBL_API_EXISTS(touch_service_subscribe)
  if (touch_service_is_enabled()) hint = "2 tap: pick   swipe up: line   down: rev";
#endif
  graphics_draw_text(ctx, hint,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, bounds.size.h - 20, bounds.size.w, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  if (s_line_index >= g_app_data.line_count) return;
  const LineData *line = &g_app_data.lines[s_line_index];
  time_t now = time(NULL);
  draw_header(ctx, bounds, line);
  draw_track_and_count(ctx, bounds, line, now);
  draw_footer(ctx, bounds, line, now);
}

// ---- ticks & clicks ----------------------------------------------------------

#define ALERT_SECONDS 30  // buzz this long before the train arrives

// Fires once per departure when it enters the final alert window.
static void check_arrival_alert(time_t now) {
  if (s_line_index >= g_app_data.line_count) return;
  if (g_app_data.lines[s_line_index].closed) return;
  int idx = data_next_index(s_line_index, now);
  if (idx < 0) return;
  int32_t arr = data_arrival(s_line_index, idx);
  int32_t secs = arr - (int32_t)now;
  if (secs >= 0 && secs <= ALERT_SECONDS && arr != s_last_alert_arr) {
    s_last_alert_arr = arr;
    vibes_double_pulse();
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_pulse_on = !s_pulse_on;
  check_arrival_alert(time(NULL));
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void select_click(ClickRecognizerRef rec, void *ctx) {
  station_menu_push(s_line_index);
}

static void reverse_trip(void) {
  if (s_line_index >= g_app_data.line_count) return;
  data_swap(s_line_index); // reverse the trip (A <-> B)
  data_save();
  s_last_alert_arr = 0;    // arrival changed; allow a fresh alert
  start_intro();
}

static void up_click(ClickRecognizerRef rec, void *ctx) {
  route_window_push(s_line_index); // full line stop list
}

static void down_click(ClickRecognizerRef rec, void *ctx) {
  reverse_trip(); // reverse A <-> B
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

// ---- touch (Pebble Time 2 touchscreen) ---------------------------------------

#if PBL_API_EXISTS(touch_service_subscribe)
#define SWIPE_MIN 30          // px of travel that counts as a swipe
#define DOUBLE_TAP_MS 300     // window for a second tap

static int16_t s_tx0, s_ty0;  // touchdown origin
static AppTimer *s_tap_timer; // pending single tap (awaiting a possible second)

static void tap_timeout(void *ctx) {
  s_tap_timer = NULL; // single tap expired with no second tap -> no action
}

static void touch_handler(const TouchEvent *event, void *context) {
  if (s_window != window_stack_get_top_window()) return; // ignore when not on top
  if (s_line_index >= g_app_data.line_count) return;

  if (event->type == TouchEvent_Touchdown) {
    s_tx0 = event->x;
    s_ty0 = event->y;
    return;
  }
  if (event->type != TouchEvent_Liftoff) return;

  int16_t dx = event->x - s_tx0, dy = event->y - s_ty0;
  int16_t adx = dx < 0 ? -dx : dx;
  int16_t ady = dy < 0 ? -dy : dy;

  if (ady >= SWIPE_MIN && ady > adx) {
    if (dy < 0) route_window_push(s_line_index); // swipe up -> full line list
    else reverse_trip();                         // swipe down -> reverse A <-> B
  } else if (adx >= SWIPE_MIN && adx > ady) {
    window_stack_pop(true);     // horizontal swipe -> back
  } else {                      // tap: detect a double tap
    if (s_tap_timer) {
      app_timer_cancel(s_tap_timer);
      s_tap_timer = NULL;
      station_menu_push(s_line_index); // double tap -> choose A > B
    } else {
      s_tap_timer = app_timer_register(DOUBLE_TAP_MS, tap_timeout, NULL);
    }
  }
}
#endif

// ---- window lifecycle --------------------------------------------------------

static void window_load(Window *window) {
  window_set_background_color(window, GColorBlack);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_train = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN);
  s_last_alert_arr = 0;

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  GRect from = GRect(0, bounds.size.h, bounds.size.w, bounds.size.h);
  anim_slide(s_canvas, from, bounds, 300, 0);
  start_intro();

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  s_marq_off = 0;
  s_marq_timer = app_timer_register(70, marq_tick, NULL);
}

// Own the (single) touch handler only while this window is on top. Re-subscribing
// on appear restores it after the station picker closes.
static void window_appear(Window *window) {
#if PBL_API_EXISTS(touch_service_subscribe)
  touch_service_subscribe(touch_handler, NULL); // Pebble Time 2 touchscreen
#endif
}

static void window_disappear(Window *window) {
#if PBL_API_EXISTS(touch_service_subscribe)
  if (s_tap_timer) { app_timer_cancel(s_tap_timer); s_tap_timer = NULL; }
  touch_service_unsubscribe();
#endif
}

static void window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  if (s_marq_timer) { app_timer_cancel(s_marq_timer); s_marq_timer = NULL; }
  if (s_intro_anim) {
    animation_unschedule(s_intro_anim);
    animation_destroy(s_intro_anim);
    s_intro_anim = NULL;
  }
  if (s_train) { gbitmap_destroy(s_train); s_train = NULL; }
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void detail_window_push(uint8_t line_index) {
  s_line_index = line_index;
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void detail_window_refresh(void) {
  if (s_canvas) {
    s_last_alert_arr = 0; // selection/data changed; re-arm the arrival buzz
    start_intro();
    layer_mark_dirty(s_canvas);
  }
}

bool detail_window_is_open(void) {
  return s_window != NULL;
}
