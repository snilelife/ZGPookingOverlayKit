# V13 Core Touch + Scanner Patch

This patch is focused on the current Pooking test problem: the menu/overlay blocking touches, stale lines drawing on lobby screens, and too many unused feature toggles.

## Changed

- Added overlay touch pass-through. The game receives touches everywhere except the `ZG` bubble, `AIM` quick button, and open menu panel.
- Reduced the visible menu to the core test path:
  - Enable Prediction
  - Cue Ball Line
  - Live Scanner
  - Scanner Smoothing
  - Line Extender
  - Prediction Style
- `AIM ON` now enables only the core cue-line scanner path. Pocket/bank/carom/ladder/heat/marker debug features stay disabled.
- All prediction features are still OFF on launch.
- Invalid or non-table scans now clear the old prediction result immediately.
- Added a green endpoint box to the cue guide renderer.
- Tightened Pooking table detection so title/lobby graphics are rejected instead of being mistaken for a table.
- Added the latest title/lobby screenshots as negative scanner regression fixtures.

## Verified Locally

Direct C++ tests passed:

- `pooking_real_screenshot_test`
- `pooking_scanner_engine_test`
- `pooking_engine_modes_test`
- `pooking_stabilizer_test`

The local machine does not have full Xcode/iOS SDK active, so the iOS framework build must be verified in Codemagic or a Mac with Xcode selected.
