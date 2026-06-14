/**************************************************************************/
/*  openxr_ml_world_mesh_detection_extension.h                           */
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

#include "util.h"

#include <chrono>
#include <vector>

using namespace godot;

// Wrapper for XR_ML_world_mesh_detection (Magic Leap 2).
// Provides spatial mesh scanning: discovers mesh blocks in a bounding volume,
// then retrieves triangle/vertex data for each block.
class OpenXRMlWorldMeshDetectionExtension : public OpenXRExtensionWrapper {
	GDCLASS(OpenXRMlWorldMeshDetectionExtension, OpenXRExtensionWrapper);

public:
	enum MeshLod {
		MESH_LOD_MINIMUM = XR_WORLD_MESH_DETECTOR_LOD_MINIMUM_ML,
		MESH_LOD_MEDIUM = XR_WORLD_MESH_DETECTOR_LOD_MEDIUM_ML,
		MESH_LOD_MAXIMUM = XR_WORLD_MESH_DETECTOR_LOD_MAXIMUM_ML,
	};

	enum MeshBlockStatus {
		BLOCK_STATUS_NEW = XR_WORLD_MESH_BLOCK_STATUS_NEW_ML,
		BLOCK_STATUS_UPDATED = XR_WORLD_MESH_BLOCK_STATUS_UPDATED_ML,
		BLOCK_STATUS_DELETED = XR_WORLD_MESH_BLOCK_STATUS_DELETED_ML,
		BLOCK_STATUS_UNCHANGED = XR_WORLD_MESH_BLOCK_STATUS_UNCHANGED_ML,
	};

	enum MeshBlockResult {
		BLOCK_RESULT_SUCCESS = XR_WORLD_MESH_BLOCK_RESULT_SUCCESS_ML,
		BLOCK_RESULT_FAILED = XR_WORLD_MESH_BLOCK_RESULT_FAILED_ML,
		BLOCK_RESULT_PENDING = XR_WORLD_MESH_BLOCK_RESULT_PENDING_ML,
		BLOCK_RESULT_PARTIAL_UPDATE = XR_WORLD_MESH_BLOCK_RESULT_PARTIAL_UPDATE_ML,
	};

	enum MeshDetectorFlag {
		FLAG_POINT_CLOUD = XR_WORLD_MESH_DETECTOR_POINT_CLOUD_BIT_ML,
		FLAG_COMPUTE_NORMALS = XR_WORLD_MESH_DETECTOR_COMPUTE_NORMALS_BIT_ML,
		FLAG_COMPUTE_CONFIDENCE = XR_WORLD_MESH_DETECTOR_COMPUTE_CONFIDENCE_BIT_ML,
		FLAG_PLANARIZE = XR_WORLD_MESH_DETECTOR_PLANARIZE_BIT_ML,
		FLAG_REMOVE_MESH_SKIRT = XR_WORLD_MESH_DETECTOR_REMOVE_MESH_SKIRT_BIT_ML,
		FLAG_INDEX_ORDER_CW = XR_WORLD_MESH_DETECTOR_INDEX_ORDER_CW_BIT_ML,
	};

	// OpenXR lifecycle
	Dictionary _get_requested_extensions(uint64_t p_xr_version) override;
	void _on_instance_created(uint64_t instance) override;
	void _on_instance_destroyed() override;
	void _on_session_created(uint64_t session) override;
	void _on_session_destroyed() override;
	void _on_process() override;

	// GDScript API
	bool is_available() const { return world_mesh_ext; }
	bool start(Vector3 p_bounding_extents, int p_lod, int p_flags);
	void stop();
	bool is_running() const { return running; }

	void set_bounding_extents(Vector3 p_extents) { bounding_extents = p_extents; }
	Vector3 get_bounding_extents() const { return bounding_extents; }

	void set_lod(int p_lod) { lod = (XrWorldMeshDetectorLodML)p_lod; }
	int get_lod() const { return (int)lod; }

	void set_flags(int p_flags) { flags = (XrWorldMeshDetectorFlagsML)p_flags; }
	int get_flags() const { return (int)flags; }

	void set_fill_hole_length(float p_length) { fill_hole_length = p_length; }
	float get_fill_hole_length() const { return fill_hole_length; }

	void set_disconnected_component_area(float p_area) { disconnected_component_area = p_area; }
	float get_disconnected_component_area() const { return disconnected_component_area; }

	void set_min_query_interval_ms(uint32_t p_ms) { min_query_interval_ms = p_ms; }
	uint32_t get_min_query_interval_ms() const { return min_query_interval_ms; }

	static OpenXRMlWorldMeshDetectionExtension *get_singleton();

	OpenXRMlWorldMeshDetectionExtension();
	~OpenXRMlWorldMeshDetectionExtension();

protected:
	static void _bind_methods();

private:
	// State machine for the async query loop.
	enum Phase {
		PHASE_IDLE,
		PHASE_POLLING_STATES,
		PHASE_POLLING_MESH,
	};

	// World mesh detection functions
	EXT_PROTO_XRRESULT_FUNC3(xrCreateWorldMeshDetectorML,
			(XrSession), session,
			(const XrWorldMeshDetectorCreateInfoML *), createInfo,
			(XrWorldMeshDetectorML *), detector)

	EXT_PROTO_XRRESULT_FUNC1(xrDestroyWorldMeshDetectorML,
			(XrWorldMeshDetectorML), detector)

	EXT_PROTO_XRRESULT_FUNC3(xrRequestWorldMeshStateAsyncML,
			(XrWorldMeshDetectorML), detector,
			(const XrWorldMeshStateRequestInfoML *), stateRequest,
			(XrFutureEXT *), future)

	EXT_PROTO_XRRESULT_FUNC3(xrRequestWorldMeshStateCompleteML,
			(XrWorldMeshDetectorML), detector,
			(XrFutureEXT), future,
			(XrWorldMeshStateRequestCompletionML *), completion)

	EXT_PROTO_XRRESULT_FUNC3(xrGetWorldMeshBufferRecommendSizeML,
			(XrWorldMeshDetectorML), detector,
			(const XrWorldMeshBufferRecommendedSizeInfoML *), sizeInfo,
			(XrWorldMeshBufferSizeML *), size)

	EXT_PROTO_XRRESULT_FUNC3(xrAllocateWorldMeshBufferML,
			(XrWorldMeshDetectorML), detector,
			(const XrWorldMeshBufferSizeML *), size,
			(XrWorldMeshBufferML *), buffer)

	EXT_PROTO_XRRESULT_FUNC2(xrFreeWorldMeshBufferML,
			(XrWorldMeshDetectorML), detector,
			(const XrWorldMeshBufferML *), buffer)

	EXT_PROTO_XRRESULT_FUNC4(xrRequestWorldMeshAsyncML,
			(XrWorldMeshDetectorML), detector,
			(const XrWorldMeshGetInfoML *), getInfo,
			(XrWorldMeshBufferML *), buffer,
			(XrFutureEXT *), future)

	EXT_PROTO_XRRESULT_FUNC4(xrRequestWorldMeshCompleteML,
			(XrWorldMeshDetectorML), detector,
			(const XrWorldMeshRequestCompletionInfoML *), completionInfo,
			(XrFutureEXT), future,
			(XrWorldMeshRequestCompletionML *), completion)

	// XR_EXT_future
	EXT_PROTO_XRRESULT_FUNC3(xrPollFutureEXT,
			(XrInstance), instance,
			(const XrFuturePollInfoEXT *), pollInfo,
			(XrFuturePollResultEXT *), pollResult)

	EXT_PROTO_XRRESULT_FUNC2(xrCancelFutureEXT,
			(XrInstance), instance,
			(const XrFutureCancelInfoEXT *), cancelInfo)

	EXT_PROTO_XRRESULT_FUNC3(xrCreateReferenceSpace,
			(XrSession), session,
			(const XrReferenceSpaceCreateInfo *), createInfo,
			(XrSpace *), space)

	EXT_PROTO_XRRESULT_FUNC1(xrDestroySpace,
			(XrSpace), space)

	bool initialize_extension(const XrInstance &p_instance);
	void cleanup();

	// Helpers
	String uuid_to_string(const XrUuidEXT &uuid) const;
	void request_mesh_states();
	void complete_mesh_states();
	void request_mesh_data();
	void complete_mesh_data();
	void free_mesh_buffer();

	static OpenXRMlWorldMeshDetectionExtension *singleton;

	bool world_mesh_ext = false;
	bool ext_future_ext = false;
	HashMap<String, bool *> request_extensions;

	XrSession cached_session = XR_NULL_HANDLE;
	XrInstance xr_instance = XR_NULL_HANDLE;
	XrSpace view_space = XR_NULL_HANDLE;
	XrWorldMeshDetectorML detector = XR_NULL_HANDLE;

	// Configuration
	Vector3 bounding_extents = Vector3(10, 10, 10);
	XrWorldMeshDetectorLodML lod = XR_WORLD_MESH_DETECTOR_LOD_MEDIUM_ML;
	XrWorldMeshDetectorFlagsML flags = XR_WORLD_MESH_DETECTOR_COMPUTE_NORMALS_BIT_ML | XR_WORLD_MESH_DETECTOR_REMOVE_MESH_SKIRT_BIT_ML;
	float fill_hole_length = 0.5f;
	float disconnected_component_area = 0.25f;

	// State machine
	bool running = false;
	Phase phase = PHASE_IDLE;
	XrFutureEXT pending_future = XR_NULL_HANDLE;

	// Minimum interval between query cycles to reduce ML2 meshing load
	uint32_t min_query_interval_ms = 500;
	std::chrono::steady_clock::time_point _last_query_start;

	// Block states from the most recent state query
	std::vector<XrWorldMeshBlockStateML> block_states;

	// Double-buffered mesh buffers — kept alive between cycles, only reallocated when block count grows
	XrWorldMeshBufferML mesh_buffers[2] = {
		{ XR_TYPE_WORLD_MESH_BUFFER_ML, nullptr, 0, nullptr },
		{ XR_TYPE_WORLD_MESH_BUFFER_ML, nullptr, 0, nullptr },
	};
	uint32_t mesh_buffer_max_blocks[2] = { 0, 0 };
	uint32_t mesh_buffer_index = 0;
};

VARIANT_ENUM_CAST(OpenXRMlWorldMeshDetectionExtension::MeshLod);
VARIANT_ENUM_CAST(OpenXRMlWorldMeshDetectionExtension::MeshBlockStatus);
VARIANT_ENUM_CAST(OpenXRMlWorldMeshDetectionExtension::MeshBlockResult);
VARIANT_ENUM_CAST(OpenXRMlWorldMeshDetectionExtension::MeshDetectorFlag);
