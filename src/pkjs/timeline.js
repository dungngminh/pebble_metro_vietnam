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

function pinId(lineId, slot) {
  return 'metro-' + lineId + '-' + slot;
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
  req.setRequestHeader('X-User-Token', token);
  // 404 here is fine: the slot had no pin to delete.
  req.onload = function () { console.log('pin DEL ' + id + ' -> ' + req.status); };
  req.send();
}

function buildPin(lineId, slot, line, epochSec, withReminder) {
  var pin = {
    id: pinId(lineId, slot),
    time: isoFromEpoch(epochSec),
    layout: {
      type: 'genericPin',
      title: line.name,
      subtitle: 'to ' + line.terminus,
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
        title: 'Train arriving soon',
        subtitle: line.name,
        tinyIcon: 'system://images/SCHEDULED_EVENT'
      }
    }];
  }
  return pin;
}

// Build the desired pin objects for one line (next `count` departures).
function buildPins(lineId, line, departures, count, withReminder) {
  var pins = [];
  if (count <= 0) return pins;
  if (count > MAX_SLOTS) count = MAX_SLOTS;
  departures.epochs.slice(0, count).forEach(function (e, slot) {
    pins.push(buildPin(lineId, slot, line, e, withReminder));
  });
  return pins;
}

// Reconcile the timeline against the full desired set of pins.
// `allLineIds` is every line the app knows about, so we can delete pins for
// lines the user has just UNTRACKED (their slots won't appear in `pins`).
function syncPins(pins, allLineIds) {
  var desired = {};
  pins.forEach(function (p) { desired[p.id] = true; });

  Pebble.getTimelineToken(function (token) {
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

    // Delete every slot of every known line that isn't currently wanted.
    allLineIds.forEach(function (lid) {
      for (var slot = 0; slot < MAX_SLOTS; slot++) {
        var id = pinId(lid, slot);
        if (!desired[id]) deletePin(token, id);
      }
    });
    // Upsert the current pins (PUT is idempotent).
    pins.forEach(function (p) { putPin(token, p); });
  }, function () {
    console.log('getTimelineToken failed - cannot sync pins');
  });
}

module.exports = { buildPins: buildPins, syncPins: syncPins };
