/**************************************************************************/
/*  openxr_ml_spatial_anchor_manager.cpp                                 */
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

#include "classes/openxr_ml_spatial_anchor_manager.h"

#include <godot_cpp/classes/open_xr_interface.hpp>
#include <godot_cpp/classes/xr_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "util.h"

using namespace godot;

// ============================================================
// File-scope async context structs
// These are heap-allocated, passed through the C callback as void*,
// and deleted after dispatch. manager_id uses ObjectID so that if
// the node is freed before the callback fires we can detect it safely.
// All async callbacks are invoked on the main thread from _on_process().
// ============================================================

struct ML2CreateCtx {
	ObjectID manager_id;
	Transform3D original_transform; // global-space transform passed by caller
};

struct ML2PublishCtx {
	ObjectID manager_id;
	XrSpace space;
};

struct ML2LoadCtx {
	ObjectID manager_id;
	StringName uuid;
};

// ============================================================
// UUID string <-> XrUuidEXT conversion helpers
// ============================================================

static StringName xr_uuid_to_string(const XrUuidEXT &p_uuid) {
	return OpenXRUtilities::uuid_to_string_name(reinterpret_cast<const XrUuid &>(p_uuid));
}

static bool parse_uuid_string(const StringName &p_str, XrUuidEXT &r_uuid) {
	String s = String(p_str).replace("-", "");
	if (s.length() != 32) {
		return false;
	}
	for (int i = 0; i < 16; i++) {
		r_uuid.data[i] = (uint8_t)s.substr(i * 2, 2).hex_to_int();
	}
	return true;
}

// ============================================================
// GDScript bindings
// ============================================================

void OpenXRMlSpatialAnchorManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_anchor", "transform"), &OpenXRMlSpatialAnchorManager::create_anchor);
	ClassDB::bind_method(D_METHOD("load_anchor", "uuid"), &OpenXRMlSpatialAnchorManager::load_anchor);
	ClassDB::bind_method(D_METHOD("untrack_anchor", "uuid"), &OpenXRMlSpatialAnchorManager::untrack_anchor);
	ClassDB::bind_method(D_METHOD("get_anchor_uuids"), &OpenXRMlSpatialAnchorManager::get_anchor_uuids);
	ClassDB::bind_method(D_METHOD("get_anchor_node", "uuid"), &OpenXRMlSpatialAnchorManager::get_anchor_node);

	ADD_SIGNAL(MethodInfo("openxr_ml_spatial_anchor_tracked",
			PropertyInfo(Variant::OBJECT, "anchor_node"),
			PropertyInfo(Variant::STRING_NAME, "uuid"),
			PropertyInfo(Variant::BOOL, "is_new")));
	ADD_SIGNAL(MethodInfo("openxr_ml_spatial_anchor_untracked",
			PropertyInfo(Variant::STRING_NAME, "uuid")));
	ADD_SIGNAL(MethodInfo("openxr_ml_spatial_anchor_create_failed",
			PropertyInfo(Variant::TRANSFORM3D, "transform")));
	ADD_SIGNAL(MethodInfo("openxr_ml_spatial_anchor_load_failed",
			PropertyInfo(Variant::STRING_NAME, "uuid")));
}

void OpenXRMlSpatialAnchorManager::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			Ref<OpenXRInterface> iface = XRServer::get_singleton()->find_interface("OpenXR");
			if (iface.is_valid()) {
				iface->connect("session_stopping", callable_mp(this, &OpenXRMlSpatialAnchorManager::_on_openxr_session_stopping));
			}
			xr_origin = Object::cast_to<XROrigin3D>(get_parent());
		} break;
		case NOTIFICATION_EXIT_TREE: {
			Ref<OpenXRInterface> iface = XRServer::get_singleton()->find_interface("OpenXR");
			if (iface.is_valid()) {
				iface->disconnect("session_stopping", callable_mp(this, &OpenXRMlSpatialAnchorManager::_on_openxr_session_stopping));
			}
			xr_origin = nullptr;
			_cleanup_anchors();
		} break;
	}
}

void OpenXRMlSpatialAnchorManager::_on_openxr_session_stopping() {
	_cleanup_anchors();
}

void OpenXRMlSpatialAnchorManager::_cleanup_anchors() {
	OpenXRMlSpatialAnchorsExtension *core_ext = OpenXRMlSpatialAnchorsExtension::get_singleton();
	for (KeyValue<StringName, Anchor> &E : anchors) {
		Node3D *node = Object::cast_to<Node3D>(ObjectDB::get_instance(E.value.node));
		if (node) {
			if (node->get_parent()) {
				node->get_parent()->remove_child(node);
			}
			node->queue_free();
		}
		if (core_ext) {
			core_ext->untrack_entity(E.key);
		}
	}
	anchors.clear();
}

PackedStringArray OpenXRMlSpatialAnchorManager::_get_configuration_warnings() const {
	PackedStringArray warnings = Node::_get_configuration_warnings();
	if (is_inside_tree() && Object::cast_to<XROrigin3D>(get_parent()) == nullptr) {
		warnings.push_back("OpenXRMlSpatialAnchorManager must be a child of XROrigin3D.");
	}
	return warnings;
}

// ============================================================
// create_anchor — Step 1: start async create from pose
// ============================================================

void OpenXRMlSpatialAnchorManager::create_anchor(const Transform3D &p_transform) {
	ERR_FAIL_COND_MSG(!xr_origin, "OpenXRMlSpatialAnchorManager must be in the scene tree under XROrigin3D.");

	OpenXRMlSpatialAnchorsExtension *core_ext = OpenXRMlSpatialAnchorsExtension::get_singleton();
	ERR_FAIL_COND_MSG(!core_ext || !core_ext->is_spatial_anchors_supported(),
			"XR_ML_spatial_anchors extension is not supported on this device.");

	// Convert from caller's global space to XROrigin3D-local space.
	Transform3D local_transform = xr_origin->get_global_transform().affine_inverse() * p_transform;

	ML2CreateCtx *ctx = new ML2CreateCtx();
	ctx->manager_id = get_instance_id();
	ctx->original_transform = p_transform;

	bool ok = core_ext->create_anchor_from_pose(local_transform, _on_anchor_space_created_cb, ctx);
	if (!ok) {
		delete ctx;
		emit_signal("openxr_ml_spatial_anchor_create_failed", p_transform);
	}
}

// Step 2 static trampoline — called from OpenXRMlSpatialAnchorsExtension::_on_process()
void OpenXRMlSpatialAnchorManager::_on_anchor_space_created_cb(XrResult p_result, uint32_t p_count, const XrSpace *p_spaces, void *p_userdata) {
	ML2CreateCtx *ctx = static_cast<ML2CreateCtx *>(p_userdata);
	ObjectID manager_id = ctx->manager_id;
	Transform3D original_transform = ctx->original_transform;
	delete ctx;

	OpenXRMlSpatialAnchorManager *manager = Object::cast_to<OpenXRMlSpatialAnchorManager>(ObjectDB::get_instance(manager_id));
	if (!manager) {
		return; // manager was freed before callback fired
	}
	manager->_on_anchor_space_created(p_result, (p_count > 0) ? p_spaces[0] : XR_NULL_HANDLE, original_transform);
}

// Step 2 instance dispatch — publish the new space to get a persistent UUID
void OpenXRMlSpatialAnchorManager::_on_anchor_space_created(XrResult p_result, XrSpace p_space, const Transform3D &p_transform) {
	if (XR_FAILED(p_result) || p_space == XR_NULL_HANDLE) {
		WARN_PRINT("ML2 spatial anchor space creation failed.");
		emit_signal("openxr_ml_spatial_anchor_create_failed", p_transform);
		return;
	}

	OpenXRMlSpatialAnchorsStorageExtension *storage_ext = OpenXRMlSpatialAnchorsStorageExtension::get_singleton();
	if (!storage_ext || !storage_ext->is_spatial_anchors_storage_supported()) {
		WARN_PRINT("XR_ML_spatial_anchors_storage not available — anchor will not persist across sessions.");
		// Still complete setup without persistence; use a fake UUID from the XrSpace pointer.
		// In practice you'd want to fail here or have a non-persistent mode.
		emit_signal("openxr_ml_spatial_anchor_create_failed", p_transform);
		return;
	}

	ML2PublishCtx *ctx = new ML2PublishCtx();
	ctx->manager_id = get_instance_id();
	ctx->space = p_space;

	bool ok = storage_ext->publish_anchors(1, &p_space, _on_anchor_published_cb, ctx);
	if (!ok) {
		delete ctx;
		emit_signal("openxr_ml_spatial_anchor_create_failed", p_transform);
	}
}

// Step 3 static trampoline
void OpenXRMlSpatialAnchorManager::_on_anchor_published_cb(XrResult p_result, uint32_t p_uuid_count, const XrUuidEXT *p_uuids, void *p_userdata) {
	ML2PublishCtx *ctx = static_cast<ML2PublishCtx *>(p_userdata);
	ObjectID manager_id = ctx->manager_id;
	XrSpace space = ctx->space;
	delete ctx;

	OpenXRMlSpatialAnchorManager *manager = Object::cast_to<OpenXRMlSpatialAnchorManager>(ObjectDB::get_instance(manager_id));
	if (!manager) {
		return;
	}
	manager->_on_anchor_published(p_result, p_uuid_count, p_uuids, space, true);
}

// Step 3 instance dispatch — finalize setup now we have a UUID
void OpenXRMlSpatialAnchorManager::_on_anchor_published(XrResult p_result, uint32_t p_uuid_count, const XrUuidEXT *p_uuids, XrSpace p_space, bool p_is_new) {
	if (XR_FAILED(p_result) || p_uuid_count == 0) {
		WARN_PRINT("ML2 anchor publish failed.");
		emit_signal("openxr_ml_spatial_anchor_create_failed", Transform3D());
		return;
	}
	StringName uuid = xr_uuid_to_string(p_uuids[0]);
	_complete_anchor_setup(uuid, p_space, p_is_new);
}

// ============================================================
// load_anchor — recreate XrSpace from a stored UUID
// ============================================================

void OpenXRMlSpatialAnchorManager::load_anchor(const StringName &p_uuid) {
	ERR_FAIL_COND_MSG(!xr_origin, "OpenXRMlSpatialAnchorManager must be in the scene tree under XROrigin3D.");

	if (anchors.has(p_uuid)) {
		WARN_PRINT("Anchor already tracked: " + p_uuid);
		return;
	}

	XrUuidEXT uuid_ext;
	if (!parse_uuid_string(p_uuid, uuid_ext)) {
		WARN_PRINT("Invalid UUID format: " + p_uuid);
		emit_signal("openxr_ml_spatial_anchor_load_failed", p_uuid);
		return;
	}

	OpenXRMlSpatialAnchorsExtension *core_ext = OpenXRMlSpatialAnchorsExtension::get_singleton();
	ERR_FAIL_COND_MSG(!core_ext || !core_ext->is_spatial_anchors_supported(),
			"XR_ML_spatial_anchors not supported.");

	OpenXRMlSpatialAnchorsStorageExtension *storage_ext = OpenXRMlSpatialAnchorsStorageExtension::get_singleton();
	ERR_FAIL_COND_MSG(!storage_ext || !storage_ext->is_spatial_anchors_storage_supported(),
			"XR_ML_spatial_anchors_storage not supported.");

	XrSpatialAnchorsStorageML storage = storage_ext->get_storage();
	ERR_FAIL_COND_MSG(storage == XR_NULL_HANDLE, "Could not obtain ML2 anchor storage handle.");

	ML2LoadCtx *ctx = new ML2LoadCtx();
	ctx->manager_id = get_instance_id();
	ctx->uuid = p_uuid;

	bool ok = core_ext->create_anchors_from_uuids(1, &uuid_ext, storage, _on_anchor_loaded_cb, ctx);
	if (!ok) {
		delete ctx;
		emit_signal("openxr_ml_spatial_anchor_load_failed", p_uuid);
	}
}

void OpenXRMlSpatialAnchorManager::_on_anchor_loaded_cb(XrResult p_result, uint32_t p_count, const XrSpace *p_spaces, void *p_userdata) {
	ML2LoadCtx *ctx = static_cast<ML2LoadCtx *>(p_userdata);
	ObjectID manager_id = ctx->manager_id;
	StringName uuid = ctx->uuid;
	delete ctx;

	OpenXRMlSpatialAnchorManager *manager = Object::cast_to<OpenXRMlSpatialAnchorManager>(ObjectDB::get_instance(manager_id));
	if (!manager) {
		return;
	}
	manager->_on_anchor_loaded(p_result, p_spaces, p_count, uuid);
}

void OpenXRMlSpatialAnchorManager::_on_anchor_loaded(XrResult p_result, const XrSpace *p_spaces, uint32_t p_count, const StringName &p_uuid) {
	if (XR_FAILED(p_result) || p_count == 0 || p_spaces[0] == XR_NULL_HANDLE) {
		WARN_PRINT("Failed to load ML2 spatial anchor: " + p_uuid);
		emit_signal("openxr_ml_spatial_anchor_load_failed", p_uuid);
		return;
	}
	_complete_anchor_setup(p_uuid, p_spaces[0], false);
}

// ============================================================
// Shared finalisation
// ============================================================

void OpenXRMlSpatialAnchorManager::_complete_anchor_setup(const StringName &p_uuid, XrSpace p_space, bool p_is_new) {
	ERR_FAIL_COND(!xr_origin);
	ERR_FAIL_COND_MSG(anchors.has(p_uuid), "Anchor already tracked: " + p_uuid);

	OpenXRMlSpatialAnchorsExtension *core_ext = OpenXRMlSpatialAnchorsExtension::get_singleton();
	ERR_FAIL_NULL(core_ext);

	// Register the XrSpace with the extension so it gets a pose each frame.
	core_ext->track_entity(p_uuid, p_space);

	// Create the XRAnchor3D node that reads the tracker pose.
	XRAnchor3D *node = memnew(XRAnchor3D);
	node->set_name(p_uuid);
	node->set_tracker(p_uuid);
	xr_origin->add_child(node);

	anchors[p_uuid] = Anchor(node, p_space, p_uuid);

	emit_signal("openxr_ml_spatial_anchor_tracked", node, p_uuid, p_is_new);
}

// ============================================================
// untrack_anchor — remove from scene and delete from storage
// ============================================================

static void _noop_delete_cb(XrResult, void *) {}

void OpenXRMlSpatialAnchorManager::untrack_anchor(const StringName &p_uuid) {
	Anchor *anchor = anchors.getptr(p_uuid);
	ERR_FAIL_COND_MSG(!anchor, "Anchor not found: " + p_uuid);

	Node3D *node = Object::cast_to<Node3D>(ObjectDB::get_instance(anchor->node));
	if (node) {
		if (node->get_parent()) {
			node->get_parent()->remove_child(node);
		}
		node->queue_free();
	}

	// untrack_entity also destroys the XrSpace.
	OpenXRMlSpatialAnchorsExtension *core_ext = OpenXRMlSpatialAnchorsExtension::get_singleton();
	if (core_ext) {
		core_ext->untrack_entity(p_uuid);
	}

	// Delete from persistent storage.
	OpenXRMlSpatialAnchorsStorageExtension *storage_ext = OpenXRMlSpatialAnchorsStorageExtension::get_singleton();
	if (storage_ext && storage_ext->is_spatial_anchors_storage_supported()) {
		XrUuidEXT uuid_ext;
		if (parse_uuid_string(p_uuid, uuid_ext)) {
			storage_ext->delete_anchors(1, &uuid_ext, _noop_delete_cb, nullptr);
		}
	}

	anchors.erase(p_uuid);
	emit_signal("openxr_ml_spatial_anchor_untracked", p_uuid);
}

// ============================================================
// Accessors
// ============================================================

Array OpenXRMlSpatialAnchorManager::get_anchor_uuids() const {
	Array result;
	result.resize(anchors.size());
	int i = 0;
	for (const KeyValue<StringName, Anchor> &E : anchors) {
		result[i++] = E.key;
	}
	return result;
}

XRAnchor3D *OpenXRMlSpatialAnchorManager::get_anchor_node(const StringName &p_uuid) const {
	const Anchor *anchor = anchors.getptr(p_uuid);
	if (anchor) {
		return Object::cast_to<XRAnchor3D>(ObjectDB::get_instance(anchor->node));
	}
	return nullptr;
}
