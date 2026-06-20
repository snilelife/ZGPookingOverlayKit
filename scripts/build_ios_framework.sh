#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
rm -rf build generated ZGPookingOverlayKit.xcodeproj
xcodegen generate
xcodebuild \
  -project ZGPookingOverlayKit.xcodeproj \
  -scheme ZGPookingOverlayKit \
  -configuration Release \
  -sdk iphoneos \
  -derivedDataPath build \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  build
