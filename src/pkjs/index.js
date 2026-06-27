/*
 * index.js — PebbleKit JS entry point.
 *
 * Responsibilities:
 *   - Clay config page (phone settings: which lines to track, pin count, reminders).
 *   - On watch request (or config change), compute next departures per tracked line
 *     and send them to the watch over AppMessage.
 *   - Push timeline pins for the configured number of departures.
 *
 * The watch owns only rendering + countdown; all timetable logic lives here.
 */

var Clay = require('pebble-clay');
var clayConfig = require('./config');
var keys = require('message_keys');
var timetable = require('./timetable');
var timeline = require('./timeline');

var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var DEPS_TO_WATCH = 6; // departures sent to the watch for display + countdown

// ---- settings ----------------------------------------------------------------

function loadSettings() {
  var raw = localStorage.getItem('metro_settings');
  var s = raw ? JSON.parse(raw) : {};
  return {
    track: {
      catlinh: s.track_catlinh !== undefined ? s.track_catlinh : true,
      nhon: !!s.track_nhon,
      benthanh: !!s.track_benthanh
    },
    pinCount: s.pin_count !== undefined ? parseInt(s.pin_count, 10) : 4,
    reminders: s.reminders !== undefined ? s.reminders : true,
    showSplash: s.show_splash !== undefined ? s.show_splash : true
  };
}

function trackedLineIds(settings) {
  return timetable.LINE_ORDER.filter(function (id) { return settings.track[id]; });
}

// ---- AppMessage send queue ---------------------------------------------------
// Send messages one at a time, each on the previous one's ack, to avoid
// overflowing the inbox buffer.

var sendQueue = [];
var sending = false;

function pump() {
  if (sending || sendQueue.length === 0) return;
  sending = true;
  var dict = sendQueue.shift();
  Pebble.sendAppMessage(dict, function () {
    sending = false;
    pump();
  }, function (e) {
    console.log('sendAppMessage failed: ' + JSON.stringify(e));
    sending = false;
    pump();
  });
}

function enqueue(dict) {
  sendQueue.push(dict);
  pump();
}

function epochsToBytes(epochs) {
  var b = [];
  for (var i = 0; i < epochs.length; i++) {
    var e = epochs[i];
    b.push(e & 0xff, (e >> 8) & 0xff, (e >> 16) & 0xff, (e >> 24) & 0xff);
  }
  return b;
}

// ---- main flow ---------------------------------------------------------------

function syncToWatch() {
  var settings = loadSettings();
  var ids = trackedLineIds(settings);
  var now = new Date();
  console.log('syncToWatch: ' + ids.length + ' line(s): ' + ids.join(','));

  var header = {};
  header[keys.line_count] = ids.length;
  header[keys.show_splash] = settings.showSplash ? 1 : 0;
  enqueue(header);

  ids.forEach(function (id, index) {
    var line = timetable.LINES[id];
    var dep = timetable.nextDepartures(id, now, DEPS_TO_WATCH);

    var dict = {};
    dict[keys.line_index] = index;
    dict[keys.line_name] = line.name;
    dict[keys.line_color] = line.color;
    dict[keys.terminus] = line.terminus;
    dict[keys.status] = dep.closed ? 'closed' : 'open';
    dict[keys.dep_count] = dep.epochs.length;
    dict[keys.dep_epochs] = epochsToBytes(dep.epochs);
    dict[keys.first_train] = dep.firstTrain;
    dict[keys.stations] = line.stations.join('\n');
    dict[keys.station_count] = line.stations.length;
    enqueue(dict);

    // Push timeline pins for this line.
    timeline.pushDepartures(id, line, dep, settings.pinCount, settings.reminders);
  });
}

// ---- events ------------------------------------------------------------------

Pebble.addEventListener('ready', function () {
  console.log('Metro VN PKJS ready');
  // Push current data proactively: the watch's initial request may have been
  // sent before this JS environment finished loading.
  syncToWatch();
});

// Watch asks for fresh data (request == 1).
Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && e.payload.request) {
    syncToWatch();
  }
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var resp;
  try {
    resp = JSON.parse(decodeURIComponent(e.response));
  } catch (err) {
    console.log('config parse error: ' + err);
    return;
  }
  var flat = {};
  Object.keys(resp).forEach(function (k) {
    flat[k] = resp[k] && resp[k].value !== undefined ? resp[k].value : resp[k];
  });
  localStorage.setItem('metro_settings', JSON.stringify(flat));
  syncToWatch();
});
