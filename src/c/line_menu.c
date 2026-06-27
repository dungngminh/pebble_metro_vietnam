#include "line_menu.h"
#include "data.h"
#include "detail_window.h"

#define LINE_ROW_H 54

static Window *s_window;
static MenuLayer *s_menu;
static GBitmap *s_icon;

static GPoint s_play_pts[] = {{0, 0}, {0, 12}, {11, 6}};
static const GPathInfo PLAY_INFO = {3, s_play_pts};
static GPath *s_play;

// Status glyph: play triangle = running, pause bars = closed.
static void draw_status(GContext *ctx, GPoint c, bool closed, GColor color) {
  if (closed) {
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(c.x - 5, c.y - 6, 4, 12), 1, GCornersAll);
    graphics_fill_rect(ctx, GRect(c.x + 1, c.y - 6, 4, 12), 1, GCornersAll);
  } else {
    graphics_context_set_fill_color(ctx, color);
    gpath_move_to(s_play, GPoint(c.x - 5, c.y - 6));
    gpath_draw_filled(ctx, s_play);
  }
}

static uint16_t get_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return g_app_data.line_count > 0 ? g_app_data.line_count : 1;
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *context) {
  GRect bounds = layer_get_bounds(cell);

  if (g_app_data.line_count == 0) {
    // Empty state: centered app badge + helper text (plain white, no highlight).
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    GPoint c = GPoint(bounds.size.w / 2, bounds.size.h / 2 - 18);
    graphics_context_set_fill_color(ctx, GColorVividCerulean);
    graphics_fill_circle(ctx, c, 26);
    if (s_icon) {
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, s_icon, GRect(c.x - 12, c.y - 12, 25, 25));
    }
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "No line selected", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(4, c.y + 22, bounds.size.w - 8, 24), GTextOverflowModeFill,
                       GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, "Open the phone app", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(4, c.y + 46, bounds.size.w - 8, 20), GTextOverflowModeFill,
                       GTextAlignmentCenter, NULL);
    return;
  }

  uint8_t li = idx->row;
  const LineData *line = &g_app_data.lines[li];

  int16_t w = bounds.size.w;

  // Accent bar on the left in the line colour.
  graphics_context_set_fill_color(ctx, line->color);
  graphics_fill_rect(ctx, GRect(0, 0, 6, bounds.size.h), 0, GCornerNone);

  // Line name (leave room for the status glyph on the right).
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, line->name, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(12, 2, w - 44, 26), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  // Status glyph: running (play) vs closed (pause).
  draw_status(ctx, GPoint(w - 22, 14), line->closed, line->color);

  // Route A -> B.
  LineStations *st = &g_stations[li];
  const char *a = (st->count > 0 && line->sel_station < st->count)
                      ? st->names[line->sel_station] : line->name;
  const char *b = data_heading(li);
  char route[48];
  snprintf(route, sizeof(route), "%s > %s", a, b);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, route, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(12, 27, w - 66, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  // Next train, right-aligned: clock when closed/far, else minutes.
  char nx[12] = "--";
  time_t now = time(NULL);
  int next = data_next_index(li, now);
  if (next >= 0) {
    int32_t arr = data_arrival(li, next);
    int32_t secs = arr - (int32_t)now;
    if (line->closed || secs >= 3600) {
      char hm[8];
      time_t t = (time_t)arr;
      strftime(hm, sizeof(hm), "%H:%M", localtime(&t));
      snprintf(nx, sizeof(nx), "%s", hm);
    } else {
      if (secs < 0) secs = 0;
      snprintf(nx, sizeof(nx), "%dm", (int)(secs / 60));
    }
  }
  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, nx, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(w - 58, 28, 52, 22), GTextOverflowModeFill,
                     GTextAlignmentRight, NULL);
}

static int16_t get_cell_height(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  if (g_app_data.line_count == 0) {
    return layer_get_frame(menu_layer_get_layer(menu)).size.h; // full screen
  }
  return 54;
}

static void select_click(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  if (g_app_data.line_count == 0) return;
  detail_window_push(idx->row);
}

// ---- touch (Pebble Time 2): tap a line to open it ----------------------------
#if PBL_API_EXISTS(touch_service_subscribe)
static int16_t s_ty0;
static bool s_moved;

static void line_touch(const TouchEvent *e, void *ctx) {
  if (s_window != window_stack_get_top_window()) return;
  if (g_app_data.line_count == 0) return;
  if (e->type == TouchEvent_Touchdown) { s_ty0 = e->y; s_moved = false; return; }
  if (e->type == TouchEvent_PositionUpdate) {
    int dy = e->y - s_ty0; if ((dy < 0 ? -dy : dy) >= 6) s_moved = true;
    return;
  }
  if (e->type != TouchEvent_Liftoff || s_moved) return;
  uint16_t row = e->y / LINE_ROW_H;            // list fits the screen (<=3 rows)
  if (row < g_app_data.line_count) detail_window_push(row);
}

static void line_appear(Window *window) { touch_service_subscribe(line_touch, NULL); }
static void line_disappear(Window *window) { touch_service_unsubscribe(); }
#endif

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_APP_ICON);
  s_play = gpath_create(&PLAY_INFO);

  s_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_rows,
    .draw_row = draw_row,
    .get_cell_height = get_cell_height,
    .select_click = select_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, GColorPastelYellow, GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  if (s_icon) { gbitmap_destroy(s_icon); s_icon = NULL; }
  if (s_play) { gpath_destroy(s_play); s_play = NULL; }
  window_destroy(s_window);
  s_window = NULL;
}

void line_menu_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
#if PBL_API_EXISTS(touch_service_subscribe)
    .appear = line_appear,
    .disappear = line_disappear,
#endif
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void line_menu_reload(void) {
  if (s_menu) menu_layer_reload_data(s_menu);
}
