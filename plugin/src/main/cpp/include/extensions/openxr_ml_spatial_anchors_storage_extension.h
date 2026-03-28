/**************************************************************************/
/*  openxr_ml_spatial_anchors_storage_extension.h                        */
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
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/vector.hpp>

#include "util.h"

using namespace godot;

// Wrapper for XR_ML_spatial_anchors_storage (Magic Leap 2).
// Manages the XrSpatialAnchorsStorageML handle and async publish/delete operations.
// Uses XR_EXT_future for polling (same extension as core anchors wrapper).
class OpenXRMlSpatialAnchorsStorageExtension : public OpenXRExtensionWrapper {
	GDCLASS(OpenXRMlSpatialAnchorsStorageExtension, OpenXRExtensionWrapper);

public:
	Dictionary _get_requested_extensions(uint64_t p_xr_version) override;
	void _on_instance_created(uint64_t instance) override;
	void _on_instance_destroyed() override;
	void _on_process() override;

	bool is_spatial_anchors_storage_supported() const { return ml_spatial_anchors_storage_ext; }

	// Returns the storage handle, creating it lazily on first call.
	// Returns XR_NULL_HANDLE if the extension is not supported or session is unavailable.
	XrSpatialAnchorsStorageML get_storage();

	// Publish (persist) anchors to the ML2 runtime storage.
	// p_callback is called with the resulting UUIDs on success.
	typedef void (*PublishCompleteCallback)(XrResult p_result, uint32_t p_uuid_count, const XrUuidEXT *p_uuids, void *p_userdata);
	bool publish_anchors(uint32_t p_anchor_count, const XrSpace *p_anchors, PublishCompleteCallback p_callback, void *p_userdata);

	// Delete persisted anchors by UUID.
	typedef void (*DeleteCompleteCallback)(XrResult p_result, void *p_userdata);
	bool delete_anchors(uint32_t p_uuid_count, const XrUuidEXT *p_uuids, DeleteCompleteCallback p_callback, void *p_userdata);

	static OpenXRMlSpatialAnchorsStorageExtension *get_singleton();

	OpenXRMlSpatialAnchorsStorageExtension();
	~OpenXRMlSpatialAnchorsStorageExtension();

protected:
	static void _bind_methods();

private:
	EXT_PROTO_XRRESULT_FUNC3(xrCreateSpatialAnchorsStorageML,
			(XrSession), session,
			(const XrSpatialAnchorsCreateStorageInfoML *), createInfo,
			(XrSpatialAnchorsStorageML *), storage)

	EXT_PROTO_XRRESULT_FUNC1(xrDestroySpatialAnchorsStorageML,
			(XrSpatialAnchorsStorageML), storage)

	EXT_PROTO_XRRESULT_FUNC3(xrPublishSpatialAnchorsAsyncML,
			(XrSpatialAnchorsStorageML), storage,
			(const XrSpatialAnchorsPublishInfoML *), publishInfo,
			(XrFutureEXT *), future)

	EXT_PROTO_XRRESULT_FUNC3(xrPublishSpatialAnchorsCompleteML,
			(XrSpatialAnchorsStorageML), storage,
			(XrFutureEXT), future,
			(XrSpatialAnchorsPublishCompletionML *), completion)

	EXT_PROTO_XRRESULT_FUNC3(xrDeleteSpatialAnchorsAsyncML,
			(XrSpatialAnchorsStorageML), storage,
			(const XrSpatialAnchorsDeleteInfoML *), deleteInfo,
			(XrFutureEXT *), future)

	EXT_PROTO_XRRESULT_FUNC3(xrDeleteSpatialAnchorsCompleteML,
			(XrSpatialAnchorsStorageML), storage,
			(XrFutureEXT), future,
			(XrSpatialAnchorsDeleteCompletionML *), completion)

	// XR_EXT_future (shared with core anchors extension — both init independently)
	EXT_PROTO_XRRESULT_FUNC3(xrPollFutureEXT,
			(XrInstance), instance,
			(const XrFuturePollInfoEXT *), pollInfo,
			(XrFuturePollResultEXT *), pollResult)

	bool initialize_ml_spatial_anchors_storage_extension(const XrInstance &p_instance);

	struct PendingPublish {
		PublishCompleteCallback callback = nullptr;
		void *userdata = nullptr;
		uint32_t expected_count = 0;

		PendingPublish() {}
		PendingPublish(PublishCompleteCallback p_callback, void *p_userdata, uint32_t p_count) :
				callback(p_callback), userdata(p_userdata), expected_count(p_count) {}
	};
	HashMap<XrFutureEXT, PendingPublish> pending_publishes;

	struct PendingDelete {
		DeleteCompleteCallback callback = nullptr;
		void *userdata = nullptr;

		PendingDelete() {}
		PendingDelete(DeleteCompleteCallback p_callback, void *p_userdata) :
				callback(p_callback), userdata(p_userdata) {}
	};
	HashMap<XrFutureEXT, PendingDelete> pending_deletes;

	XrInstance xr_instance = XR_NULL_HANDLE;
	XrSpatialAnchorsStorageML storage_handle = XR_NULL_HANDLE;

	void cleanup();

	static OpenXRMlSpatialAnchorsStorageExtension *singleton;

	bool ml_spatial_anchors_storage_ext = false;
	HashMap<String, bool *> request_extensions;
};
