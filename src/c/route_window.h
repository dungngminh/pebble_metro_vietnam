#pragma once
#include <pebble.h>

// Show the next train's stop list for the current A->B trip (station + ETA).
void route_window_push(uint8_t line_index);
