/*
 * timeline.js — push departure pins to the user's Pebble timeline via the
 * Rebble timeline web API. Requires a per-user timeline token (Pebble.getTimelineToken)
 * and an internet connection on the phone.
 */

var TIMELINE_API = 'https://timeline-api.rebble.io/v1/user/pins/';

function isoFromEpoch(sec) {
  return new Date(sec * 1000).toISOString();
}

function putPin(token, pin) {
  var req = new XMLHttpRequest();
  req.open('PUT', TIMELINE_API + pin.id, true);
  req.setRequestHeader('Content-Type', 'application/json');
  req.setRequestHeader('X-User-Token', token);
  req.onload = function () {
    console.log('timeline pin ' + pin.id + ' -> ' + req.status);
  };
  req.onerror = function () {
    console.log('timeline pin ' + pin.id + ' failed');
  };
  req.send(JSON.stringify(pin));
}

function buildPin(lineId, line, epochSec, withReminder) {
  var pin = {
    id: 'metro-' + lineId + '-' + epochSec,
    time: isoFromEpoch(epochSec),
    layout: {
      type: 'genericPin',
      title: line.name,
      subtitle: 'to ' + line.terminus,
      tinyIcon: 'app://images/TRAIN',
      primaryColor: '#FFFFFF',
      backgroundColor: '#000000'
    }
  };
  if (withReminder) {
    pin.reminders = [{
      time: isoFromEpoch(epochSec - 120), // 2 min before
      layout: {
        type: 'genericReminder',
        title: 'Train arriving soon',
        subtitle: line.name,
        tinyIcon: 'app://images/TRAIN'
      }
    }];
  }
  return pin;
}

/*
 * Push the next `count` departures of one line as timeline pins.
 * `departures` is the { epochs, ... } object from timetable.nextDepartures.
 */
function pushDepartures(lineId, line, departures, count, withReminder) {
  if (count <= 0) return;
  Pebble.getTimelineToken(function (token) {
    var epochs = departures.epochs.slice(0, count);
    for (var i = 0; i < epochs.length; i++) {
      putPin(token, buildPin(lineId, line, epochs[i], withReminder));
    }
  }, function () {
    console.log('getTimelineToken failed - cannot push pins');
  });
}

module.exports = { pushDepartures: pushDepartures };
