# V12 Build Link Fix

Fixes the Codemagic framework linker error:

```text
_OBJC_CLASS_$_UIView
_OBJC_CLASS_$_UIButton
_UIApplicationDidBecomeActiveNotification
_UIFontWeightBlack
_UIGraphicsGetCurrentContext
ld: symbol(s) not found for architecture arm64
```

Cause: the dynamic framework target compiled Objective-C++ UIKit code, but the XcodeGen project did not explicitly link the required iOS SDK frameworks.

Changed:

- `project.yml` now links:
  - `Foundation.framework`
  - `UIKit.framework`
  - `CoreGraphics.framework`
  - `QuartzCore.framework`
- `project.yml` also adds matching `OTHER_LDFLAGS`.
- `scripts/build_ios_framework.sh` passes the same `OTHER_LDFLAGS` directly to `xcodebuild` as a fallback.

Keep the fresh V11 scanner rebuild. This patch only fixes the iOS framework build/link step.
