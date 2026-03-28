/**************************************************************************/
/*  openxr_ml_spatial_anchor_manager.h                                   */
/**************************************************************************/
/*                       This file is part of:                            */
/*                              GODOT XR                                  */
/*                      https://godotengine.org                           */
/**************************************************************************/
/* Copyright (c) 2022-present Godot XR contributors (see CONTRIBUTORS.md) */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include <openxr/openxr.h>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/xr_anchor3d.hpp>
#include <godot_cpp/classes/xr_origin3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include "extensions/openxr_ml_spatial_anchors_extension.h"
#include "extensions/openxr_ml_spatial_anchors_storage_extension.h"

using namespace godot;

// GDScript-facing Node for managing ML2 spatial anchors.
// Must be a child of XROrigin3D.
//
// Signals:
//   openxr_ml_spatial_anchor_tracked(anchor_node, uuid, is_new)
//   openxr_ml_spatial_anchor_untracked(uuid)
//   openxr_ml_spatial_anchor_create_failed(transform)
//   openxr_ml_spatial_anchor_load_failed(uuid)
class OpenXRMlSpatialAnchorManager : public Node {
	GDCLASS(OpenXRMlSpatialAnchorManager, Node);

public:
	// Create a new anchor at world-space transform. Publishes to ML2 storage automatically.
	// Emits openxr_ml_spatial_anchor_tracked on success with is_new=true.
	void create_anchor(const Transform3D &p_transform);

	// Load a previously created anchor by its UUID string.
	// Emits openxr_ml_spatial_anchor_tracked on success with is_new=false.
	void load_anchor(const StringName &p_uuid);

	// Remove an anchor from the scene and delete it from ML2 persistent storage.
	void untrack_anchor(const StringName &p_uuid);

	Array get_anchor_uuids() const;
	XRAnchor3D *get_anchor_node(const StringName &p_uuid) const;

	void _notification(int p_what);
	PackedStringArray _get_configuration_warnings() const override;

protected:
	static void _bind_methods();

private:
	struct Anchor {
		ObjectID node;
		XrSpace space = XR_NULL_HANDLE;
		StringName uuid;

		Anchor() {}
		Anchor(XRAnchor3D *p_node, XrSpace p_space, const StringName &p_uuid) :
				node(p_node->get_instance_id()), space(p_space), uuid(p_uuid) {}
	};
	HashMap<StringName, Anchor> anchors;

	XROrigin3D *xr_origin = nullptr;

	// --- create_anchor async chain ---
	// 1. create_anchor() -> core_ext.create_anchor_from_pose() [async]
	// 2. _on_anchor_space_created() -> storage_ext.publish_anchors() [async]
	// 3. _on_anchor_published() -> _complete_anchor_setup()

	static void _on_anchor_space_created_cb(XrResult p_result, uint32_t p_count, const XrSpace *p_spaces, void *p_userdata);
	void _on_anchor_space_created(XrResult p_result, XrSpace p_space, const Transform3D &p_original_transform);

	static void _on_anchor_published_cb(XrResult p_result, uint32_t p_uuid_count, const XrUuidEXT *p_uuids, void *p_userdata);
	void _on_anchor_published(XrResult p_result, uint32_t p_uuid_count, const XrUuidEXT *p_uuids, XrSpace p_space, bool p_is_new);

	// --- load_anchor async chain ---
	// 1. load_anchor() -> core_ext.create_anchors_from_uuids() [async]
	// 2. _on_anchor_loaded() -> _complete_anchor_setup()

	static void _on_anchor_loaded_cb(XrResult p_result, uint32_t p_count, const XrSpace *p_spaces, void *p_userdata);
	void _on_anchor_loaded(XrResult p_result, const XrSpace *p_spaces, uint32_t p_count, const StringName &p_uuid);

	// Shared finalisation: registers tracker, creates XRAnchor3D node, emits signal.
	void _complete_anchor_setup(const StringName &p_uuid, XrSpace p_space, bool p_is_new);

	void _cleanup_anchors();
	void _on_openxr_session_stopping();
};
