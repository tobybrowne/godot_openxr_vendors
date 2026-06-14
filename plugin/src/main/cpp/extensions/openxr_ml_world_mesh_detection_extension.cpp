/**************************************************************************/
/*  openxr_ml_world_mesh_detection_extension.cpp                         */
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

#include "extensions/openxr_ml_world_mesh_detection_extension.h"

#include <godot_cpp/classes/open_xrapi_extension.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

using namespace godot;

OpenXRMlWorldMeshDetectionExtension *OpenXRMlWorldMeshDetectionExtension::singleton = nullptr;

OpenXRMlWorldMeshDetectionExtension *OpenXRMlWorldMeshDetectionExtension::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(OpenXRMlWorldMeshDetectionExtension());
	}
	return singleton;
}

OpenXRMlWorldMeshDetectionExtension::OpenXRMlWorldMeshDetectionExtension() :
		OpenXRExtensionWrapper() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "An OpenXRMlWorldMeshDetectionExtension singleton already exists.");

	request_extensions[XR_ML_WORLD_MESH_DETECTION_EXTENSION_NAME] = &world_mesh_ext;
	request_extensions[XR_EXT_FUTURE_EXTENSION_NAME] = &ext_future_ext;

	singleton = this;
}

OpenXRMlWorldMeshDetectionExtension::~OpenXRMlWorldMeshDetectionExtension() {
	cleanup();
}

void OpenXRMlWorldMeshDetectionExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &OpenXRMlWorldMeshDetectionExtension::is_available);
	ClassDB::bind_method(D_METHOD("start", "bounding_extents", "lod", "flags"), &OpenXRMlWorldMeshDetectionExtension::start);
	ClassDB::bind_method(D_METHOD("stop"), &OpenXRMlWorldMeshDetectionExtension::stop);
	ClassDB::bind_method(D_METHOD("is_running"), &OpenXRMlWorldMeshDetectionExtension::is_running);

	ClassDB::bind_method(D_METHOD("set_bounding_extents", "extents"), &OpenXRMlWorldMeshDetectionExtension::set_bounding_extents);
	ClassDB::bind_method(D_METHOD("get_bounding_extents"), &OpenXRMlWorldMeshDetectionExtension::get_bounding_extents);
	ClassDB::bind_method(D_METHOD("set_lod", "lod"), &OpenXRMlWorldMeshDetectionExtension::set_lod);
	ClassDB::bind_method(D_METHOD("get_lod"), &OpenXRMlWorldMeshDetectionExtension::get_lod);
	ClassDB::bind_method(D_METHOD("set_flags", "flags"), &OpenXRMlWorldMeshDetectionExtension::set_flags);
	ClassDB::bind_method(D_METHOD("get_flags"), &OpenXRMlWorldMeshDetectionExtension::get_flags);
	ClassDB::bind_method(D_METHOD("set_fill_hole_length", "length"), &OpenXRMlWorldMeshDetectionExtension::set_fill_hole_length);
	ClassDB::bind_method(D_METHOD("get_fill_hole_length"), &OpenXRMlWorldMeshDetectionExtension::get_fill_hole_length);
	ClassDB::bind_method(D_METHOD("set_disconnected_component_area", "area"), &OpenXRMlWorldMeshDetectionExtension::set_disconnected_component_area);
	ClassDB::bind_method(D_METHOD("get_disconnected_component_area"), &OpenXRMlWorldMeshDetectionExtension::get_disconnected_component_area);

	ClassDB::bind_method(D_METHOD("set_min_query_interval_ms", "ms"), &OpenXRMlWorldMeshDetectionExtension::set_min_query_interval_ms);
	ClassDB::bind_method(D_METHOD("get_min_query_interval_ms"), &OpenXRMlWorldMeshDetectionExtension::get_min_query_interval_ms);

	// Signal emitted per block with mesh data.
	// vertices: PackedVector3Array, indices: PackedInt32Array, normals: PackedVector3Array
	ADD_SIGNAL(MethodInfo("mesh_block_updated",
			PropertyInfo(Variant::STRING, "block_id"),
			PropertyInfo(Variant::INT, "status"),
			PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "vertices"),
			PropertyInfo(Variant::PACKED_INT32_ARRAY, "indices"),
			PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "normals")));

	ADD_SIGNAL(MethodInfo("mesh_block_deleted",
			PropertyInfo(Variant::STRING, "block_id")));

	ADD_SIGNAL(MethodInfo("meshing_error",
			PropertyInfo(Variant::STRING, "error")));

	BIND_ENUM_CONSTANT(MESH_LOD_MINIMUM);
	BIND_ENUM_CONSTANT(MESH_LOD_MEDIUM);
	BIND_ENUM_CONSTANT(MESH_LOD_MAXIMUM);

	BIND_ENUM_CONSTANT(BLOCK_STATUS_NEW);
	BIND_ENUM_CONSTANT(BLOCK_STATUS_UPDATED);
	BIND_ENUM_CONSTANT(BLOCK_STATUS_DELETED);
	BIND_ENUM_CONSTANT(BLOCK_STATUS_UNCHANGED);

	BIND_ENUM_CONSTANT(BLOCK_RESULT_SUCCESS);
	BIND_ENUM_CONSTANT(BLOCK_RESULT_FAILED);
	BIND_ENUM_CONSTANT(BLOCK_RESULT_PENDING);
	BIND_ENUM_CONSTANT(BLOCK_RESULT_PARTIAL_UPDATE);

	BIND_ENUM_CONSTANT(FLAG_POINT_CLOUD);
	BIND_ENUM_CONSTANT(FLAG_COMPUTE_NORMALS);
	BIND_ENUM_CONSTANT(FLAG_COMPUTE_CONFIDENCE);
	BIND_ENUM_CONSTANT(FLAG_PLANARIZE);
	BIND_ENUM_CONSTANT(FLAG_REMOVE_MESH_SKIRT);
	BIND_ENUM_CONSTANT(FLAG_INDEX_ORDER_CW);
}

// ── OpenXR lifecycle ─────────────────────────────────────────────────────────

Dictionary OpenXRMlWorldMeshDetectionExtension::_get_requested_extensions(uint64_t p_xr_version) {
	Dictionary result;
	for (auto &ext : request_extensions) {
		uint64_t value = reinterpret_cast<uint64_t>(ext.value);
		result[ext.key] = (Variant)value;
	}
	return result;
}

void OpenXRMlWorldMeshDetectionExtension::_on_instance_created(uint64_t instance) {
	xr_instance = (XrInstance)instance;
	if (world_mesh_ext && ext_future_ext) {
		bool result = initialize_extension(xr_instance);
		if (!result) {
			UtilityFunctions::print("Failed to initialize XR_ML_world_mesh_detection extension");
			world_mesh_ext = false;
		}
	}
}

void OpenXRMlWorldMeshDetectionExtension::_on_instance_destroyed() {
	cleanup();
}

void OpenXRMlWorldMeshDetectionExtension::_on_session_created(uint64_t session) {
	cached_session = (XrSession)session;

	XrReferenceSpaceCreateInfo create_info = {
		XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
		nullptr,
		XR_REFERENCE_SPACE_TYPE_VIEW,
		{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } },
	};
	XrResult result = xrCreateReferenceSpace(cached_session, &create_info, &view_space);
	if (XR_FAILED(result)) {
		UtilityFunctions::printerr("WorldMesh: failed to create view space: ", (int64_t)result);
		view_space = XR_NULL_HANDLE;
	}
}

void OpenXRMlWorldMeshDetectionExtension::_on_session_destroyed() {
	stop();
	if (view_space != XR_NULL_HANDLE) {
		xrDestroySpace(view_space);
		view_space = XR_NULL_HANDLE;
	}
	cached_session = XR_NULL_HANDLE;
}

void OpenXRMlWorldMeshDetectionExtension::_on_process() {
	if (!running || !world_mesh_ext || xr_instance == XR_NULL_HANDLE) {
		return;
	}

	switch (phase) {
		case PHASE_IDLE: {
			auto now = std::chrono::steady_clock::now();
			auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_query_start).count();
			if (elapsed_ms >= (int64_t)min_query_interval_ms) {
				request_mesh_states();
			}
			break;
		}

		case PHASE_POLLING_STATES: {
			XrFuturePollInfoEXT poll_info = { XR_TYPE_FUTURE_POLL_INFO_EXT, nullptr, pending_future };
			XrFuturePollResultEXT poll_result = { XR_TYPE_FUTURE_POLL_RESULT_EXT, nullptr, XR_FUTURE_STATE_PENDING_EXT };

			XrResult result = xrPollFutureEXT(xr_instance, &poll_info, &poll_result);
			if (XR_FAILED(result)) {
				emit_signal("meshing_error", String("poll_states_failed"));
				phase = PHASE_IDLE;
				break;
			}
			if (poll_result.state == XR_FUTURE_STATE_READY_EXT) {
				complete_mesh_states();
			}
			break;
		}

		case PHASE_POLLING_MESH: {
			XrFuturePollInfoEXT poll_info = { XR_TYPE_FUTURE_POLL_INFO_EXT, nullptr, pending_future };
			XrFuturePollResultEXT poll_result = { XR_TYPE_FUTURE_POLL_RESULT_EXT, nullptr, XR_FUTURE_STATE_PENDING_EXT };

			XrResult result = xrPollFutureEXT(xr_instance, &poll_info, &poll_result);
			if (XR_FAILED(result)) {
				emit_signal("meshing_error", String("poll_mesh_failed"));
				phase = PHASE_IDLE;
				break;
			}
			if (poll_result.state == XR_FUTURE_STATE_READY_EXT) {
				complete_mesh_data();
			}
			break;
		}
	}
}

// ── GDScript API ─────────────────────────────────────────────────────────────

bool OpenXRMlWorldMeshDetectionExtension::start(Vector3 p_bounding_extents, int p_lod, int p_flags) {
	if (!world_mesh_ext || cached_session == XR_NULL_HANDLE) {
		emit_signal("meshing_error", String("extension_not_available"));
		return false;
	}

	if (running) {
		stop();
	}

	bounding_extents = p_bounding_extents;
	lod = (XrWorldMeshDetectorLodML)p_lod;
	flags = (XrWorldMeshDetectorFlagsML)p_flags;

	XrWorldMeshDetectorCreateInfoML create_info = {
		XR_TYPE_WORLD_MESH_DETECTOR_CREATE_INFO_ML,
		nullptr,
	};

	XrResult result = xrCreateWorldMeshDetectorML(cached_session, &create_info, &detector);
	if (XR_FAILED(result)) {
		UtilityFunctions::printerr("WorldMesh: xrCreateWorldMeshDetectorML failed: ", (int64_t)result);
		emit_signal("meshing_error", String("create_detector_failed"));
		return false;
	}

	running = true;
	phase = PHASE_IDLE;
	return true;
}

void OpenXRMlWorldMeshDetectionExtension::stop() {
	if (!running) {
		return;
	}

	if (pending_future != XR_NULL_HANDLE) {
		XrFutureCancelInfoEXT cancel_info = { XR_TYPE_FUTURE_CANCEL_INFO_EXT, nullptr, pending_future };
		xrCancelFutureEXT(xr_instance, &cancel_info);
	}

	running = false;
	phase = PHASE_IDLE;
	pending_future = XR_NULL_HANDLE;

	free_mesh_buffer();

	if (detector != XR_NULL_HANDLE && world_mesh_ext) {
		xrDestroyWorldMeshDetectorML(detector);
		detector = XR_NULL_HANDLE;
	}

	block_states.clear();
}

// ── State machine helpers ────────────────────────────────────────────────────

void OpenXRMlWorldMeshDetectionExtension::request_mesh_states() {
	XrSpace base_space = (view_space != XR_NULL_HANDLE)
			? view_space
			: reinterpret_cast<XrSpace>(get_openxr_api()->get_play_space());

	XrWorldMeshStateRequestInfoML request_info = {
		XR_TYPE_WORLD_MESH_STATE_REQUEST_INFO_ML,
		nullptr,
		base_space,
		get_openxr_api()->get_predicted_display_time(),
		{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } },  // identity pose — centred on view space (user's head)
		{ bounding_extents.x, bounding_extents.y, bounding_extents.z },
	};

	XrResult result = xrRequestWorldMeshStateAsyncML(detector, &request_info, &pending_future);
	if (XR_FAILED(result)) {
		UtilityFunctions::printerr("WorldMesh: xrRequestWorldMeshStateAsyncML failed: ", (int64_t)result);
		emit_signal("meshing_error", String("request_states_failed"));
		return;
	}

	phase = PHASE_POLLING_STATES;
}

void OpenXRMlWorldMeshDetectionExtension::complete_mesh_states() {
	// First call: capacity=0 to query the block count without consuming the future.
	XrWorldMeshStateRequestCompletionML completion = {
		XR_TYPE_WORLD_MESH_STATE_REQUEST_COMPLETION_ML,
		nullptr,
		XR_SUCCESS,
		0,        // timestamp
		0,        // capacity — 0 means count-only
		0,        // count output
		nullptr,  // no buffer yet
	};

	XrResult result = xrRequestWorldMeshStateCompleteML(detector, pending_future, &completion);
	if (XR_FAILED(result) || XR_FAILED(completion.futureResult)) {
		UtilityFunctions::printerr("WorldMesh: complete_states_failed result=", (int64_t)result, " futureResult=", (int64_t)completion.futureResult);
		emit_signal("meshing_error", String("complete_states_failed"));
		phase = PHASE_IDLE;
		return;
	}

	uint32_t block_count = completion.meshBlockStateCountOutput;
	if (block_count == 0) {
		block_states.clear();
		phase = PHASE_IDLE;
		return;
	}

	// Second call: provide a correctly sized buffer to get the actual block states.
	block_states.resize(block_count);
	for (uint32_t i = 0; i < block_count; i++) {
		block_states[i] = { XR_TYPE_WORLD_MESH_BLOCK_STATE_ML, nullptr };
	}
	completion.meshBlockStateCapacityInput = block_count;
	completion.meshBlockStateCountOutput   = 0;
	completion.meshBlockStates             = block_states.data();

	result = xrRequestWorldMeshStateCompleteML(detector, pending_future, &completion);
	if (XR_FAILED(result) || XR_FAILED(completion.futureResult)) {
		UtilityFunctions::printerr("WorldMesh: complete_states_failed (data) result=", (int64_t)result, " futureResult=", (int64_t)completion.futureResult);
		emit_signal("meshing_error", String("complete_states_failed"));
		block_states.clear();
		phase = PHASE_IDLE;
		return;
	}

	// Emit delete signals for deleted blocks.
	// Collect non-deleted blocks for mesh data request.
	std::vector<XrWorldMeshBlockStateML> blocks_to_fetch;
	for (uint32_t i = 0; i < block_count; i++) {
		if (block_states[i].status == XR_WORLD_MESH_BLOCK_STATUS_DELETED_ML) {
			emit_signal("mesh_block_deleted", uuid_to_string(block_states[i].uuid));
		} else if (block_states[i].status != XR_WORLD_MESH_BLOCK_STATUS_UNCHANGED_ML) {
			blocks_to_fetch.push_back(block_states[i]);
		}
	}

	if (blocks_to_fetch.empty()) {
		phase = PHASE_IDLE;
		return;
	}

	// Save the blocks we need to fetch and request mesh data.
	block_states = blocks_to_fetch;
	request_mesh_data();
}

void OpenXRMlWorldMeshDetectionExtension::request_mesh_data() {
	uint32_t block_count = (uint32_t)block_states.size();

	// Switch to the next buffer slot — the other slot may still be in use by the app.
	mesh_buffer_index = (mesh_buffer_index + 1) % 2;
	uint32_t idx = mesh_buffer_index;

	// Only reallocate if this slot's buffer is too small for the current block count.
	if (block_count > mesh_buffer_max_blocks[idx]) {
		if (mesh_buffer_max_blocks[idx] > 0) {
			xrFreeWorldMeshBufferML(detector, &mesh_buffers[idx]);
			mesh_buffers[idx] = { XR_TYPE_WORLD_MESH_BUFFER_ML, nullptr, 0, nullptr };
		}

		XrWorldMeshBufferRecommendedSizeInfoML size_info = {
			XR_TYPE_WORLD_MESH_BUFFER_RECOMMENDED_SIZE_INFO_ML,
			nullptr,
			block_count,
		};
		XrWorldMeshBufferSizeML buffer_size = {
			XR_TYPE_WORLD_MESH_BUFFER_SIZE_ML,
			nullptr,
			0,
		};

		XrResult result = xrGetWorldMeshBufferRecommendSizeML(detector, &size_info, &buffer_size);
		if (XR_FAILED(result)) {
			UtilityFunctions::printerr("WorldMesh: buffer recommend size failed: ", (int64_t)result);
			emit_signal("meshing_error", String("buffer_size_failed"));
			phase = PHASE_IDLE;
			return;
		}

		result = xrAllocateWorldMeshBufferML(detector, &buffer_size, &mesh_buffers[idx]);
		if (XR_FAILED(result)) {
			UtilityFunctions::printerr("WorldMesh: buffer allocate failed: ", (int64_t)result);
			emit_signal("meshing_error", String("buffer_alloc_failed"));
			mesh_buffer_max_blocks[idx] = 0;
			phase = PHASE_IDLE;
			return;
		}
		mesh_buffer_max_blocks[idx] = block_count;
	}

	// Build block requests.
	std::vector<XrWorldMeshBlockRequestML> block_requests(block_count);
	for (uint32_t i = 0; i < block_count; i++) {
		block_requests[i] = {
			XR_TYPE_WORLD_MESH_BLOCK_REQUEST_ML,
			nullptr,
			block_states[i].uuid,
			lod,
		};
	}

	XrWorldMeshGetInfoML get_info = {
		XR_TYPE_WORLD_MESH_GET_INFO_ML,
		nullptr,
		flags,
		fill_hole_length,
		disconnected_component_area,
		block_count,
		block_requests.data(),
	};

	XrResult result = xrRequestWorldMeshAsyncML(detector, &get_info, &mesh_buffers[idx], &pending_future);
	if (XR_FAILED(result)) {
		UtilityFunctions::printerr("WorldMesh: xrRequestWorldMeshAsyncML failed: ", (int64_t)result);
		emit_signal("meshing_error", String("request_mesh_failed"));
		phase = PHASE_IDLE;
		return;
	}

	phase = PHASE_POLLING_MESH;
}

void OpenXRMlWorldMeshDetectionExtension::complete_mesh_data() {
	uint32_t block_count = (uint32_t)block_states.size();

	std::vector<XrWorldMeshBlockML> mesh_blocks(block_count);
	for (uint32_t i = 0; i < block_count; i++) {
		mesh_blocks[i] = { XR_TYPE_WORLD_MESH_BLOCK_ML };
	}

	XrWorldMeshRequestCompletionInfoML completion_info = {
		XR_TYPE_WORLD_MESH_REQUEST_COMPLETION_INFO_ML,
		nullptr,
		reinterpret_cast<XrSpace>(get_openxr_api()->get_play_space()),
		get_openxr_api()->get_predicted_display_time(),
	};

	XrWorldMeshRequestCompletionML completion = {
		XR_TYPE_WORLD_MESH_REQUEST_COMPLETION_ML,
		nullptr,
		XR_SUCCESS,
		block_count,
		mesh_blocks.data(),
	};

	XrResult result = xrRequestWorldMeshCompleteML(detector, &completion_info, pending_future, &completion);
	if (XR_FAILED(result) || XR_FAILED(completion.futureResult)) {
		emit_signal("meshing_error", String("complete_mesh_failed"));
		phase = PHASE_IDLE;
		return;
	}

	// Convert each block's vertex data to Godot types and emit signals.
	for (uint32_t i = 0; i < completion.blockCount; i++) {
		const XrWorldMeshBlockML &block = mesh_blocks[i];

		// Accept all results — FAILED blocks may still contain valid mesh data.
		if (block.vertexCount == 0 || block.indexCount == 0) {
			continue;
		}

		String block_id = uuid_to_string(block.uuid);

		PackedVector3Array vertices;
		vertices.resize(block.vertexCount);
		for (uint32_t v = 0; v < block.vertexCount; v++) {
			vertices.set(v, Vector3(
					block.vertexBuffer[v].x,
					block.vertexBuffer[v].y,
					block.vertexBuffer[v].z));
		}

		PackedInt32Array indices;
		indices.resize(block.indexCount);
		for (uint32_t idx = 0; idx < block.indexCount; idx++) {
			indices.set(idx, (int32_t)block.indexBuffer[idx]);
		}

		PackedVector3Array normals;
		if (block.normalCount > 0 && block.normalBuffer != nullptr) {
			normals.resize(block.normalCount);
			for (uint32_t n = 0; n < block.normalCount; n++) {
				normals.set(n, Vector3(
						block.normalBuffer[n].x,
						block.normalBuffer[n].y,
						block.normalBuffer[n].z));
			}
		}

		int status = (i < block_states.size())
				? (int)block_states[i].status
				: (int)XR_WORLD_MESH_BLOCK_STATUS_NEW_ML;

		emit_signal("mesh_block_updated", block_id, status, vertices, indices, normals);
	}

	// Buffer is kept alive for reuse — freed only in stop()/cleanup().
	block_states.clear();
	phase = PHASE_IDLE;
}

// ── Utility ──────────────────────────────────────────────────────────────────

void OpenXRMlWorldMeshDetectionExtension::free_mesh_buffer() {
	for (uint32_t i = 0; i < 2; i++) {
		if (mesh_buffer_max_blocks[i] > 0 && detector != XR_NULL_HANDLE) {
			xrFreeWorldMeshBufferML(detector, &mesh_buffers[i]);
			mesh_buffers[i] = { XR_TYPE_WORLD_MESH_BUFFER_ML, nullptr, 0, nullptr };
			mesh_buffer_max_blocks[i] = 0;
		}
	}
}

String OpenXRMlWorldMeshDetectionExtension::uuid_to_string(const XrUuidEXT &uuid) const {
	const uint8_t *d = uuid.data;
	char buf[37];
	snprintf(buf, sizeof(buf),
			"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
			d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
	return String(buf);
}

bool OpenXRMlWorldMeshDetectionExtension::initialize_extension(const XrInstance &p_instance) {
	GDEXTENSION_INIT_XR_FUNC_V(xrCreateWorldMeshDetectorML);
	GDEXTENSION_INIT_XR_FUNC_V(xrDestroyWorldMeshDetectorML);
	GDEXTENSION_INIT_XR_FUNC_V(xrRequestWorldMeshStateAsyncML);
	GDEXTENSION_INIT_XR_FUNC_V(xrRequestWorldMeshStateCompleteML);
	GDEXTENSION_INIT_XR_FUNC_V(xrGetWorldMeshBufferRecommendSizeML);
	GDEXTENSION_INIT_XR_FUNC_V(xrAllocateWorldMeshBufferML);
	GDEXTENSION_INIT_XR_FUNC_V(xrFreeWorldMeshBufferML);
	GDEXTENSION_INIT_XR_FUNC_V(xrRequestWorldMeshAsyncML);
	GDEXTENSION_INIT_XR_FUNC_V(xrRequestWorldMeshCompleteML);
	GDEXTENSION_INIT_XR_FUNC_V(xrPollFutureEXT);
	GDEXTENSION_INIT_XR_FUNC_V(xrCancelFutureEXT);
	GDEXTENSION_INIT_XR_FUNC_V(xrCreateReferenceSpace);
	GDEXTENSION_INIT_XR_FUNC_V(xrDestroySpace);
	return true;
}

void OpenXRMlWorldMeshDetectionExtension::cleanup() {
	stop();
	world_mesh_ext = false;
	ext_future_ext = false;
	xr_instance = XR_NULL_HANDLE;
}
