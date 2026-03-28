# ML2 spatial anchor changes

This document records the modifications made to this fork of `godot_openxr_vendors` to support **Magic Leap 2 spatial anchors** in Godot 4.6 and the bug fixes required to get the plugin loading correctly.

---

## What was added

`OpenXRMlSpatialAnchorManager` — a GDScript-facing `Node` class that wraps the two ML2-specific OpenXR extensions:

| Extension                                                                  | Purpose                                                                                |
| -------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| `XR_ML_spatial_anchors` (`OpenXRMlSpatialAnchorsExtension`)                | Create anchors from a pose, load anchors from UUIDs, track `XrSpace` handles per frame |
| `XR_ML_spatial_anchors_storage` (`OpenXRMlSpatialAnchorsStorageExtension`) | Publish new anchors to ML2's persistent spatial map, delete anchors from storage       |

The manager must be placed as a **direct child of `XROrigin3D`** in the scene tree. On `NOTIFICATION_ENTER_TREE` it caches a pointer to its `XROrigin3D` parent, which is required to convert between global world space and XR-local space.

### API surface

```gdscript
# Place in scene under XROrigin3D, then:
manager.create_anchor(world_transform: Transform3D)
manager.load_anchor(uuid: StringName)
manager.untrack_anchor(uuid: StringName)
manager.get_anchor_uuids() -> Array
manager.get_anchor_node(uuid: StringName) -> XRAnchor3D

# Signals
signal openxr_ml_spatial_anchor_tracked(anchor_node: XRAnchor3D, uuid: StringName, is_new: bool)
signal openxr_ml_spatial_anchor_untracked(uuid: StringName)
signal openxr_ml_spatial_anchor_create_failed(transform: Transform3D)
signal openxr_ml_spatial_anchor_load_failed(uuid: StringName)
```

### How anchor coordinates work

ML2's spatial anchor system operates in the device's persistent **spatial map** coordinate frame. This is a world-locked coordinate system tied to the physical environment, independent of where the device was positioned when the app launched.

When you call `create_anchor(world_transform)`:

1. The manager converts the transform from Godot world space into `XROrigin3D`-local space.
2. The ML2 runtime creates an `XrSpace` at that physical location.
3. The space is published to ML2 storage and assigned a persistent UUID.
4. A `XRAnchor3D` node is created under `XROrigin3D` and its tracker is set to the UUID.

Each frame, Godot's OpenXR implementation queries every tracked `XrSpace` for its current pose relative to the LOCAL reference space. This pose is fed into the `XRAnchor3D` node's transform. Because ML2 knows the anchor's absolute position in the spatial map, and also knows the headset's current position in the spatial map, it can always compute the correct relative pose — so the `XRAnchor3D` sits at the right physical location regardless of where the head was at startup.

When you call `load_anchor(uuid)` in a later session:

1. The UUID is looked up in ML2 storage to get the original `XrSpace` handle.
2. The same tracking pipeline starts again — the anchor's physical location is recovered from the spatial map.

---

## Bug fix: `OpenXRMlSpatialAnchorManager` registration level

**File:** `plugin/src/main/cpp/register_types.cpp`

**Problem:** `OpenXRMlSpatialAnchorManager` extends `Node`. In Godot's GDExtension initialisation, `Node` and the rest of the scene class hierarchy are only available from `MODULE_INITIALIZATION_LEVEL_SCENE` onwards. The class was originally registered at `MODULE_INITIALIZATION_LEVEL_CORE`, causing this error at startup:

```
Attempt to register an extension class 'OpenXRMlSpatialAnchorManager'
using non-existing parent class 'Node'
```

**Fix:** Moved `GDREGISTER_CLASS(OpenXRMlSpatialAnchorManager)` from the `CORE` block to the `SCENE` block, alongside `OpenXRFbSpatialAnchorManager` and the other scene-level node classes.

```cpp
// BEFORE (CORE block) — wrong, Node not available yet
GDREGISTER_CLASS(OpenXRMlSpatialAnchorsExtension);
GDREGISTER_CLASS(OpenXRMlSpatialAnchorsStorageExtension);
GDREGISTER_CLASS(OpenXRMlSpatialAnchorManager);  // ← was here

// AFTER (SCENE block) — correct
GDREGISTER_CLASS(OpenXRMlMarkerUnderstandingManager);
GDREGISTER_CLASS(OpenXRMlSpatialAnchorManager);  // ← moved here
GDREGISTER_CLASS(OpenXRHybridApp);
```

The two extension classes (`OpenXRMlSpatialAnchorsExtension`, `OpenXRMlSpatialAnchorsStorageExtension`) remain at CORE level because they are `OpenXRExtensionWrapper` subclasses, not `Node` subclasses, and they need to register themselves with the OpenXR API before the session starts.

---

## Build instructions

See [BUILD_ML2.md](BUILD_ML2.md).

---

## Files changed (commit guide)

```
plugin/src/main/cpp/register_types.cpp          # OpenXRMlSpatialAnchorManager registration level fix
plugin/src/main/cpp/classes/openxr_ml_spatial_anchor_manager.cpp    # new file
plugin/src/main/cpp/classes/openxr_ml_spatial_anchor_manager.h      # new file (in include/)
plugin/src/main/cpp/include/classes/openxr_ml_spatial_anchor_manager.h
```

The built AAR outputs are not committed to this repo — rebuild from source using the instructions above.
