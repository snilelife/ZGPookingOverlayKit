# Touch pass-through + hide/restore patch

This patch fixes the overlay blocking the host app.

## Changes

- The full-screen overlay no longer eats app touches.
- Only these UI parts receive touches:
  - ZG bubble
  - AIM ON/OFF quick button
  - open menu panel and its buttons
- The prediction canvas is touch-through.
- Long-press the ZG bubble or AIM button for about 0.75 seconds to hide the controls.
- When hidden, triple-tap the tiny invisible top-left corner area (72x72 px) to bring the ZG controls back.

## Files changed

- `src/ZGPookingOverlayController.mm`

## Notes

The restore area only catches touches while the controls are hidden. The rest of the screen stays pass-through so the app remains usable.
