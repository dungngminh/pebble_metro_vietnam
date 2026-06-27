#pragma once
#include <pebble.h>

// Push the countdown detail window for the line at `line_index` in g_app_data.
void detail_window_push(uint8_t line_index);

// Re-read data for the currently shown line (call when fresh data arrives).
void detail_window_refresh(void);

bool detail_window_is_open(void);
