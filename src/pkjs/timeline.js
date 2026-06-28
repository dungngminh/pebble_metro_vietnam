/*
 * timeline.js — push/delete departure pins on the Pebble timeline via the
 * Rebble timeline web API. Requires a per-user token and phone internet.
 *
 * Pin ids are STABLE per (line, slot): 'metro-<lineId>-<slot>'. This makes
 * reconciliation reliable without depending on persisted state — syncPins()
 * deletes every slot of every known line that is not in the desired set, so
 * pins for an untracked line (or trimmed by a lower pin count) always go away.
 */

var TIMELINE_API = 'https://timeline-api.rebble.io/v1/user/pins/';
var MAX_SLOTS = 6; // upper bound on departures pinned per line
var LEGACY_IDS_KEY = 'metro_pin_ids'; // ids from the old per-epoch id scheme

function isoFromEpoch(sec) {
  return new Date(sec * 1000).toISOString();
}

// Local HH:MM for a departure epoch (phone's local time zone).
function hhmm(sec) {
  var d = new Date(sec * 1000);
  var h = d.getHours(), m = d.getMinutes();
  return (h < 10 ? '0' : '') + h + ':' + (m < 10 ? '0' : '') + m;
}

// dir is 'fwd' (toward the terminus) or 'rev' (back toward the first station).
function pinId(lineId, dir, slot) {
  return 'metro-' + lineId + '-' + dir + '-' + slot;
}

function putPin(token, pin) {
  var req = new XMLHttpRequest();
  req.open('PUT', TIMELINE_API + pin.id, true);
  req.setRequestHeader('Content-Type', 'application/json');
  req.setRequestHeader('X-User-Token', token);
  req.onload = function () { console.log('pin PUT ' + pin.id + ' -> ' + req.status); };
  req.send(JSON.stringify(pin));
}

function deletePin(token, id) {
  var req = new XMLHttpRequest();
  req.open('DELETE', TIMELINE_API + id, true);
  // The Rebble timeline service 500s on a DELETE without these headers, so mirror
  // the (working) PUT request exactly. 404 here just means the slot was empty.
  req.setRequestHeader('Content-Type', 'application/json');
  req.setRequestHeader('X-User-Token', token);
  req.onload = function () { console.log('pin DEL ' + id + ' -> ' + req.status); };
  // Mirror the canonical timeline helper, which sends a JSON body on DELETE too.
  req.send(JSON.stringify({ id: id }));
}

function buildPin(lineId, dir, slot, origin, heading, line, epochSec, withReminder) {
  var route = origin + ' -> ' + heading;
  var pin = {
    id: pinId(lineId, dir, slot),
    time: isoFromEpoch(epochSec),
    layout: {
      type: 'genericPin',
      title: line.name,
      subtitle: route,
      body: 'Departs ' + hhmm(epochSec) + ' from ' + origin + ' (scheduled).',
      tinyIcon: 'system://images/SCHEDULED_EVENT',
      primaryColor: '#FFFFFF',
      backgroundColor: '#000000'
    }
  };
  if (withReminder) {
    pin.reminders = [{
      time: isoFromEpoch(epochSec - 120),
      layout: {
        type: 'genericReminder',
        title: line.name,
        subtitle: route,
        body: 'Train departs ' + origin + ' at ' + hhmm(epochSec) + ' toward ' + heading + '.',
        tinyIcon: 'system://images/SCHEDULED_EVENT'
      }
    }];
  }
  return pin;
}

// Build the desired pin objects for one line, BOTH directions (next `count`
// departures each). The timetable is symmetric, so both directions share the
// same departure epochs; only the heading terminus differs.
//   fwd: heads to line.terminus (the named end station)
//   rev: heads back to the first station (line.stations[0])
function buildPins(lineId, line, departures, count, withReminder, bothDir) {
  var pins = [];
  if (count <= 0) return pins;
  if (count > MAX_SLOTS) count = MAX_SLOTS;
  var first = line.stations[0];
  // fwd departs the first station heading to the terminus; rev is the mirror.
  departures.epochs.slice(0, count).forEach(function (e, slot) {
    pins.push(buildPin(lineId, 'fwd', slot, first, line.terminus, line, e, withReminder));
    if (bothDir) {
      pins.push(buildPin(lineId, 'rev', slot, line.terminus, first, line, e, withReminder));
    }
  });
  return pins;
}

// Reconcile the timeline against the full desired set of pins.
// `allLineIds` is every line the app knows about, so we can delete pins for
// lines the user has just UNTRACKED (their slots won't appear in `pins`).
function syncPins(pins, allLineIds) {
  var desired = {};
  pins.forEach(function (p) { desired[p.id] = true; });

  console.log('syncPins: desired=' + Object.keys(desired).length + ' pin(s)');
  Pebble.getTimelineToken(function (token) {
    console.log('timeline token acquired');
    // One-time migration: older versions used per-epoch ids (metro-<line>-<epoch>)
    // recorded here. The slot-id scheme can't enumerate them, so delete them
    // explicitly, then clear the key so this runs only until they're gone.
    try {
      var legacy = JSON.parse(localStorage.getItem(LEGACY_IDS_KEY)) || [];
      if (legacy.length) {
        console.log('purging ' + legacy.length + ' legacy pin(s)');
        legacy.forEach(function (id) { deletePin(token, id); });
        localStorage.removeItem(LEGACY_IDS_KEY);
      }
    } catch (e) { localStorage.removeItem(LEGACY_IDS_KEY); }

    // Delete every slot (both directions) of every known line that isn't wanted.
    // Also delete the old single-direction id format ('metro-<line>-<slot>') so
    // pins from earlier versions are cleaned up.
    allLineIds.forEach(function (lid) {
      for (var slot = 0; slot < MAX_SLOTS; slot++) {
        var candidates = [
          pinId(lid, 'fwd', slot),
          pinId(lid, 'rev', slot),
          'metro-' + lid + '-' + slot // legacy single-direction id
        ];
        candidates.forEach(function (id) {
          if (!desired[id]) deletePin(token, id);
        });
      }
    });
    // Upsert the current pins (PUT is idempotent).
    pins.forEach(function (p) { putPin(token, p); });
  }, function () {
    console.log('getTimelineToken failed - cannot sync pins');
  });
}

module.exports = { buildPins: buildPins, syncPins: syncPins };
