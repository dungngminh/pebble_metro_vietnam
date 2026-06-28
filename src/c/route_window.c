#include "route_window.h"
#include "data.h"
#include "anim.h"

#define HDR_H 26
#define ROW_H 34
#define SWIPE_MIN 30

static Window *s_window;
static Layer *s_list;
static Layer *s_header;
static uint8_t s_line;
static int s_dep_index;   // chosen departure, or -1 for "next train"
static int s_scroll;
static uint8_t s_focus;
static int s_marq_off;
static AppTimer *s_marq_timer;

static int list_height(void) { return layer_get_bounds(s_list).size.h; }

// Full line, ordered in the current travel direction.
static int row_count(void) { return g_stations[s_line].count; }
static uint8_t row_station(int i) {
  uint8_t n = g_stations[s_line].count;
  return data_direction(s_line) == 0 ? (uint8_t)i : (uint8_t)(n - 1 - i);
}

static int max_scroll(void) {
  int total = row_count() * ROW_H;
  int h = list_height();
  return total > h ? total - h : 0;
}
static void clamp_scroll(void) {
  if (s_scroll < 0) s_scroll = 0;
  int m = max_scroll();
  if (s_scroll > m) s_scroll = m;
}
static void reveal_focus(void) {
  int top = s_focus * ROW_H, bot = top + ROW_H;
  if (top - s_scroll < 0) s_scroll = top;
  else if (bot - s_scroll > list_height()) s_scroll = bot - list_height();
  clamp_scroll();
}

static void header_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, g_app_data.lines[s_line].color);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  // Wide: full wording. Narrow 144px: short code + compact wording.
  bool narrow = b.size.w < 180;
  const char *sc = g_app_data.lines[s_line].shortname;
  char title[28];
  if (s_dep_index >= 0 && s_dep_index < g_app_data.lines[s_line].dep_count) {
    char clk[8];
    time_t t = (time_t)data_arrival(s_line, s_dep_index);
    strftime(clk, sizeof(clk), "%H:%M", localtime(&t));
    if (narrow) snprintf(title, sizeof(title), "%s Departs %s", sc, clk);
    else        snprintf(title, sizeof(title), "Departs %s", clk);
  } else {
    snprintf(title, sizeof(title), narrow ? "%s full line" : "Full line - next train",
             sc);
  }
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(6, 2, b.size.w - 12 - UI_NOW_W, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  ui_draw_now(ctx, b); // live-clock chip, top-right (distinct from the "Departs" time)
}

static void list_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  LineData *line = &g_app_data.lines[s_line];
  int len = row_count();
  time_t now = time(NULL);
  int idx = s_dep_index >= 0 ? s_dep_index
                             : data_next_index(s_line, now); // chosen or approaching
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  for (int i = 0; i < len; i++) {
    int y = i * ROW_H - s_scroll;
    if (y + ROW_H <= 0 || y >= b.size.h) continue;
    uint8_t st = row_station(i);

    if (i == s_focus) {
      graphics_context_set_fill_color(ctx, data_light(s_line));
      graphics_fill_rect(ctx, GRect(0, y, b.size.w, ROW_H), 0, GCornerNone);
    }

    // Highlight the trip's A and B stops; smaller dots for the rest.
    bool endpoint = (st == line->sel_station || st == line->sel_dest);
    if (endpoint) {
      graphics_context_set_fill_color(ctx, line->color);
      graphics_fill_circle(ctx, GPoint(8, y + ROW_H / 2), 4);
    } else {
      graphics_context_set_stroke_color(ctx, GColorDarkGray);
      graphics_draw_circle(ctx, GPoint(8, y + ROW_H / 2), 2);
    }

    // Time-until on the right (hours/minutes); clock time below.
    char eta[12] = "--";
    char hm[12] = "";
    if (idx >= 0) {
      int32_t arr = data_arrival_at(s_line, idx, st);
      int32_t secs = arr - (int32_t)now;
      if (secs < 0) secs = 0;
      int mins = secs / 60;
      if (secs < 60) snprintf(eta, sizeof(eta), "<1m");
      else if (mins < 60) snprintf(eta, sizeof(eta), "%dm", mins);
      else snprintf(eta, sizeof(eta), "%dh%02dm", mins / 60, mins % 60);
      char clk[8];
      time_t t = (time_t)arr;
      strftime(clk, sizeof(clk), "%H:%M", localtime(&t));
      snprintf(hm, sizeof(hm), "at %s", clk);
    }
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, eta, font, GRect(b.size.w - 64, y + 1, 58, 20),
                       GTextOverflowModeFill, GTextAlignmentRight, NULL);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, hm, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(b.size.w - 64, y + 18, 58, 14),
                       GTextOverflowModeFill, GTextAlignmentRight, NULL);

    // Station name, marquee when focused.
    GRect tr = GRect(18, y + 7, b.size.w - 86, 22);
    if (i == s_focus) {
      marquee_draw(ctx, g_stations[s_line].names[st], font, tr, GTextAlignmentLeft, s_marq_off);
    } else {
      graphics_draw_text(ctx, g_stations[s_line].names[st], font, tr,
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }
}

static void up_click(ClickRecognizerRef rec, void *ctx) {
  if (s_focus > 0) { s_focus--; reveal_focus(); s_marq_off = 0; layer_mark_dirty(s_list); }
}
static void down_click(ClickRecognizerRef rec, void *ctx) {
  if (s_focus + 1 < row_count()) {
    s_focus++; reveal_focus(); s_marq_off = 0; layer_mark_dirty(s_list);
  }
}
static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

#if PBL_API_EXISTS(touch_service_subscribe)
static int16_t s_tx0, s_ty0;
static int s_scroll0;
static bool s_moved;
static void route_touch(const TouchEvent *e, void *ctx) {
  if (s_window != window_stack_get_top_window()) return;
  switch (e->type) {
    case TouchEvent_Touchdown:
      s_tx0 = e->x; s_ty0 = e->y; s_scroll0 = s_scroll; s_moved = false;
      break;
    case TouchEvent_PositionUpdate: {
      int dy = e->y - s_ty0, ady = dy < 0 ? -dy : dy;
      if (ady >= 6) s_moved = true;
      s_scroll = s_scroll0 - dy;
      clamp_scroll();
      layer_mark_dirty(s_list);
      break;
    }
    case TouchEvent_Liftoff: {
      int dx = e->x - s_tx0, dy = e->y - s_ty0;
      int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
      if (!s_moved) break;
      if (adx >= SWIPE_MIN && adx > ady) {
        window_stack_pop(true);                       // horizontal swipe -> back
      } else if (dy > 0 && ady >= SWIPE_MIN && ady > adx && s_scroll0 == 0) {
        window_stack_pop(true);                       // swipe down at top -> back to detail
      }
      break;
    }
    default: break;
  }
}
#endif

static void marq_tick(void *ctx) {
  s_marq_off += 3;
  if (s_list) layer_mark_dirty(s_list);
  if (s_header) layer_mark_dirty(s_header); // keep the live-clock chip current
  s_marq_timer = app_timer_register(70, marq_tick, NULL);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_list = layer_create(GRect(0, HDR_H, b.size.w, b.size.h - HDR_H));
  layer_set_update_proc(s_list, list_update);
  layer_add_child(root, s_list);

  s_header = layer_create(GRect(0, 0, b.size.w, HDR_H));
  layer_set_update_proc(s_header, header_update);
  layer_add_child(root, s_header);

  s_focus = 0;
  s_scroll = 0;
  s_marq_off = 0;
  s_marq_timer = app_timer_register(70, marq_tick, NULL);
  window_set_click_config_provider(window, click_config);
}

static void window_appear(Window *window) {
#if PBL_API_EXISTS(touch_service_subscribe)
  touch_service_subscribe(route_touch, NULL);
#endif
}
static void window_disappear(Window *window) {
#if PBL_API_EXISTS(touch_service_subscribe)
  touch_service_unsubscribe();
#endif
}

static void window_unload(Window *window) {
  if (s_marq_timer) { app_timer_cancel(s_marq_timer); s_marq_timer = NULL; }
  layer_destroy(s_header);
  s_header = NULL;
  layer_destroy(s_list);
  s_list = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void route_window_push_dep(uint8_t line_index, int dep_index) {
  if (line_index >= g_app_data.line_count) return;
  s_line = line_index;
  s_dep_index = dep_index;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void route_window_push(uint8_t line_index) {
  route_window_push_dep(line_index, -1); // -1 = next train
}
