#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
GODOT_PROJECT=~/entangl-copilot/app/addons/godotopenxrvendors/.bin/android

cd "$REPO_ROOT"

# Ensure scons is on PATH (miniforge install location)
export PATH="$HOME/miniforge3/bin:$PATH"
export ANDROID_HOME=~/Library/Android/sdk

# Clear SCons dependency cache so newly added .cpp files are always linked
rm -f .sconsign.dblite

echo "=== Building macOS framework (for Godot editor) ==="
scons platform=macos target=template_debug
scons platform=macos target=template_release

echo "=== Copying macOS framework into Godot project ==="
chmod -R u+w ~/entangl-copilot/app/addons/godotopenxrvendors/.bin/macos/
MACOS_SRC="$REPO_ROOT/demo/addons/godotopenxrvendors/.bin/macos"
MACOS_PROJECT=~/entangl-copilot/app/addons/godotopenxrvendors/.bin/macos
rm -rf "$MACOS_PROJECT/template_debug/libgodotopenxrvendors.macos.framework"
cp -r "$MACOS_SRC/template_debug/libgodotopenxrvendors.macos.framework" \
  "$MACOS_PROJECT/template_debug/libgodotopenxrvendors.macos.framework"
rm -rf "$MACOS_PROJECT/template_release/libgodotopenxrvendors.macos.framework"
cp -r "$MACOS_SRC/template_release/libgodotopenxrvendors.macos.framework" \
  "$MACOS_PROJECT/template_release/libgodotopenxrvendors.macos.framework"

echo "=== Building .so files (arm64 + x86_64) ==="
./gradlew buildSconsArtifacts

echo "=== Building MagicLeap AAR ==="
./gradlew :plugin:assembleMagicleapRelease --rerun-tasks

echo "=== Copying AAR into Godot project ==="
cp plugin/build/outputs/aar/godotopenxr-magicleap-release.aar \
  "$GODOT_PROJECT/release/godotopenxr-magicleap-release.aar"
cp plugin/build/outputs/aar/godotopenxr-magicleap-release.aar \
  "$GODOT_PROJECT/debug/godotopenxr-magicleap-debug.aar"

echo "=== Done ==="
