#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
GODOT_PROJECT=~/entangl-copilot/app/addons/godotopenxrvendors/.bin/android

cd "$REPO_ROOT"

export PATH="$HOME/miniforge3/bin:$PATH"
export ANDROID_HOME=~/Library/Android/sdk

# Clean only plugin object files (preserve godot-cpp cache for faster rebuilds)
find . -path './thirdparty' -prune -o -name '*.o' -print -delete

echo "=== Building .so files + MagicLeap AAR ==="
./gradlew buildSconsArtifacts :plugin:clean :plugin:assembleMagicleapRelease

echo "=== Copying AAR into Godot project ==="
cp plugin/build/outputs/aar/godotopenxr-magicleap-release.aar \
  "$GODOT_PROJECT/release/godotopenxr-magicleap-release.aar"
cp plugin/build/outputs/aar/godotopenxr-magicleap-release.aar \
  "$GODOT_PROJECT/debug/godotopenxr-magicleap-debug.aar"

echo "=== Done ==="
