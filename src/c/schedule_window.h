#pragma once
#include <pebble.h>

// List upcoming departures (khung gio) for the current A->B trip. Tapping a row
// opens the full-line stop list for that specific departure.
void schedule_window_push(uint8_t line_index);
