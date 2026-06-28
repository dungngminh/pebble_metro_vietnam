# Changelog

All notable changes to Metro VietNam are documented here.

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
