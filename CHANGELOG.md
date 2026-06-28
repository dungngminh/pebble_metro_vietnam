# Changelog

All notable changes to Metro VietNam are documented here.

## [1.2.0] - 2026-06-28

### Added
- Live current time on every screen (lines, detail, departures, full line, station
  picker), drawn as an outlined chip in the header so it is never mistaken for the
  schedule/title times.
- Short line codes (A2, N-CG, BT-ST). Titles show the full line name on wide
  watches and the short code on the narrow 144x168 watches; the station picker and
  full-line headers fall back to compact wording there too (responsive layout).

### Changed
- The big countdown no longer flashes red in the final minute (steady text now).
- The boarding banner can be dismissed with the BACK button (or any tap/swipe on
  touch watches).

## [1.1.0] - 2026-06-28

### Added
- Departures page: swipe up / UP on the detail screen lists the upcoming trains
  for the trip (depart -> arrive + countdown). Tap one to open the full-line
  per-station schedule for that specific departure. Cache raised to 10 departures.

## [1.0.6] - 2026-06-28

### Changed
- Clearer timeline pins and arrival reminders. Each now shows the full route
  (origin -> destination), the scheduled departure time, and which station the
  train leaves from, instead of a generic "Train arriving soon".

### Added
- In-app boarding banner: when the app is open and a train is about to arrive, a
  clear banner appears over the screen showing "Train boarding", the trip route
  (A > B), and the boarding time — alongside the existing vibration.
- Live current time shown in the top-right of the detail screen header.

## [1.0.5] - 2026-06-28

### Added
- Timeline pins are now created for both directions of each tracked line: one set
  heading to the terminus and one heading back to the first station. Pin ids carry
  a direction (`fwd`/`rev`); the old single-direction pins are cleaned up on sync.
- New setting "Pin return direction too" (on by default) to toggle the reverse pins.

## [1.0.4] - 2026-06-28

### Fixed
- Timeline pin deletion failed with HTTP 500. The DELETE request was missing the
  `Content-Type: application/json` header that the Rebble timeline service requires
  (the PUT request already sent it). Untracking a line now actually removes its pins.

## [1.0.3] - 2026-06-28

### Fixed
- Legacy timeline pins from older versions (which used per-departure ids) are now
  purged. Earlier pins could not be removed after the id scheme changed, so they
  lingered on the timeline even after a line was untracked.

## [1.0.2] - 2026-06-28

### Fixed
- Untracking a line in settings now removes its timeline pins. Pin ids are stable
  per slot, and every slot of every known line that is no longer wanted is deleted,
  so pins no longer depend on persisted state to be cleaned up.
- Countdown now follows the train phase: before the train reaches station A it
  counts down to A ("until departure"); once it is running A→B it counts down to B
  ("until arrival").
- The progress bar only advances while the train is on the selected A→B route.

## [1.0.1] - 2026-06-28

### Fixed
- Settings no longer revert when reopening the config page (read/write Clay's own
  `clay-settings` store).
- Timeline pin icon changed to a schedule glyph.

## [1.0.0] - 2026-06-28

### Added
- Initial release. Tracks Vietnam's 3 operating metro lines (Cat Linh–Ha Dong,
  Nhon–Cau Giay, Ben Thanh–Suoi Tien).
- Per-line A→B trip picker with live countdown to the next train.
- Horizontal animated track with a running train marker.
- Full-line stop list with per-station ETAs.
- Direction reversal, station picker, touch + gesture support (Pebble Time 2).
- Timeline pins for upcoming departures with optional reminders.
- Splash animation, app icon, and Clay phone settings (tracked lines, pin count,
  reminders, intro animation).
