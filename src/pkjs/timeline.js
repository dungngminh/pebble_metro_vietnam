/*
 * timeline.js — push/delete departure pins on the Pebble timeline via the
 * Rebble timeline web API. Requires a per-user token and phone internet.
 *
 * syncPins() reconciles: it PUTs the desired pins and DELETEs any pin that was
 * pushed before but is no longer wanted (e.g. a line was untracked or the pin
 * count lowered).
 */

var TIMELINE_API = 'https://timeline-api.rebble.io/v1/user/pins/';
var PIN_IDS_KEY = 'metro_pin_ids';

function isoFromEpoch(sec) {
  return new Date(sec * 1000).toISOString();
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
  req.onload = function () { console.log('pin DEL ' + id + ' -> ' + req.status); };
  req.send();
}

function buildPin(lineId, line, epochSec, withReminder) {
  var pin = {
    id: 'metro-' + lineId + '-' + epochSec,
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
  departures.epochs.slice(0, count).forEach(function (e) {
    pins.push(buildPin(lineId, line, e, withReminder));
  });
  return pins;
}

// Reconcile the timeline against the full desired set of pins.
function syncPins(pins) {
  var newIds = pins.map(function (p) { return p.id; });
  var prevIds;
  try { prevIds = JSON.parse(localStorage.getItem(PIN_IDS_KEY)) || []; }
  catch (e) { prevIds = []; }

  Pebble.getTimelineToken(function (token) {
    // Remove pins that are no longer wanted.
    prevIds.filter(function (id) { return newIds.indexOf(id) < 0; })
           .forEach(function (id) { deletePin(token, id); });
    // Upsert the current pins (PUT is idempotent).
    pins.forEach(function (p) { putPin(token, p); });
    localStorage.setItem(PIN_IDS_KEY, JSON.stringify(newIds));
  }, function () {
    console.log('getTimelineToken failed - cannot sync pins');
  });
}

module.exports = { buildPins: buildPins, syncPins: syncPins };
