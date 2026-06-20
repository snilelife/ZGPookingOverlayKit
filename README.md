# ZGPookingOverlayKit

Clean drop-in overlay module for a host app that intentionally starts it. This version is tuned for Pooking / `com.pool.club.billiards.city` style aim lines.

This package is designed to build as a dynamic iOS framework. The framework binary is a dylib-style Mach-O inside:

```text
ZGPookingOverlayKit.framework/ZGPookingOverlayKit
```

It does not contain injection, loader, tweak, or auto-start code. The host app must call the public API.

Prediction lines are OFF by default. Starting the overlay only shows the draggable menu bubble. Lines appear after the menu `OVERLAY ON` toggle is tapped or after the host applies settings with `predictionEnabled = YES`.

## Files

```text
include/ZGPookingOverlayController.h   Objective-C public API
include/ZGPookingOverlayExports.h      Exported helper function prototypes
include/ZGPookingEngine.hpp            C++ prediction engine API
include/ZGPookingFrameScanner.hpp      Optional RGBA/BGRA frame scanner API
include/ZGOverlayTypes.hpp                C++ data types
src/ZGPookingOverlayController.mm      UIKit menu + overlay renderer
src/ZGPookingEngine.cpp                Aim prediction engine
src/ZGPookingFrameScanner.cpp          Host-fed frame scanner
bridges/ZGPookingOverlayExports.mm     Exported C ABI helpers for dylib/framework builds
project.yml                               XcodeGen dynamic framework project
CMakeLists.txt                            CMake dynamic framework target
scripts/build_ios_framework.sh            Xcode build helper
examples/HostAppUsage.mm                  Minimal integration example
cocos2dx/                                 Optional Cocos2d-x adapter source
```

## Build Dynamic Framework

From this folder:

```bash
chmod +x scripts/build_ios_framework.sh
scripts/build_ios_framework.sh
```

The build output is under:

```text
build/Build/Products/Release-iphoneos/
```

## Host App Start Call

In the host app source:

```objc
#import "ZGPookingOverlayController.h"

[ZGPookingOverlayController startInWindow:self.window];
```

Then feed live table geometry whenever the cue/balls/guide line changes. This can happen while prediction is still OFF; the renderer will stay blank until enabled.

```objc
[ZGPookingOverlayController updateTable:table
                                   cueBall:cue
                                hasCueBall:YES
                                     balls:balls
                                     count:ballCount
                                     guide:guide];
```

The overlay recomputes immediately on every `updateTable(...)` call. This clean kit only contains the menu, renderer, prediction engine, and exported start/update helpers.

## Exported Dylib-Friendly Functions

`include/ZGPookingOverlayExports.h` and `bridges/ZGPookingOverlayExports.mm` expose simple exported symbols:

```objc
#import "ZGPookingOverlayExports.h"

ZGPookingOverlayStartInWindow(window);
ZGPookingOverlayStartInView(view);
ZGPookingOverlayStop();
ZGPookingOverlaySetVisible(YES);
ZGPookingOverlayUpdateTable(table, cue, YES, balls, ballCount, guide);
ZGPookingOverlayUpdateFromFrameBytes(bytes, width, height, bytesPerRow, ZGPookingPixelFormatBGRA8888);
NSUInteger hiddenCount = ZGPookingOverlayHiddenLineCount();
ZGOverlayLine hiddenLines[32];
NSUInteger copied = ZGPookingOverlayCopyHiddenLines(hiddenLines, 32);
```

These are included in the dynamic framework target and are useful if you later build the same source as a `.dylib` or call it from mixed Objective-C++ code in your own host app.

## Frame Scanner

The scanner path is optional. It does not record the screen by itself. The host app must pass frame bytes it already owns:

```objc
BOOL ok = ZGPookingOverlayUpdateFromFrameBytes(bytes,
                                               width,
                                               height,
                                               bytesPerRow,
                                               ZGPookingPixelFormatBGRA8888);
```

The scanner estimates the Pooking table area, cue ball, visible balls, and the bright/cyan/violet on-screen guide line, then feeds the prediction engine. Best accuracy still comes from direct game geometry; frame scanning is a fallback when geometry is not available.

## Dynamic Guide Solver

When the host supplies `guide.valid = YES`, or when the frame scanner detects the visible Pooking guide line, the engine uses the guide direction to pick the contacted ball, calculate the ghost-ball position, then recompute:

```text
1. cue/ghost guide line
2. object-to-target line, where the target is a pocket or a bank rail contact
3. cue-after-hit / carom line
4. bank / bounce line
```

Moving the guide endpoint changes the selected contact, pocket path, carom path, and bank path on the next `updateTable(...)` call.
Guide-driven shots compute the ghost-ball impact point from the aim ray offset, so thin hits, full hits, and cue-after-hit paths change with where the guide crosses the object ball.
In Bank Shot mode the guide-driven solver follows the current aim angle through multiple rail contacts, then draws the pocket leg from the final rail point.

Hidden lines are recorded with roles:

```text
CueGuide
ObjectPath
GhostContact
CaromPath
BankPath
BouncePath
CollisionWarning
CenterLine
```

The scanner regression test draws two Pooking-like frames with different guide angles and verifies that the detected guide endpoint and cue prediction line both move:

```bash
clang++ -std=c++17 -Iinclude tests/pooking_scanner_engine_test.cpp src/ZGPookingFrameScanner.cpp src/ZGPookingEngine.cpp -o /tmp/pooking_scanner_engine_test
/tmp/pooking_scanner_engine_test
```

The engine mode regression test verifies four-line output and hidden recording across every style/mode combination, plus guide movement, object-path movement, cue-after-hit movement, finite geometry, bank rail targeting, multi-rail bank chaining, and blocked-lane avoidance:

```bash
clang++ -std=c++17 -Iinclude tests/pooking_engine_modes_test.cpp src/ZGPookingEngine.cpp -o /tmp/pooking_engine_modes_test
/tmp/pooking_engine_modes_test
```

The stabilizer regression test verifies that scanner-fed guide jitter is smoothed while real aim changes still snap immediately:

```bash
clang++ -std=c++17 -Iinclude tests/pooking_stabilizer_test.cpp src/ZGPookingFrameScanner.cpp src/ZGPookingEngine.cpp -o /tmp/pooking_stabilizer_test
/tmp/pooking_stabilizer_test
```

## Shot Solver

The prediction engine now scores candidate shots using:

```text
guide-line alignment
aim-ray ghost-ball contact point
cue-to-object lane clearance
object-to-pocket lane clearance
bank-rail lane clearance
one-rail and two-rail indirect bank alternatives
post-impact cue-ball tangent / stop prediction
long-shot distance preference
carom continuation path
blocked-lane penalties
```

This keeps the guide-driven line responsive while making Auto, Long, Carom, and Bank modes prefer cleaner paths instead of simply picking the closest ball.
For non-guided Bank Shot mode, the ghost-ball point is calculated from the chosen rail contact instead of the final pocket, so suggested bank shots are no longer scored as direct shots. Hidden line recording keeps the alternate one-rail and two-rail bank routes for later comparison.

## Menu Features

- Draggable `ZG` bubble.
- Overlay ON/OFF.
- Floating `AIM ON/OFF` quick button.
- Movable menu panel.
- Pooking aim ladder ON/OFF.
- Four-line prediction pack ON/OFF.
- Hidden line recording ON/OFF.
- Scanner smoothing ON/OFF.
- Cue prediction line ON/OFF.
- Pocket/object path ON/OFF.
- Bank/bounce paths ON/OFF.
- Carom path ON/OFF.
- Collision/block warning ON/OFF.
- Pocket heat/ring markers ON/OFF.
- Shot mode cycling:
  - Auto
  - Long Shot
  - Carom Shot
  - Bank Shot
- Route cycling:
  - Auto Hybrid
  - Guide Lock
  - Ball Geometry
  - Corner Lock
- Style cycling:
  - Simple
  - Advanced
  - Pro Video
- Auto/manual pocket selection.
- Bounce count.
- Line length.
- Ghost ball toggle.
- Detected balls toggle.
- Table centerline toggle.
- Dark floating menu with `created by zav G` footer.

## Important

A `.dylib` or dynamic framework will not run inside an app by itself. The host app must link it or explicitly load it from its own source code. This package intentionally does not include injection or binary patching.
