# Building

How to build and use this add-on.

## Gradle

Gradle is the build system for Android projects, it helps us convert this add-on into `.arr/.apk` files.

# Building for MagicLeap 2

ML2 uses an **x86_64** CPU. The Gradle build is already configured to build both arm64 and x86_64 `.so` files and include both in the MagicLeap AAR — you just need to run two tasks in sequence.

## Quick build

```bash
./build_ml2.sh
```

Builds the `.so` files, assembles the AAR, and copies it into the Godot project at `~/entangl-copilot/app/addons/godotopenxrvendors/`.

## Manual steps

```bash
# 1. Build macOS framework (loaded by the Godot editor — required for project settings to appear)
scons platform=macos target=template_debug
scons platform=macos target=template_release
cp -r bin/macos/template_debug/libgodotopenxrvendors.macos.framework \
  ~/entangl-copilot/app/addons/godotopenxrvendors/.bin/macos/template_debug/
cp -r bin/macos/template_release/libgodotopenxrvendors.macos.framework \
  ~/entangl-copilot/app/addons/godotopenxrvendors/.bin/macos/template_release/

# 2. Build .so files for both arm64 and x86_64
ANDROID_HOME=~/Library/Android/sdk ./gradlew buildSconsArtifacts

# 3. Assemble the MagicLeap AAR
./gradlew :plugin:assembleMagicleapRelease

# Output: plugin/build/outputs/aar/godotopenxr-magicleap-release.aar

# 3. Copy AAR into the Godot project
cp plugin/build/outputs/aar/godotopenxr-magicleap-release.aar \
   ~/entangl-copilot/app/addons/godotopenxrvendors/.bin/android/release/godotopenxr-magicleap-release.aar
cp plugin/build/outputs/aar/godotopenxr-magicleap-release.aar \
   ~/entangl-copilot/app/addons/godotopenxrvendors/.bin/android/debug/godotopenxr-magicleap-debug.aar
```

> **Why copy release into debug?** The debug template AAR was originally built without x86_64 support. Using the release build for both slots is fine for development on ML2 — the distinction only matters for debug symbols and optimisation level.
