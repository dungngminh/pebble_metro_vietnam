#pragma once
#include <pebble.h>

// Show the station picker for the given line: a direction toggle row followed by
// the line's stations. Selecting a station updates the line's sel_station.
void station_menu_push(uint8_t line_index);
