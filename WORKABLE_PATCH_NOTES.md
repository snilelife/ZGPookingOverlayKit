# Workable package notes

This package is based on `ZGPookingOverlayKit_IMPACT_SOLVER_V9(1).zip`, but fixed for Codemagic and launch testing.

Changes:
- Removed the `work/ZGPookingOverlayKit/` wrapper so `codemagic.yaml` is at ZIP root.
- Added explicit XcodeGen source list to avoid duplicate engine symbols from stale files.
- Linked UIKit, Foundation, CoreGraphics, and QuartzCore.
- Added `include/ZGPookingOverlayKit.h` umbrella header.
- Added `src/ZGPookingOverlayAutoStart.mm`, which tries to attach the menu to the active UIWindow when the framework is actually loaded.
- Added `ZGPookingOverlayAutoAttachNow()` and `ZGPookingOverlayAutoStartSetEnabled()` public exports.

Important:
Embedding the framework in an IPA is not enough by itself. The app must load/link the framework. The best owner-source integration is:

```objc
#import <ZGPookingOverlayKit/ZGPookingOverlayKit.h>

ZGPookingOverlayStartInWindow(self.window);
// or
ZGPookingOverlayAutoAttachNow();
```
