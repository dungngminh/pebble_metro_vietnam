#pragma once
#include <pebble.h>

#define MAX_LINES 3
#define MAX_DEPS 6
#define MAX_STATIONS 14
#define NAME_LEN 28
#define TERM_LEN 20
#define STN_LEN 26

// Estimated travel time between adjacent stations (seconds). VN metros publish no
// per-segment timings, so arrival times at a chosen station are estimates.
#define SEG_SECONDS 120

// Persisted per-line state: line meta, terminus departures, and the user's
// chosen station / direction. Station names are NOT stored here (too large for
// persistent storage); they live in g_stations and are re-synced from the phone.
typedef struct {
  char name[NAME_LEN];
  char terminus[TERM_LEN];
  GColor color;
  bool closed;            // currently outside operating hours
  int32_t first_train;    // epoch of first train when closed
  uint8_t dep_count;
  int32_t deps[MAX_DEPS]; // ascending epoch seconds, departures from origin
  uint8_t sel_station;    // A: departure station index
  uint8_t sel_dest;       // B: destination station index
} LineData;

// RAM-only station names for a line (re-sent by the phone each sync).
typedef struct {
  uint8_t count;
  char names[MAX_STATIONS][STN_LEN];
} LineStations;

typedef struct {
  uint8_t line_count;
  LineData lines[MAX_LINES];
} AppData;

extern AppData g_app_data;
extern LineStations g_stations[MAX_LINES];

void data_load(void);
void data_save(void);
bool data_has_lines(void);

void data_set_line_count(uint8_t count);
void data_set_line(uint8_t index, const char *name, const char *terminus,
                   GColor color, bool closed, int32_t first_train,
                   const int32_t *deps, uint8_t dep_count);
void data_set_stations(uint8_t index, const char *joined); // '\n'-separated

// Seconds added to a departure to reach the selected station in the chosen direction.
int32_t data_offset(uint8_t line_index);

// Arrival epoch of departure `dep_i` at the selected station.
int32_t data_arrival(uint8_t line_index, uint8_t dep_i);

// Index of the first departure whose arrival at the selected station is after `now`.
int data_next_index(uint8_t line_index, time_t now);

// A pale tint of the line's accent colour (for menu highlight backgrounds).
GColor data_light(uint8_t line_index);

// Name of the destination station (B).
const char *data_heading(uint8_t line_index);

// Travel direction of trains serving A->B: 0 = toward terminus (B after A),
// 1 = toward origin (B before A).
int data_direction(uint8_t line_index);

// Swap the A and B stations (reverse the trip).
void data_swap(uint8_t line_index);

// Stations between A and B (inclusive of the ride length).
int data_remaining(uint8_t line_index);

// Estimated arrival epoch at the heading terminus for departure `dep_i`.
int32_t data_dest_arrival(uint8_t line_index, uint8_t dep_i);

// The trip A->B as an ordered list of station indices.
int data_trip_len(uint8_t line_index);                       // number of stops
uint8_t data_trip_station(uint8_t line_index, int i);        // i-th stop index

// Estimated arrival epoch of departure `dep_i` at an arbitrary station index.
int32_t data_arrival_at(uint8_t line_index, uint8_t dep_i, uint8_t station_idx);
