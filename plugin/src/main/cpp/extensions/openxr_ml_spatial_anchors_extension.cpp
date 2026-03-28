/**************************************************************************/
/*  openxr_ml_spatial_anchors_extension.cpp                              */
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

#include "extensions/openxr_ml_spatial_anchors_extension.h"

#include <godot_cpp/classes/open_xrapi_extension.hpp>
#include <godot_cpp/classes/xr_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

OpenXRMlSpatialAnchorsExtension *OpenXRMlSpatialAnchorsExtension::singleton = nullptr;

OpenXRMlSpatialAnchorsExtension *OpenXRMlSpatialAnchorsExtension::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(OpenXRMlSpatialAnchorsExtension());
	}
	return singleton;
}

OpenXRMlSpatialAnchorsExtension::OpenXRMlSpatialAnchorsExtension() :
		OpenXRExtensionWrapper() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "An OpenXRMlSpatialAnchorsExtension singleton already exists.");

	request_extensions[XR_ML_SPATIAL_ANCHORS_EXTENSION_NAME] = &ml_spatial_anchors_ext;
	request_extensions[XR_EXT_FUTURE_EXTENSION_NAME] = &ext_future_ext;

	singleton = this;
}

OpenXRMlSpatialAnchorsExtension::~OpenXRMlSpatialAnchorsExtension() {
	cleanup();
}

void OpenXRMlSpatialAnchorsExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_spatial_anchors_supported"), &OpenXRMlSpatialAnchorsExtension::is_spatial_anchors_supported);
}

void OpenXRMlSpatialAnchorsExtension::cleanup() {
	ml_spatial_anchors_ext = false;
	ext_future_ext = false;
	xr_instance = XR_NULL_HANDLE;
	pending_creates.clear();

	for (KeyValue<StringName, TrackedEntity> &E : tracked_entities) {
		if (E.value.tracker.is_valid()) {
			XRServer::get_singleton()->remove_tracker(E.value.tracker);
			E.value.tracker.unref();
		}
	}
	tracked_entities.clear();
}

Dictionary OpenXRMlSpatialAnchorsExtension::_get_requested_extensions(uint64_t p_xr_version) {
	Dictionary result;
	for (auto &ext : request_extensions) {
		uint64_t value = reinterpret_cast<uint64_t>(ext.value);
		result[ext.key] = (Variant)value;
	}
	return result;
}

void OpenXRMlSpatialAnchorsExtension::_on_instance_created(uint64_t instance) {
	xr_instance = (XrInstance)instance;

	if (ml_spatial_anchors_ext) {
		bool result = initialize_ml_spatial_anchors_extension((XrInstance)instance);
		if (!result) {
			UtilityFunctions::print("Failed to initialize XR_ML_spatial_anchors extension");
			ml_spatial_anchors_ext = false;
		}
	}
}

void OpenXRMlSpatialAnchorsExtension::_on_instance_destroyed() {
	cleanup();
}

bool OpenXRMlSpatialAnchorsExtension::initialize_ml_spatial_anchors_extension(const XrInstance &p_instance) {
	GDEXTENSION_INIT_XR_FUNC_V(xrCreateSpatialAnchorsAsyncML);
	GDEXTENSION_INIT_XR_FUNC_V(xrCreateSpatialAnchorsCompleteML);
	GDEXTENSION_INIT_XR_FUNC_V(xrGetSpatialAnchorStateML);
	GDEXTENSION_INIT_XR_FUNC_V(xrDestroySpace);
	GDEXTENSION_INIT_XR_FUNC_V(xrLocateSpace);
	GDEXTENSION_INIT_XR_FUNC_V(xrPollFutureEXT);
	return true;
}

void OpenXRMlSpatialAnchorsExtension::_on_process() {
	if (!ml_spatial_anchors_ext || xr_instance == XR_NULL_HANDLE) {
		return;
	}

	// Poll pending async anchor create futures.
	Vector<XrFutureEXT> completed;
	for (KeyValue<XrFutureEXT, PendingCreate> &E : pending_creates) {
		XrFuturePollInfoEXT poll_info = {
			XR_TYPE_FUTURE_POLL_INFO_EXT,
			nullptr,
			E.key,
		};
		XrFuturePollResultEXT poll_result = {
			XR_TYPE_FUTURE_POLL_RESULT_EXT,
			nullptr,
			XR_FUTURE_STATE_PENDING_EXT,
		};

		XrResult result = xrPollFutureEXT(xr_instance, &poll_info, &poll_result);
		if (XR_FAILED(result)) {
			WARN_PRINT("xrPollFutureEXT failed for pending anchor create.");
			WARN_PRINT(get_openxr_api()->get_error_string(result));
			E.value.callback(result, 0, nullptr, E.value.userdata);
			completed.push_back(E.key);
			continue;
		}

		if (poll_result.state != XR_FUTURE_STATE_READY_EXT) {
			continue;
		}

		// Future is ready — complete the async create.
		Vector<XrSpace> spaces;
		spaces.resize(E.value.expected_count);

		XrCreateSpatialAnchorsCompletionML completion = {
			XR_TYPE_CREATE_SPATIAL_ANCHORS_COMPLETION_ML,
			nullptr,
			XR_SUCCESS,
			(uint32_t)spaces.size(),
			spaces.ptrw(),
		};

		result = xrCreateSpatialAnchorsCompleteML(SESSION, E.key, &completion);
		if (XR_FAILED(result)) {
			WARN_PRINT("xrCreateSpatialAnchorsCompleteML failed.");
			WARN_PRINT(get_openxr_api()->get_error_string(result));
			E.value.callback(result, 0, nullptr, E.value.userdata);
		} else if (XR_FAILED(completion.futureResult)) {
			WARN_PRINT("xrCreateSpatialAnchorsCompleteML async operation failed.");
			WARN_PRINT(get_openxr_api()->get_error_string(completion.futureResult));
			E.value.callback(completion.futureResult, 0, nullptr, E.value.userdata);
		} else {
			E.value.callback(XR_SUCCESS, completion.spaceCount, spaces.ptr(), E.value.userdata);
		}

		completed.push_back(E.key);
	}

	for (XrFutureEXT future : completed) {
		pending_creates.erase(future);
	}

	// Update pose for each tracked anchor entity.
	for (KeyValue<StringName, TrackedEntity> &E : tracked_entities) {
		if (E.value.tracker.is_null()) {
			E.value.tracker.instantiate();
			E.value.tracker->set_tracker_name(E.key);
			E.value.tracker->set_tracker_desc(String("ML2 Anchor ") + E.key);
			E.value.tracker->set_tracker_type(XRServer::TRACKER_ANCHOR);
			XRServer::get_singleton()->add_tracker(E.value.tracker);
		}

		XrSpaceLocation location = {
			XR_TYPE_SPACE_LOCATION,
			nullptr,
			0,
			{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } },
		};

		XrResult result = xrLocateSpace(
				E.value.space,
				reinterpret_cast<XrSpace>(get_openxr_api()->get_play_space()),
				get_openxr_api()->get_predicted_display_time(),
				&location);

		if (XR_FAILED(result)) {
			WARN_PRINT("xrLocateSpace failed for ML2 anchor " + E.key);
			continue;
		}

		if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
				(location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
			Transform3D transform(
					Basis(Quaternion(
							location.pose.orientation.x,
							location.pose.orientation.y,
							location.pose.orientation.z,
							location.pose.orientation.w)),
					Vector3(
							location.pose.position.x,
							location.pose.position.y,
							location.pose.position.z));
			E.value.tracker->set_pose("default", transform, Vector3(), Vector3(), XRPose::XR_TRACKING_CONFIDENCE_HIGH);
		} else {
			Ref<XRPose> default_pose = E.value.tracker->get_pose("default");
			if (default_pose.is_valid()) {
				default_pose->set_tracking_confidence(XRPose::XR_TRACKING_CONFIDENCE_NONE);
			} else {
				E.value.tracker->set_pose("default", Transform3D(), Vector3(), Vector3(), XRPose::XR_TRACKING_CONFIDENCE_NONE);
			}
		}
	}
}

bool OpenXRMlSpatialAnchorsExtension::create_anchor_from_pose(const Transform3D &p_transform, AnchorCreatedCallback p_callback, void *p_userdata) {
	ERR_FAIL_COND_V(!ml_spatial_anchors_ext, false);

	Quaternion quat(p_transform.basis);
	Vector3 pos = p_transform.origin;

	XrSpatialAnchorsCreateInfoFromPoseML create_info = {
		XR_TYPE_SPATIAL_ANCHORS_CREATE_INFO_FROM_POSE_ML,
		nullptr,
		reinterpret_cast<XrSpace>(get_openxr_api()->get_play_space()),
		{
				{ static_cast<float>(quat.x), static_cast<float>(quat.y), static_cast<float>(quat.z), static_cast<float>(quat.w) },
				{ static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z) },
		},
		get_openxr_api()->get_predicted_display_time(),
	};

	XrFutureEXT future = XR_NULL_FUTURE_EXT;
	XrResult result = xrCreateSpatialAnchorsAsyncML(
			SESSION,
			reinterpret_cast<const XrSpatialAnchorsCreateInfoBaseHeaderML *>(&create_info),
			&future);

	if (XR_FAILED(result)) {
		WARN_PRINT("xrCreateSpatialAnchorsAsyncML (from pose) failed.");
		WARN_PRINT(get_openxr_api()->get_error_string(result));
		p_callback(result, 0, nullptr, p_userdata);
		return false;
	}

	pending_creates[future] = PendingCreate(p_callback, p_userdata, 1);
	return true;
}

bool OpenXRMlSpatialAnchorsExtension::create_anchors_from_uuids(uint32_t p_uuid_count, const XrUuidEXT *p_uuids, XrSpatialAnchorsStorageML p_storage, AnchorCreatedCallback p_callback, void *p_userdata) {
	ERR_FAIL_COND_V(!ml_spatial_anchors_ext, false);
	ERR_FAIL_COND_V(p_uuid_count == 0, false);
	ERR_FAIL_COND_V(p_storage == XR_NULL_HANDLE, false);

	XrSpatialAnchorsCreateInfoFromUuidsML create_info = {
		XR_TYPE_SPATIAL_ANCHORS_CREATE_INFO_FROM_UUIDS_ML,
		nullptr,
		p_storage,
		p_uuid_count,
		p_uuids,
	};

	XrFutureEXT future = XR_NULL_FUTURE_EXT;
	XrResult result = xrCreateSpatialAnchorsAsyncML(
			SESSION,
			reinterpret_cast<const XrSpatialAnchorsCreateInfoBaseHeaderML *>(&create_info),
			&future);

	if (XR_FAILED(result)) {
		WARN_PRINT("xrCreateSpatialAnchorsAsyncML (from UUIDs) failed.");
		WARN_PRINT(get_openxr_api()->get_error_string(result));
		p_callback(result, 0, nullptr, p_userdata);
		return false;
	}

	pending_creates[future] = PendingCreate(p_callback, p_userdata, p_uuid_count);
	return true;
}

bool OpenXRMlSpatialAnchorsExtension::destroy_space(const XrSpace &p_space) {
	return XR_SUCCEEDED(xrDestroySpace(p_space));
}

void OpenXRMlSpatialAnchorsExtension::track_entity(const StringName &p_name, const XrSpace &p_space) {
	tracked_entities[p_name] = TrackedEntity(p_space);
}

void OpenXRMlSpatialAnchorsExtension::untrack_entity(const StringName &p_name) {
	TrackedEntity *entity = tracked_entities.getptr(p_name);
	if (entity) {
		if (entity->tracker.is_valid()) {
			XRServer::get_singleton()->remove_tracker(entity->tracker);
			entity->tracker.unref();
		}
		if (entity->space != XR_NULL_HANDLE) {
			xrDestroySpace(entity->space);
		}
		tracked_entities.erase(p_name);
	}
}

bool OpenXRMlSpatialAnchorsExtension::is_entity_tracked(const StringName &p_name) const {
	return tracked_entities.has(p_name);
}
