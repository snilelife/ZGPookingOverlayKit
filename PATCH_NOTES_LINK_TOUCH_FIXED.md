# ZGPookingOverlayKit ZavIOS V10 link + touch fixed

Fixes the Codemagic linker error with missing UIKit/CoreGraphics symbols by linking required iOS frameworks in both project.yml and the xcodebuild command.

Also adds:
- exact source list to avoid duplicate-symbol builds
- umbrella header include/ZGPookingOverlayKit.h
- src/ZGPookingOverlayAutoStart.mm for auto attach
- long-hold ZG/AIM to hide controls
- triple-tap top-left restore hotspot
- hitTest pass-through so the game/app remains usable under the overlay
