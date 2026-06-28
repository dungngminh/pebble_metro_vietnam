/*
 * timetable.js — static timetable data for Vietnam's 3 operating metro lines and
 * the departure-computation logic. There is NO public realtime/GTFS feed for these
 * systems; trains run on fixed operating hours + headway windows, so departure times
 * here are *scheduled estimates* anchored at the start of each headway window.
 *
 * All times are minutes-from-local-midnight. Headway is minutes between trains.
 * Station names are ASCII (no Vietnamese diacritics) by design.
 */

// Pebble GColor8 (argb) byte from an 8-bit-per-channel RGB. Each channel keeps its
// top 2 bits; alpha is always opaque (0b11).
function argb(r, g, b) {
  return (0x3 << 6) | ((r >> 6) << 4) | ((g >> 6) << 2) | (b >> 6);
}

// Hanoi lines (2A and 3-elevated) share the same operating profile.
var HANOI_WEEKDAY = [
  { start: 330, end: 390, headway: 10 }, // 05:30-06:30
  { start: 390, end: 540, headway: 6 },  // 06:30-09:00 peak
  { start: 540, end: 990, headway: 10 }, // 09:00-16:30
  { start: 990, end: 1170, headway: 6 }, // 16:30-19:30 peak
  { start: 1170, end: 1320, headway: 15 } // 19:30-22:00
];
var HANOI_WEEKEND = [
  { start: 330, end: 1320, headway: 10 } // 05:30-22:00 uniform
];

var HCMC_DAILY = [
  { start: 300, end: 360, headway: 12 },  // 05:00-06:00
  { start: 360, end: 480, headway: 8 },   // 06:00-08:00 peak
  { start: 480, end: 660, headway: 12 },  // 08:00-11:00
  { start: 660, end: 720, headway: 8 },   // 11:00-12:00 peak
  { start: 720, end: 930, headway: 12 },  // 12:00-15:30
  { start: 930, end: 1080, headway: 8 },  // 15:30-18:00 peak
  { start: 1080, end: 1320, headway: 12 } // 18:00-22:00
];

var LINES = {
  catlinh: {
    id: 'catlinh',
    name: 'Cat Linh - Ha Dong',
    short: 'A2',
    color: argb(0xE6, 0x00, 0x00), // red
    terminus: 'Yen Nghia',
    weekday: HANOI_WEEKDAY,
    weekend: HANOI_WEEKEND,
    stations: ['Cat Linh', 'La Thanh', 'Thai Ha', 'Lang', 'Thuong Dinh',
               'Vanh Dai 3', 'Phung Khoang', 'Van Quan', 'Ha Dong', 'La Khe',
               'Van Khe', 'Yen Nghia']
  },
  nhon: {
    id: 'nhon',
    name: 'Nhon - Cau Giay',
    short: 'N-CG',
    color: argb(0x00, 0xB0, 0x40), // green
    terminus: 'Cau Giay',
    weekday: HANOI_WEEKDAY,
    weekend: HANOI_WEEKEND,
    stations: ['Nhon', 'Minh Khai', 'Phu Dien', 'Cau Dien', 'Le Duc Tho',
               'Hanoi National University', 'Chua Ha', 'Cau Giay']
  },
  benthanh: {
    id: 'benthanh',
    name: 'Ben Thanh - Suoi Tien',
    short: 'BT-ST',
    color: argb(0x00, 0x70, 0xE0), // blue
    terminus: 'Suoi Tien',
    weekday: HCMC_DAILY,
    weekend: HCMC_DAILY,
    stations: ['Ben Thanh', 'Opera House', 'Ba Son', 'Van Thanh', 'Tan Cang',
               'Thao Dien', 'An Phu', 'Rach Chiec', 'Phuoc Long', 'Binh Thai',
               'Thu Duc', 'Hi-Tech Park', 'National University', 'Suoi Tien']
  }
};

var LINE_ORDER = ['catlinh', 'nhon', 'benthanh'];

function isWeekend(date) {
  var d = date.getDay();
  return d === 0 || d === 6;
}

// Epoch seconds of local midnight for the given date.
function midnightEpoch(date) {
  var m = new Date(date.getFullYear(), date.getMonth(), date.getDate(), 0, 0, 0, 0);
  return Math.floor(m.getTime() / 1000);
}

// All scheduled departures (epoch seconds) for one calendar day.
function dayDepartures(line, date) {
  var windows = isWeekend(date) ? line.weekend : line.weekday;
  var base = midnightEpoch(date);
  var deps = [];
  for (var i = 0; i < windows.length; i++) {
    var w = windows[i];
    for (var t = w.start; t < w.end; t += w.headway) {
      deps.push(base + t * 60);
    }
  }
  return deps; // already sorted: windows are contiguous & ascending
}

// Operating bounds (epoch seconds) for the given day.
function dayBounds(line, date) {
  var windows = isWeekend(date) ? line.weekend : line.weekday;
  var base = midnightEpoch(date);
  return {
    open: base + windows[0].start * 60,
    close: base + windows[windows.length - 1].end * 60
  };
}

/*
 * Next `n` scheduled departures for a line, at or after `fromDate`.
 * Returns { epochs: [seconds...], closed: bool, firstTrain: seconds }.
 * `closed` is true when `fromDate` is outside today's operating window
 * (before first train or after last train).
 */
function nextDepartures(lineId, fromDate, n) {
  var line = LINES[lineId];
  if (!line) return null;
  var nowSec = Math.floor(fromDate.getTime() / 1000);

  var epochs = dayDepartures(line, fromDate).filter(function (e) { return e >= nowSec; });

  // Walk forward day-by-day until we have n (handles end-of-day and closures).
  var probe = fromDate;
  while (epochs.length < n) {
    probe = new Date(probe.getFullYear(), probe.getMonth(), probe.getDate() + 1, 12, 0, 0, 0);
    epochs = epochs.concat(dayDepartures(line, probe));
  }
  epochs = epochs.slice(0, n);

  var bounds = dayBounds(line, fromDate);
  var closed = nowSec < bounds.open || nowSec >= bounds.close;

  return { epochs: epochs, closed: closed, firstTrain: epochs[0] };
}

module.exports = {
  LINES: LINES,
  LINE_ORDER: LINE_ORDER,
  nextDepartures: nextDepartures
};
