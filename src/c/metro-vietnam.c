#include <pebble.h>
#include "data.h"
#include "line_menu.h"
#include "detail_window.h"
#include "splash.h"

// Ask the phone to recompute and resend departures (also re-pushes timeline pins).
void app_request_refresh(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) == APP_MSG_OK) {
    dict_write_uint8(out, MESSAGE_KEY_request, 1);
    app_message_outbox_send();
  }
}

static void ui_refresh(void) {
  line_menu_reload();
  detail_window_refresh();
}

static void parse_line_message(DictionaryIterator *it) {
  Tuple *t_index = dict_find(it, MESSAGE_KEY_line_index);
  if (!t_index) return;
  uint8_t index = t_index->value->uint8;

  Tuple *t_name = dict_find(it, MESSAGE_KEY_line_name);
  Tuple *t_term = dict_find(it, MESSAGE_KEY_terminus);
  Tuple *t_color = dict_find(it, MESSAGE_KEY_line_color);
  Tuple *t_status = dict_find(it, MESSAGE_KEY_status);
  Tuple *t_count = dict_find(it, MESSAGE_KEY_dep_count);
  Tuple *t_eps = dict_find(it, MESSAGE_KEY_dep_epochs);
  Tuple *t_first = dict_find(it, MESSAGE_KEY_first_train);

  const char *name = t_name ? t_name->value->cstring : "";
  const char *term = t_term ? t_term->value->cstring : "";
  GColor color = t_color ? (GColor){ .argb = (uint8_t)t_color->value->int32 } : GColorWhite;
  bool closed = t_status && strcmp(t_status->value->cstring, "closed") == 0;
  int32_t first = t_first ? t_first->value->int32 : 0;

  int32_t deps[MAX_DEPS] = {0};
  uint8_t dep_count = 0;
  if (t_eps) {
    uint8_t avail = t_eps->length / 4;
    if (avail > MAX_DEPS) avail = MAX_DEPS;
    const uint8_t *b = t_eps->value->data;
    for (uint8_t i = 0; i < avail; i++) {
      deps[i] = (int32_t)((uint32_t)b[i * 4] | ((uint32_t)b[i * 4 + 1] << 8) |
                          ((uint32_t)b[i * 4 + 2] << 16) | ((uint32_t)b[i * 4 + 3] << 24));
    }
    dep_count = avail;
  }
  if (t_count && t_count->value->uint8 < dep_count) dep_count = t_count->value->uint8;

  data_set_line(index, name, term, color, closed, first, deps, dep_count);

  Tuple *t_stations = dict_find(it, MESSAGE_KEY_stations);
  if (t_stations) {
    data_set_stations(index, t_stations->value->cstring);
  }
}

#define PERSIST_KEY_SHOW_SPLASH 2

static bool s_entered; // true once the splash has handed off to the main UI

static void inbox_received(DictionaryIterator *it, void *context) {
  Tuple *t_lc = dict_find(it, MESSAGE_KEY_line_count);
  if (t_lc) {
    data_set_line_count(t_lc->value->uint8);
    Tuple *t_ss = dict_find(it, MESSAGE_KEY_show_splash);
    if (t_ss) persist_write_bool(PERSIST_KEY_SHOW_SPLASH, t_ss->value->uint8 != 0);
  } else {
    parse_line_message(it);
    data_save();
    // With a single tracked line, jump straight to its countdown screen
    // (but never while the splash is still showing).
    if (s_entered && g_app_data.line_count == 1 && !detail_window_is_open()) {
      detail_window_push(0);
    }
  }
  ui_refresh();
}

// Called when the splash animation finishes.
static void on_splash_done(void) {
  s_entered = true;
  if (g_app_data.line_count == 1 && !detail_window_is_open()) {
    detail_window_push(0);
  }
}

static void init(void) {
  data_load();

  app_message_register_inbox_received(inbox_received);
  app_message_open(512, 64);

  line_menu_push();        // base window, revealed when the splash leaves
  bool show_splash = persist_exists(PERSIST_KEY_SHOW_SPLASH)
                         ? persist_read_bool(PERSIST_KEY_SHOW_SPLASH) : true;
  if (show_splash) {
    splash_push(on_splash_done);
  } else {
    on_splash_done();      // skip the intro
  }
  app_request_refresh();
}

static void deinit(void) {
  data_save();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
