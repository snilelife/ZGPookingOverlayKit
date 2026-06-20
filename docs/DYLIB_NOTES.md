# Dynamic Library Notes

On iOS, the easiest legal packaging is a dynamic framework:

```text
ZGPookingOverlayKit.framework/ZGPookingOverlayKit
```

The inner `ZGPookingOverlayKit` binary is a Mach-O dynamic library. A host app can embed and link that framework when it is built from source.

This kit intentionally does not include:

```text
injection
binary patching
auto-load tricks
third-party app hooks
```

Expected integration model:

```text
host app source -> links framework -> calls [ZGPookingOverlayController startInWindow:]
```

Build options:

```text
1. Codemagic: use codemagic.yaml
2. Local Xcode: run scripts/build_ios_framework.sh
3. CMake: use CMakeLists.txt as a dynamic framework target
```

The public entry point is:

```objc
#import "ZGPookingOverlayController.h"

[ZGPookingOverlayController startInWindow:self.window];
```

Starting the overlay does not enable prediction lines. The default settings keep `predictionEnabled` off, so the user must tap `OVERLAY ON` in the menu or the host app must apply settings with `predictionEnabled = YES`.

For a dylib-style build, `bridges/ZGPookingOverlayExports.mm` also provides exported C symbols:

```objc
ZGPookingOverlayStartInWindow(window);
ZGPookingOverlayUpdateTable(table, cue, YES, balls, ballCount, guide);
ZGPookingOverlayStop();
```

Those exports are only an easier call surface. They do not auto-load the overlay; the host app still starts it intentionally.
