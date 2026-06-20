#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if ! command -v xcodegen >/dev/null 2>&1; then
  echo "xcodegen is required. Install with: brew install xcodegen"
  exit 1
fi

rm -rf build generated ZGPookingOverlayKit.xcodeproj
mkdir -p generated build

xcodegen generate --spec project.yml

xcodebuild \
  -project ZGPookingOverlayKit.xcodeproj \
  -scheme ZGPookingOverlayKit \
  -configuration Release \
  -sdk iphoneos \
  -destination 'generic/platform=iOS' \
  -derivedDataPath build \
  clean build \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  DEVELOPMENT_TEAM=""

echo "Built products:"
find build/Build/Products/Release-iphoneos -maxdepth 2 -print
