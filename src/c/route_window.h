#pragma once
#include <pebble.h>

// Show the next train's stop list for the current A->B trip (station + ETA).
void route_window_push(uint8_t line_index);

// Same, but for a specific departure index (dep_index < 0 means "next train").
void route_window_push_dep(uint8_t line_index, int dep_index);
