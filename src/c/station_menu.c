#include "station_menu.h"
#include "data.h"
#include "detail_window.h"
#include "anim.h"

#define HDR_H 26
#define ROW_H 40
#define DRAG_MIN 6   // px before a touch counts as a scroll (vs a tap)
#define SWIPE_MIN 30 // px for a horizontal back swipe

static Window *s_window;
static Layer *s_list;
static Layer *s_header;
static uint8_t s_line;
static uint8_t s_mode;   // 0 = pick A (origin), 1 = pick B (destination)
static int s_scroll;     // vertical scroll offset (px)
static uint8_t s_focus;  // focused row for button navigation
static int s_marq_off;   // marquee offset for long names
static AppTimer *s_marq_timer;

static int list_height(void) {
  return layer_get_bounds(s_list).size.h;
}

static int max_scroll(void) {
  int total = g_stations[s_line].count * ROW_H;
  int h = list_height();
  return total > h ? total - h : 0;
}

static void clamp_scroll(void) {
  if (s_scroll < 0) s_scroll = 0;
  int m = max_scroll();
  if (s_scroll > m) s_scroll = m;
}

// Keep the focused row fully visible (for button navigation).
static void reveal_focus(void) {
  int top = s_focus * ROW_H;
  int bot = top + ROW_H;
  if (top - s_scroll < 0) s_scroll = top;
  else if (bot - s_scroll > list_height()) s_scroll = bot - list_height();
  clamp_scroll();
}

// ---- header ------------------------------------------------------------------

static void header_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, g_app_data.lines[s_line].color);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_mode == 0 ? "Choose origin (A)" : "Choose destination (B)",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(6, 2, b.size.w - 12, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

// ---- list drawing ------------------------------------------------------------

static void list_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  LineData *line = &g_app_data.lines[s_line];
  uint8_t count = g_stations[s_line].count;
  uint8_t active = s_mode == 0 ? line->sel_station : line->sel_dest;
  uint8_t other = s_mode == 0 ? line->sel_dest : line->sel_station;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  for (uint8_t i = 0; i < count; i++) {
    int y = i * ROW_H - s_scroll;
    if (y + ROW_H <= 0 || y >= b.size.h) continue; // off-screen
    bool disabled = (s_mode == 1 && i == line->sel_station);

    if (i == s_focus) {
      graphics_context_set_fill_color(ctx, data_light(s_line));
      graphics_fill_rect(ctx, GRect(0, y, b.size.w, ROW_H), 0, GCornerNone);
    }

    GRect tr = GRect(8, y + 4, b.size.w - 30, 30);
    graphics_context_set_text_color(ctx, disabled ? GColorLightGray : GColorBlack);
    if (i == s_focus) {
      marquee_draw(ctx, g_stations[s_line].names[i], font, tr, GTextAlignmentLeft, s_marq_off);
    } else {
      graphics_draw_text(ctx, g_stations[s_line].names[i], font, tr,
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }

    GPoint dot = GPoint(b.size.w - 16, y + ROW_H / 2);
    if (i == active && !disabled) {
      graphics_context_set_fill_color(ctx, line->color);
      graphics_fill_circle(ctx, dot, 6);
    } else if (i == other) {
      graphics_context_set_stroke_color(ctx, line->color);
      graphics_draw_circle(ctx, dot, 5);
    }
  }
}

// ---- selection ---------------------------------------------------------------

static void pick_row(uint8_t row) {
  LineData *line = &g_app_data.lines[s_line];
  if (row >= g_stations[s_line].count) return;
  if (s_mode == 0) {
    line->sel_station = row;
    s_mode = 1;                 // go choose B
    s_focus = line->sel_dest;
    s_scroll = 0;
    reveal_focus();
    layer_mark_dirty(s_header);
    layer_mark_dirty(s_list);
  } else {
    if (row == line->sel_station) return; // A and B must differ
    line->sel_dest = row;
    data_save();
    window_stack_pop(true);
  }
}

// ---- buttons -----------------------------------------------------------------

static void up_click(ClickRecognizerRef rec, void *ctx) {
  if (s_focus > 0) { s_focus--; reveal_focus(); s_marq_off = 0; layer_mark_dirty(s_list); }
}
static void down_click(ClickRecognizerRef rec, void *ctx) {
  if (s_focus + 1 < g_stations[s_line].count) {
    s_focus++; reveal_focus(); s_marq_off = 0; layer_mark_dirty(s_list);
  }
}
static void select_click(ClickRecognizerRef rec, void *ctx) {
  pick_row(s_focus);
}
static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

// ---- touch (Pebble Time 2): drag to scroll, tap a row to pick it -------------

#if PBL_API_EXISTS(touch_service_subscribe)
static int16_t s_tx0, s_ty0;
static int s_scroll0;
static bool s_moved;

static void picker_touch(const TouchEvent *e, void *ctx) {
  if (s_window != window_stack_get_top_window()) return;
  switch (e->type) {
    case TouchEvent_Touchdown:
      s_tx0 = e->x; s_ty0 = e->y; s_scroll0 = s_scroll; s_moved = false;
      break;
    case TouchEvent_PositionUpdate: {
      int dy = e->y - s_ty0;
      int ady = dy < 0 ? -dy : dy;
      if (ady >= DRAG_MIN) s_moved = true;
      s_scroll = s_scroll0 - dy; // drag up -> scroll down
      clamp_scroll();
      layer_mark_dirty(s_list);
      break;
    }
    case TouchEvent_Liftoff: {
      int dx = e->x - s_tx0, dy = e->y - s_ty0;
      int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
      if (!s_moved) {                          // tap -> pick the touched row
        int ly = e->y - HDR_H + s_scroll;      // list-local content y
        if (ly >= 0) {
          uint8_t row = ly / ROW_H;
          s_focus = row < g_stations[s_line].count ? row : s_focus;
          pick_row(row);
        }
      } else if (adx >= SWIPE_MIN && adx > ady) {
        window_stack_pop(true);                // horizontal swipe -> back
      }
      break;
    }
    default: break;
  }
}
#endif

// ---- marquee timer -----------------------------------------------------------

static void marq_tick(void *ctx) {
  s_marq_off += 3;
  if (s_list) layer_mark_dirty(s_list);
  s_marq_timer = app_timer_register(70, marq_tick, NULL);
}

// ---- window ------------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_list = layer_create(GRect(0, HDR_H, b.size.w, b.size.h - HDR_H));
  layer_set_update_proc(s_list, list_update);
  layer_add_child(root, s_list);

  s_header = layer_create(GRect(0, 0, b.size.w, HDR_H));
  layer_set_update_proc(s_header, header_update);
  layer_add_child(root, s_header);

  s_focus = g_app_data.lines[s_line].sel_station;
  s_scroll = 0;
  reveal_focus();
  s_marq_off = 0;
  s_marq_timer = app_timer_register(70, marq_tick, NULL);

  window_set_click_config_provider(window, click_config);
}

static void window_appear(Window *window) {
#if PBL_API_EXISTS(touch_service_subscribe)
  touch_service_subscribe(picker_touch, NULL);
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
  detail_window_refresh();
}

void station_menu_push(uint8_t line_index) {
  if (g_stations[line_index].count == 0) return;
  s_line = line_index;
  s_mode = 0;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
