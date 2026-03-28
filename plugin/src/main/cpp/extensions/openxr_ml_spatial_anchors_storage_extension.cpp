/**************************************************************************/
/*  openxr_ml_spatial_anchors_storage_extension.cpp                      */
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

#include "extensions/openxr_ml_spatial_anchors_storage_extension.h"

#include <godot_cpp/classes/open_xrapi_extension.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

OpenXRMlSpatialAnchorsStorageExtension *OpenXRMlSpatialAnchorsStorageExtension::singleton = nullptr;

OpenXRMlSpatialAnchorsStorageExtension *OpenXRMlSpatialAnchorsStorageExtension::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(OpenXRMlSpatialAnchorsStorageExtension());
	}
	return singleton;
}

OpenXRMlSpatialAnchorsStorageExtension::OpenXRMlSpatialAnchorsStorageExtension() :
		OpenXRExtensionWrapper() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "An OpenXRMlSpatialAnchorsStorageExtension singleton already exists.");

	request_extensions[XR_ML_SPATIAL_ANCHORS_STORAGE_EXTENSION_NAME] = &ml_spatial_anchors_storage_ext;

	singleton = this;
}

OpenXRMlSpatialAnchorsStorageExtension::~OpenXRMlSpatialAnchorsStorageExtension() {
	cleanup();
}

void OpenXRMlSpatialAnchorsStorageExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_spatial_anchors_storage_supported"), &OpenXRMlSpatialAnchorsStorageExtension::is_spatial_anchors_storage_supported);
}

void OpenXRMlSpatialAnchorsStorageExtension::cleanup() {
	if (storage_handle != XR_NULL_HANDLE) {
		xrDestroySpatialAnchorsStorageML(storage_handle);
		storage_handle = XR_NULL_HANDLE;
	}
	ml_spatial_anchors_storage_ext = false;
	xr_instance = XR_NULL_HANDLE;
	pending_publishes.clear();
	pending_deletes.clear();
}

Dictionary OpenXRMlSpatialAnchorsStorageExtension::_get_requested_extensions(uint64_t p_xr_version) {
	Dictionary result;
	for (auto &ext : request_extensions) {
		uint64_t value = reinterpret_cast<uint64_t>(ext.value);
		result[ext.key] = (Variant)value;
	}
	return result;
}

void OpenXRMlSpatialAnchorsStorageExtension::_on_instance_created(uint64_t instance) {
	xr_instance = (XrInstance)instance;

	if (ml_spatial_anchors_storage_ext) {
		bool result = initialize_ml_spatial_anchors_storage_extension((XrInstance)instance);
		if (!result) {
			UtilityFunctions::print("Failed to initialize XR_ML_spatial_anchors_storage extension");
			ml_spatial_anchors_storage_ext = false;
		}
	}
}

void OpenXRMlSpatialAnchorsStorageExtension::_on_instance_destroyed() {
	cleanup();
}

bool OpenXRMlSpatialAnchorsStorageExtension::initialize_ml_spatial_anchors_storage_extension(const XrInstance &p_instance) {
	GDEXTENSION_INIT_XR_FUNC_V(xrCreateSpatialAnchorsStorageML);
	GDEXTENSION_INIT_XR_FUNC_V(xrDestroySpatialAnchorsStorageML);
	GDEXTENSION_INIT_XR_FUNC_V(xrPublishSpatialAnchorsAsyncML);
	GDEXTENSION_INIT_XR_FUNC_V(xrPublishSpatialAnchorsCompleteML);
	GDEXTENSION_INIT_XR_FUNC_V(xrDeleteSpatialAnchorsAsyncML);
	GDEXTENSION_INIT_XR_FUNC_V(xrDeleteSpatialAnchorsCompleteML);
	GDEXTENSION_INIT_XR_FUNC_V(xrPollFutureEXT);
	return true;
}

XrSpatialAnchorsStorageML OpenXRMlSpatialAnchorsStorageExtension::get_storage() {
	if (!ml_spatial_anchors_storage_ext) {
		return XR_NULL_HANDLE;
	}

	if (storage_handle != XR_NULL_HANDLE) {
		return storage_handle;
	}

	// Lazily create storage handle — requires an active session.
	XrSpatialAnchorsCreateStorageInfoML create_info = {
		XR_TYPE_SPATIAL_ANCHORS_CREATE_STORAGE_INFO_ML,
		nullptr,
	};

	XrResult result = xrCreateSpatialAnchorsStorageML(SESSION, &create_info, &storage_handle);
	if (XR_FAILED(result)) {
		WARN_PRINT("xrCreateSpatialAnchorsStorageML failed.");
		WARN_PRINT(get_openxr_api()->get_error_string(result));
		storage_handle = XR_NULL_HANDLE;
	}

	return storage_handle;
}

void OpenXRMlSpatialAnchorsStorageExtension::_on_process() {
	if (!ml_spatial_anchors_storage_ext || xr_instance == XR_NULL_HANDLE) {
		return;
	}

	// Poll pending publish futures.
	{
		Vector<XrFutureEXT> completed;
		for (KeyValue<XrFutureEXT, PendingPublish> &E : pending_publishes) {
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
				WARN_PRINT("xrPollFutureEXT failed for pending anchor publish.");
				E.value.callback(result, 0, nullptr, E.value.userdata);
				completed.push_back(E.key);
				continue;
			}

			if (poll_result.state != XR_FUTURE_STATE_READY_EXT) {
				continue;
			}

			Vector<XrUuidEXT> uuids;
			uuids.resize(E.value.expected_count);

			XrSpatialAnchorsPublishCompletionML completion = {
				XR_TYPE_SPATIAL_ANCHORS_PUBLISH_COMPLETION_ML,
				nullptr,
				XR_SUCCESS,
				(uint32_t)uuids.size(),
				uuids.ptrw(),
			};

			result = xrPublishSpatialAnchorsCompleteML(storage_handle, E.key, &completion);
			if (XR_FAILED(result)) {
				WARN_PRINT("xrPublishSpatialAnchorsCompleteML failed.");
				WARN_PRINT(get_openxr_api()->get_error_string(result));
				E.value.callback(result, 0, nullptr, E.value.userdata);
			} else if (XR_FAILED(completion.futureResult)) {
				WARN_PRINT("xrPublishSpatialAnchorsCompleteML async operation failed.");
				WARN_PRINT(get_openxr_api()->get_error_string(completion.futureResult));
				E.value.callback(completion.futureResult, 0, nullptr, E.value.userdata);
			} else {
				E.value.callback(XR_SUCCESS, completion.uuidCount, uuids.ptr(), E.value.userdata);
			}

			completed.push_back(E.key);
		}
		for (XrFutureEXT future : completed) {
			pending_publishes.erase(future);
		}
	}

	// Poll pending delete futures.
	{
		Vector<XrFutureEXT> completed;
		for (KeyValue<XrFutureEXT, PendingDelete> &E : pending_deletes) {
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
				WARN_PRINT("xrPollFutureEXT failed for pending anchor delete.");
				E.value.callback(result, E.value.userdata);
				completed.push_back(E.key);
				continue;
			}

			if (poll_result.state != XR_FUTURE_STATE_READY_EXT) {
				continue;
			}

			XrSpatialAnchorsDeleteCompletionML completion = {
				XR_TYPE_SPATIAL_ANCHORS_DELETE_COMPLETION_ML,
				nullptr,
				XR_SUCCESS,
			};

			result = xrDeleteSpatialAnchorsCompleteML(storage_handle, E.key, &completion);
			if (XR_FAILED(result)) {
				WARN_PRINT("xrDeleteSpatialAnchorsCompleteML failed.");
				E.value.callback(result, E.value.userdata);
			} else {
				E.value.callback(completion.futureResult, E.value.userdata);
			}

			completed.push_back(E.key);
		}
		for (XrFutureEXT future : completed) {
			pending_deletes.erase(future);
		}
	}
}

bool OpenXRMlSpatialAnchorsStorageExtension::publish_anchors(uint32_t p_anchor_count, const XrSpace *p_anchors, PublishCompleteCallback p_callback, void *p_userdata) {
	ERR_FAIL_COND_V(!ml_spatial_anchors_storage_ext, false);

	XrSpatialAnchorsStorageML storage = get_storage();
	ERR_FAIL_COND_V(storage == XR_NULL_HANDLE, false);

	XrSpatialAnchorsPublishInfoML publish_info = {
		XR_TYPE_SPATIAL_ANCHORS_PUBLISH_INFO_ML,
		nullptr,
		p_anchor_count,
		p_anchors,
		0, // expiration = 0 means no expiration
	};

	XrFutureEXT future = XR_NULL_FUTURE_EXT;
	XrResult result = xrPublishSpatialAnchorsAsyncML(storage, &publish_info, &future);
	if (XR_FAILED(result)) {
		WARN_PRINT("xrPublishSpatialAnchorsAsyncML failed.");
		WARN_PRINT(get_openxr_api()->get_error_string(result));
		p_callback(result, 0, nullptr, p_userdata);
		return false;
	}

	pending_publishes[future] = PendingPublish(p_callback, p_userdata, p_anchor_count);
	return true;
}

bool OpenXRMlSpatialAnchorsStorageExtension::delete_anchors(uint32_t p_uuid_count, const XrUuidEXT *p_uuids, DeleteCompleteCallback p_callback, void *p_userdata) {
	ERR_FAIL_COND_V(!ml_spatial_anchors_storage_ext, false);

	XrSpatialAnchorsStorageML storage = get_storage();
	ERR_FAIL_COND_V(storage == XR_NULL_HANDLE, false);

	XrSpatialAnchorsDeleteInfoML delete_info = {
		XR_TYPE_SPATIAL_ANCHORS_DELETE_INFO_ML,
		nullptr,
		p_uuid_count,
		p_uuids,
	};

	XrFutureEXT future = XR_NULL_FUTURE_EXT;
	XrResult result = xrDeleteSpatialAnchorsAsyncML(storage, &delete_info, &future);
	if (XR_FAILED(result)) {
		WARN_PRINT("xrDeleteSpatialAnchorsAsyncML failed.");
		WARN_PRINT(get_openxr_api()->get_error_string(result));
		p_callback(result, p_userdata);
		return false;
	}

	pending_deletes[future] = PendingDelete(p_callback, p_userdata);
	return true;
}
