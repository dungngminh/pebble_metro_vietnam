#include "schedule_window.h"
#include "data.h"
#include "route_window.h"
#include "anim.h"

#define HDR_H 30
#define ROW_H 40
#define SWIPE_MIN 30
#define DRAG_MIN 6

static Window *s_window;
static Layer *s_list;
static Layer *s_header;
static uint8_t s_line;
static int s_scroll;
static uint8_t s_focus;
static AppTimer *s_tick;   // 1s refresh so the countdowns tick

static int list_height(void) { return layer_get_bounds(s_list).size.h; }

// First upcoming departure index (the one still approaching A), or -1 if none.
static int base_index(void) { return data_next_index(s_line, time(NULL)); }

static int row_count(void) {
  int base = base_index();
  if (base < 0) return 0;
  return g_app_data.lines[s_line].dep_count - base;
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

static void fmt_clock(int32_t epoch, char *buf, size_t len) {
  time_t t = (time_t)epoch;
  strftime(buf, len, "%H:%M", localtime(&t));
}

static void header_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  LineData *line = &g_app_data.lines[s_line];
  graphics_context_set_fill_color(ctx, line->color);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  ui_draw_now(ctx, b); // live-clock chip, top-right
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "Departures", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(6, -1, b.size.w - 12 - UI_NOW_W, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  // Sub-line: the chosen trip A > B.
  char sub[52];
  LineStations *st = &g_stations[s_line];
  const char *a = (st->count > 0 && line->sel_station < st->count)
                      ? st->names[line->sel_station] : "origin";
  snprintf(sub, sizeof(sub), "%s > %s", a, data_heading(s_line));
  graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(6, 15, b.size.w - 12, 15), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void list_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  LineData *line = &g_app_data.lines[s_line];
  int base = base_index();
  time_t now = time(NULL);

  if (base < 0) {
    // No more trains today (or closed): show first train if we know it.
    char msg[28] = "No more departures";
    if (line->closed && line->first_train > 0) {
      char clk[8];
      fmt_clock(line->first_train, clk, sizeof(clk));
      snprintf(msg, sizeof(msg), "First train %s", clk);
    }
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, msg, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(8, b.size.h / 2 - 16, b.size.w - 16, 40),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    return;
  }

  int len = row_count();
  for (int i = 0; i < len; i++) {
    int y = i * ROW_H - s_scroll;
    if (y + ROW_H <= 0 || y >= b.size.h) continue;
    int dep = base + i;

    if (i == s_focus) {
      graphics_context_set_fill_color(ctx, data_light(s_line));
      graphics_fill_rect(ctx, GRect(0, y, b.size.w, ROW_H), 0, GCornerNone);
    }

    char dt[8], at[8];
    fmt_clock(data_arrival(s_line, dep), dt, sizeof(dt));
    fmt_clock(data_dest_arrival(s_line, dep), at, sizeof(at));

    // Depart time (prominent) + arrival time below.
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, dt, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(8, y + 1, 80, 26), GTextOverflowModeFill,
                       GTextAlignmentLeft, NULL);
    char arr[16];
    snprintf(arr, sizeof(arr), "> %s", at);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, arr, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(10, y + 24, 80, 14), GTextOverflowModeFill,
                       GTextAlignmentLeft, NULL);

    // Countdown until this train departs A, on the right.
    int32_t secs = data_arrival(s_line, dep) - (int32_t)now;
    if (secs < 0) secs = 0;
    int mins = secs / 60;
    char eta[12];
    if (secs < 60) snprintf(eta, sizeof(eta), "<1m");
    else if (mins < 60) snprintf(eta, sizeof(eta), "%dm", mins);
    else snprintf(eta, sizeof(eta), "%dh%02dm", mins / 60, mins % 60);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, eta, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(b.size.w - 64, y + 9, 58, 22), GTextOverflowModeFill,
                       GTextAlignmentRight, NULL);
  }
}

// Open the full-line stop list for the departure at list row `row`.
static void open_dep(int row) {
  int base = base_index();
  if (base < 0) return;
  if (row < 0 || row >= row_count()) return;
  route_window_push_dep(s_line, base + row);
}

static void up_click(ClickRecognizerRef rec, void *ctx) {
  if (s_focus > 0) { s_focus--; reveal_focus(); layer_mark_dirty(s_list); }
}
static void down_click(ClickRecognizerRef rec, void *ctx) {
  if (s_focus + 1 < row_count()) { s_focus++; reveal_focus(); layer_mark_dirty(s_list); }
}
static void select_click(ClickRecognizerRef rec, void *ctx) {
  open_dep(s_focus);
}
static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

#if PBL_API_EXISTS(touch_service_subscribe)
static int16_t s_tx0, s_ty0;
static int s_scroll0;
static bool s_moved;
static void sched_touch(const TouchEvent *e, void *ctx) {
  if (s_window != window_stack_get_top_window()) return;
  switch (e->type) {
    case TouchEvent_Touchdown:
      s_tx0 = e->x; s_ty0 = e->y; s_scroll0 = s_scroll; s_moved = false;
      break;
    case TouchEvent_PositionUpdate: {
      int dy = e->y - s_ty0, ady = dy < 0 ? -dy : dy;
      if (ady >= DRAG_MIN) s_moved = true;
      s_scroll = s_scroll0 - dy;
      clamp_scroll();
      layer_mark_dirty(s_list);
      break;
    }
    case TouchEvent_Liftoff: {
      int dx = e->x - s_tx0, dy = e->y - s_ty0;
      int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
      if (!s_moved) {                       // tap -> open that departure
        int ly = e->y - HDR_H + s_scroll;
        if (ly >= 0) {
          int row = ly / ROW_H;
          if (row < row_count()) { s_focus = row; open_dep(row); }
        }
      } else if (adx >= SWIPE_MIN && adx > ady) {
        window_stack_pop(true);             // horizontal swipe -> back
      } else if (dy > 0 && ady >= SWIPE_MIN && ady > adx && s_scroll0 == 0) {
        window_stack_pop(true);             // swipe down at top -> back to detail
      }
      break;
    }
    default: break;
  }
}
#endif

static void tick(void *ctx) {
  if (s_list) layer_mark_dirty(s_list);
  if (s_header) layer_mark_dirty(s_header); // refresh the live-clock chip
  s_tick = app_timer_register(1000, tick, NULL);
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
  s_tick = app_timer_register(1000, tick, NULL);
  window_set_click_config_provider(window, click_config);
}

static void window_appear(Window *window) {
#if PBL_API_EXISTS(touch_service_subscribe)
  touch_service_subscribe(sched_touch, NULL);
#endif
}
static void window_disappear(Window *window) {
#if PBL_API_EXISTS(touch_service_subscribe)
  touch_service_unsubscribe();
#endif
}

static void window_unload(Window *window) {
  if (s_tick) { app_timer_cancel(s_tick); s_tick = NULL; }
  layer_destroy(s_header);
  s_header = NULL;
  layer_destroy(s_list);
  s_list = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void schedule_window_push(uint8_t line_index) {
  if (line_index >= g_app_data.line_count) return;
  s_line = line_index;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
