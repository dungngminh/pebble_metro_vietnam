#include "data.h"

#define PERSIST_KEY_DATA 1

AppData g_app_data;
LineStations g_stations[MAX_LINES];

void data_load(void) {
  if (persist_exists(PERSIST_KEY_DATA)) {
    persist_read_data(PERSIST_KEY_DATA, &g_app_data, sizeof(g_app_data));
  } else {
    g_app_data.line_count = 0;
  }
  for (int i = 0; i < MAX_LINES; i++) g_stations[i].count = 0;
}

void data_save(void) {
  persist_write_data(PERSIST_KEY_DATA, &g_app_data, sizeof(g_app_data));
}

bool data_has_lines(void) {
  return g_app_data.line_count > 0;
}

void data_set_line_count(uint8_t count) {
  if (count > MAX_LINES) count = MAX_LINES;
  g_app_data.line_count = count;
}

void data_set_line(uint8_t index, const char *name, const char *terminus,
                   GColor color, bool closed, int32_t first_train,
                   const int32_t *deps, uint8_t dep_count) {
  if (index >= MAX_LINES) return;
  LineData *line = &g_app_data.lines[index];
  // Preserve user selection across data refreshes.
  uint8_t sel = line->sel_station;
  uint8_t dst = line->sel_dest;

  strncpy(line->name, name, NAME_LEN - 1);
  line->name[NAME_LEN - 1] = '\0';
  strncpy(line->terminus, terminus, TERM_LEN - 1);
  line->terminus[TERM_LEN - 1] = '\0';
  line->color = color;
  line->closed = closed;
  line->first_train = first_train;
  if (dep_count > MAX_DEPS) dep_count = MAX_DEPS;
  line->dep_count = dep_count;
  for (uint8_t i = 0; i < dep_count; i++) line->deps[i] = deps[i];

  line->sel_station = sel;
  line->sel_dest = dst;
}

void data_set_stations(uint8_t index, const char *joined) {
  if (index >= MAX_LINES) return;
  LineStations *st = &g_stations[index];
  st->count = 0;
  const char *p = joined;
  while (*p && st->count < MAX_STATIONS) {
    int n = 0;
    while (p[n] && p[n] != '\n') n++;
    int copy = n < STN_LEN - 1 ? n : STN_LEN - 1;
    strncpy(st->names[st->count], p, copy);
    st->names[st->count][copy] = '\0';
    st->count++;
    p += n;
    if (*p == '\n') p++;
  }
  // Clamp / default the A and B selection against the new station count.
  if (index < g_app_data.line_count && st->count > 0) {
    LineData *line = &g_app_data.lines[index];
    if (line->sel_station >= st->count) line->sel_station = 0;
    if (line->sel_dest >= st->count) line->sel_dest = st->count - 1;
    // First-time default: ride the whole line (origin -> terminus).
    if (line->sel_station == line->sel_dest) {
      line->sel_station = 0;
      line->sel_dest = st->count - 1;
    }
  }
}

int data_direction(uint8_t line_index) {
  if (line_index >= g_app_data.line_count) return 0;
  LineData *line = &g_app_data.lines[line_index];
  return line->sel_dest >= line->sel_station ? 0 : 1;
}

void data_swap(uint8_t line_index) {
  if (line_index >= g_app_data.line_count) return;
  LineData *line = &g_app_data.lines[line_index];
  uint8_t t = line->sel_station;
  line->sel_station = line->sel_dest;
  line->sel_dest = t;
}

int32_t data_offset(uint8_t line_index) {
  if (line_index >= g_app_data.line_count) return 0;
  LineData *line = &g_app_data.lines[line_index];
  uint8_t n = g_stations[line_index].count;
  if (n == 0) return 0;
  uint8_t idx = line->sel_station;
  if (idx >= n) idx = 0;
  // Arrival of an A->B train at station A depends on the travel direction.
  uint8_t from_origin = data_direction(line_index) == 0 ? idx : (n - 1 - idx);
  return (int32_t)from_origin * SEG_SECONDS;
}

int32_t data_arrival(uint8_t line_index, uint8_t dep_i) {
  return g_app_data.lines[line_index].deps[dep_i] + data_offset(line_index);
}

int data_next_index(uint8_t line_index, time_t now) {
  if (line_index >= g_app_data.line_count) return -1;
  LineData *line = &g_app_data.lines[line_index];
  for (uint8_t i = 0; i < line->dep_count; i++) {
    if ((time_t)data_arrival(line_index, i) > now) return i;
  }
  return -1;
}

int data_remaining(uint8_t line_index) {
  if (line_index >= g_app_data.line_count) return 0;
  LineData *line = &g_app_data.lines[line_index];
  int diff = (int)line->sel_dest - (int)line->sel_station;
  return diff < 0 ? -diff : diff; // number of segments from A to B
}

int32_t data_dest_arrival(uint8_t line_index, uint8_t dep_i) {
  return data_arrival(line_index, dep_i) + (int32_t)data_remaining(line_index) * SEG_SECONDS;
}

int data_trip_len(uint8_t line_index) {
  return data_remaining(line_index) + 1;
}

uint8_t data_trip_station(uint8_t line_index, int i) {
  LineData *line = &g_app_data.lines[line_index];
  return line->sel_dest >= line->sel_station ? line->sel_station + i
                                             : line->sel_station - i;
}

int32_t data_arrival_at(uint8_t line_index, uint8_t dep_i, uint8_t station_idx) {
  uint8_t n = g_stations[line_index].count;
  int32_t dep = g_app_data.lines[line_index].deps[dep_i];
  if (n == 0) return dep;
  uint8_t from_origin = data_direction(line_index) == 0 ? station_idx : (n - 1 - station_idx);
  return dep + (int32_t)from_origin * SEG_SECONDS;
}

TripPhase data_trip_state(uint8_t line_index, time_t now,
                          int32_t *a_time, int32_t *b_time) {
  if (line_index >= g_app_data.line_count) return TRIP_NONE;
  LineData *line = &g_app_data.lines[line_index];
  // 1) A train already running the chosen A->B leg (departed A, not yet at B).
  for (uint8_t i = 0; i < line->dep_count; i++) {
    int32_t aA = data_arrival(line_index, i);
    int32_t aB = data_dest_arrival(line_index, i);
    if ((time_t)aA <= now && now < (time_t)aB) {
      if (a_time) *a_time = aA;
      if (b_time) *b_time = aB;
      return TRIP_INROUTE;
    }
  }
  // 2) Otherwise the next train still approaching A.
  for (uint8_t i = 0; i < line->dep_count; i++) {
    int32_t aA = data_arrival(line_index, i);
    if ((time_t)aA > now) {
      if (a_time) *a_time = aA;
      if (b_time) *b_time = data_dest_arrival(line_index, i);
      return TRIP_APPROACH;
    }
  }
  return TRIP_NONE;
}

GColor data_light(uint8_t line_index) {
  GColor c = line_index < g_app_data.line_count ? g_app_data.lines[line_index].color
                                                : GColorWhite;
  uint8_t r = (c.argb >> 4) & 3, g = (c.argb >> 2) & 3, b = c.argb & 3;
  r = (r + 3) / 2; g = (g + 3) / 2; b = (b + 3) / 2; // blend toward white
  GColor out;
  out.argb = (0x3 << 6) | (r << 4) | (g << 2) | b;
  return out;
}

const char *data_heading(uint8_t line_index) {
  if (line_index >= g_app_data.line_count) return "";
  LineData *line = &g_app_data.lines[line_index];
  LineStations *st = &g_stations[line_index];
  if (st->count == 0) return line->terminus;
  uint8_t b = line->sel_dest < st->count ? line->sel_dest : st->count - 1;
  return st->names[b];
}
