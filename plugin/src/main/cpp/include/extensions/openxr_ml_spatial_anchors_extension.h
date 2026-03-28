/**************************************************************************/
/*  openxr_ml_spatial_anchors_extension.h                                */
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
#include <godot_cpp/classes/open_xr_extension_wrapper.hpp>
#include <godot_cpp/classes/xr_positional_tracker.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/vector.hpp>

#include "util.h"

using namespace godot;

// Wrapper for XR_ML_spatial_anchors + XR_EXT_future extensions (Magic Leap 2).
// Handles async anchor creation (from pose or from stored UUIDs) and per-frame
// pose tracking of active anchors via XRPositionalTracker.
class OpenXRMlSpatialAnchorsExtension : public OpenXRExtensionWrapper {
	GDCLASS(OpenXRMlSpatialAnchorsExtension, OpenXRExtensionWrapper);

public:
	Dictionary _get_requested_extensions(uint64_t p_xr_version) override;
	void _on_instance_created(uint64_t instance) override;
	void _on_instance_destroyed() override;
	void _on_process() override;

	bool is_spatial_anchors_supported() const { return ml_spatial_anchors_ext; }

	// Callback fired when one or more anchor spaces have been created.
	// p_spaces is only valid for the duration of the callback.
	typedef void (*AnchorCreatedCallback)(XrResult p_result, uint32_t p_space_count, const XrSpace *p_spaces, void *p_userdata);

	// Create a single anchor at the given world-space transform.
	bool create_anchor_from_pose(const Transform3D &p_transform, AnchorCreatedCallback p_callback, void *p_userdata);

	// Recreate tracked spaces for previously published anchors identified by UUID.
	// p_storage must be a valid XrSpatialAnchorsStorageML handle.
	bool create_anchors_from_uuids(uint32_t p_uuid_count, const XrUuidEXT *p_uuids, XrSpatialAnchorsStorageML p_storage, AnchorCreatedCallback p_callback, void *p_userdata);

	bool destroy_space(const XrSpace &p_space);

	// Register a named XrSpace so its pose is tracked each frame.
	void track_entity(const StringName &p_name, const XrSpace &p_space);
	void untrack_entity(const StringName &p_name);
	bool is_entity_tracked(const StringName &p_name) const;

	static OpenXRMlSpatialAnchorsExtension *get_singleton();

	OpenXRMlSpatialAnchorsExtension();
	~OpenXRMlSpatialAnchorsExtension();

protected:
	static void _bind_methods();

private:
	// XR_ML_spatial_anchors
	EXT_PROTO_XRRESULT_FUNC3(xrCreateSpatialAnchorsAsyncML,
			(XrSession), session,
			(const XrSpatialAnchorsCreateInfoBaseHeaderML *), createInfo,
			(XrFutureEXT *), future)

	EXT_PROTO_XRRESULT_FUNC3(xrCreateSpatialAnchorsCompleteML,
			(XrSession), session,
			(XrFutureEXT), future,
			(XrCreateSpatialAnchorsCompletionML *), completion)

	EXT_PROTO_XRRESULT_FUNC2(xrGetSpatialAnchorStateML,
			(XrSpace), anchor,
			(XrSpatialAnchorStateML *), state)

	EXT_PROTO_XRRESULT_FUNC1(xrDestroySpace,
			(XrSpace), space)

	EXT_PROTO_XRRESULT_FUNC4(xrLocateSpace,
			(XrSpace), space,
			(XrSpace), baseSpace,
			(XrTime), time,
			(XrSpaceLocation *), location)

	// XR_EXT_future
	EXT_PROTO_XRRESULT_FUNC3(xrPollFutureEXT,
			(XrInstance), instance,
			(const XrFuturePollInfoEXT *), pollInfo,
			(XrFuturePollResultEXT *), pollResult)

	bool initialize_ml_spatial_anchors_extension(const XrInstance &p_instance);

	struct PendingCreate {
		AnchorCreatedCallback callback = nullptr;
		void *userdata = nullptr;
		uint32_t expected_count = 1;

		PendingCreate() {}
		PendingCreate(AnchorCreatedCallback p_callback, void *p_userdata, uint32_t p_count) :
				callback(p_callback), userdata(p_userdata), expected_count(p_count) {}
	};
	HashMap<XrFutureEXT, PendingCreate> pending_creates;

	struct TrackedEntity {
		XrSpace space = XR_NULL_HANDLE;
		Ref<XRPositionalTracker> tracker;

		TrackedEntity(XrSpace p_space) :
				space(p_space) {}
		TrackedEntity() {}
	};
	HashMap<StringName, TrackedEntity> tracked_entities;

	XrInstance xr_instance = XR_NULL_HANDLE;

	void cleanup();

	static OpenXRMlSpatialAnchorsExtension *singleton;

	bool ml_spatial_anchors_ext = false;
	bool ext_future_ext = false;
	HashMap<String, bool *> request_extensions;
};
