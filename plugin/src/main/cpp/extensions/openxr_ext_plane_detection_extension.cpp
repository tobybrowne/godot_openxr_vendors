#include "extensions/openxr_ext_plane_detection_extension.h"

#include <vector>
#include <godot_cpp/classes/open_xrapi_extension.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

OpenXRExtPlaneDetectionExtension *OpenXRExtPlaneDetectionExtension::singleton = nullptr;

OpenXRExtPlaneDetectionExtension *OpenXRExtPlaneDetectionExtension::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(OpenXRExtPlaneDetectionExtension());
	}
	return singleton;
}

OpenXRExtPlaneDetectionExtension::OpenXRExtPlaneDetectionExtension() : OpenXRExtensionWrapper() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "OpenXRExtPlaneDetectionExtension singleton already exists.");
	request_extensions[XR_EXT_PLANE_DETECTION_EXTENSION_NAME] = &plane_detection_ext;
	singleton = this;
}

OpenXRExtPlaneDetectionExtension::~OpenXRExtPlaneDetectionExtension() {
	cleanup();
}

void OpenXRExtPlaneDetectionExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_supported"), &OpenXRExtPlaneDetectionExtension::is_supported);
	ClassDB::bind_method(D_METHOD("begin_detection"), &OpenXRExtPlaneDetectionExtension::begin_detection);
	ClassDB::bind_method(D_METHOD("get_state"), &OpenXRExtPlaneDetectionExtension::get_state);
	ClassDB::bind_method(D_METHOD("get_planes"), &OpenXRExtPlaneDetectionExtension::get_planes);

	BIND_ENUM_CONSTANT(ORIENTATION_HORIZONTAL_UPWARD);
	BIND_ENUM_CONSTANT(ORIENTATION_HORIZONTAL_DOWNWARD);
	BIND_ENUM_CONSTANT(ORIENTATION_VERTICAL);
	BIND_ENUM_CONSTANT(ORIENTATION_ARBITRARY);

	BIND_ENUM_CONSTANT(SEMANTIC_TYPE_UNDEFINED);
	BIND_ENUM_CONSTANT(SEMANTIC_TYPE_CEILING);
	BIND_ENUM_CONSTANT(SEMANTIC_TYPE_FLOOR);
	BIND_ENUM_CONSTANT(SEMANTIC_TYPE_WALL);
	BIND_ENUM_CONSTANT(SEMANTIC_TYPE_PLATFORM);

	BIND_ENUM_CONSTANT(STATE_NONE);
	BIND_ENUM_CONSTANT(STATE_PENDING);
	BIND_ENUM_CONSTANT(STATE_DONE);
	BIND_ENUM_CONSTANT(STATE_ERROR);
	BIND_ENUM_CONSTANT(STATE_FATAL);
}

Dictionary OpenXRExtPlaneDetectionExtension::_get_requested_extensions(uint64_t p_xr_version) {
	Dictionary result;
	for (auto &ext : request_extensions) {
		uint64_t value = reinterpret_cast<uint64_t>(ext.value);
		result[ext.key] = (Variant)value;
	}
	return result;
}

void OpenXRExtPlaneDetectionExtension::_on_instance_created(uint64_t instance) {
	if (plane_detection_ext) {
		if (!initialize_ext((XrInstance)instance)) {
			UtilityFunctions::printerr("OpenXRExtPlaneDetectionExtension: failed to initialize");
			plane_detection_ext = false;
		}
	}
}

void OpenXRExtPlaneDetectionExtension::_on_instance_destroyed() {
	cleanup();
}

void OpenXRExtPlaneDetectionExtension::_on_session_created(uint64_t session) {
	cached_session = (XrSession)session;
}

void OpenXRExtPlaneDetectionExtension::_on_session_destroyed() {
	if (plane_detector != XR_NULL_HANDLE) {
		xrDestroyPlaneDetectorEXT(plane_detector);
		plane_detector = XR_NULL_HANDLE;
	}
	cached_session = XR_NULL_HANDLE;
}

void OpenXRExtPlaneDetectionExtension::_on_state_focused() {
	if (!plane_detection_ext || cached_session == XR_NULL_HANDLE) {
		return;
	}
	if (plane_detector != XR_NULL_HANDLE) {
		return; // already created
	}

	XrPlaneDetectorCreateInfoEXT create_info = {
		XR_TYPE_PLANE_DETECTOR_CREATE_INFO_EXT,
		nullptr,
		0, // flags
	};

	XrResult result = xrCreatePlaneDetectorEXT(cached_session, &create_info, &plane_detector);
	if (XR_FAILED(result)) {
		UtilityFunctions::printerr("OpenXRExtPlaneDetectionExtension: xrCreatePlaneDetectorEXT failed result=", (int64_t)result);
		plane_detector = XR_NULL_HANDLE;
	} else {
		UtilityFunctions::print("OpenXRExtPlaneDetectionExtension: plane detector created");
	}
}

bool OpenXRExtPlaneDetectionExtension::begin_detection() {
	if (!plane_detection_ext || plane_detector == XR_NULL_HANDLE) {
		return false;
	}

	XrSpace local_space = (XrSpace)get_openxr_api()->get_play_space();
	XrTime  display_time = (XrTime)get_openxr_api()->get_predicted_display_time();

	if (local_space == XR_NULL_HANDLE || display_time == 0) {
		UtilityFunctions::print("OpenXRExtPlaneDetectionExtension: play space not ready yet");
		return false;
	}

	XrPlaneDetectorOrientationEXT orientations[] = {
		XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_UPWARD_EXT,
		XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_DOWNWARD_EXT,
		XR_PLANE_DETECTOR_ORIENTATION_VERTICAL_EXT,
	};

	XrPosef identity_pose = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f} };

	XrPlaneDetectorBeginInfoEXT begin_info = {
		XR_TYPE_PLANE_DETECTOR_BEGIN_INFO_EXT,
		nullptr,
		local_space,
		display_time,
		3,
		orientations,
		0,        // all semantic types
		nullptr,
		100,      // max planes
		0.1f,     // minArea 0.1 m²
		identity_pose,
		{20.0f, 20.0f, 20.0f},  // 20m bounding box centred on play space origin
	};

	XrResult result = xrBeginPlaneDetectionEXT(plane_detector, &begin_info);
	if (XR_FAILED(result)) {
		UtilityFunctions::printerr("OpenXRExtPlaneDetectionExtension: xrBeginPlaneDetectionEXT failed result=", (int64_t)result);
		return false;
	}
	return true;
}

int OpenXRExtPlaneDetectionExtension::get_state() {
	if (!plane_detection_ext || plane_detector == XR_NULL_HANDLE) {
		return STATE_NONE;
	}

	XrPlaneDetectionStateEXT state = XR_PLANE_DETECTION_STATE_NONE_EXT;
	XrResult result = xrGetPlaneDetectionStateEXT(plane_detector, &state);
	if (XR_FAILED(result)) {
		return STATE_ERROR;
	}
	return (int)state;
}

Array OpenXRExtPlaneDetectionExtension::get_planes() {
	Array result;
	if (!plane_detection_ext || plane_detector == XR_NULL_HANDLE) {
		return result;
	}

	XrSpace local_space = (XrSpace)get_openxr_api()->get_play_space();
	XrTime  display_time = (XrTime)get_openxr_api()->get_predicted_display_time();

	if (local_space == XR_NULL_HANDLE || display_time == 0) {
		return result;
	}

	XrPlaneDetectorGetInfoEXT get_info = {
		XR_TYPE_PLANE_DETECTOR_GET_INFO_EXT,
		nullptr,
		local_space,
		display_time,
	};

	// Two-call: get count first.
	XrPlaneDetectorLocationsEXT locations = {
		XR_TYPE_PLANE_DETECTOR_LOCATIONS_EXT,
		nullptr,
		0,
		0,
		nullptr,
	};

	XrResult xr_result = xrGetPlaneDetectionsEXT(plane_detector, &get_info, &locations);
	UtilityFunctions::print("OpenXRExtPlaneDetectionExtension: get_planes count query result=", (int64_t)xr_result, " count=", (int64_t)locations.planeLocationCountOutput);
	if (XR_FAILED(xr_result) || locations.planeLocationCountOutput == 0) {
		return result;
	}

	std::vector<XrPlaneDetectorLocationEXT> plane_locations(locations.planeLocationCountOutput,
		{ XR_TYPE_PLANE_DETECTOR_LOCATION_EXT });
	locations.planeLocationCapacityInput = locations.planeLocationCountOutput;
	locations.planeLocations = plane_locations.data();

	xr_result = xrGetPlaneDetectionsEXT(plane_detector, &get_info, &locations);
	if (XR_FAILED(xr_result)) {
		return result;
	}

	for (uint32_t i = 0; i < locations.planeLocationCountOutput; i++) {
		const XrPlaneDetectorLocationEXT &loc = plane_locations[i];

		// Skip planes without a valid pose.
		if (!(loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) ||
			!(loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
			continue;
		}

		const XrPosef &pose = loc.pose;
		Transform3D transform;
		transform.basis = Basis(Quaternion(
			pose.orientation.x,
			pose.orientation.y,
			pose.orientation.z,
			pose.orientation.w));
		transform.origin = Vector3(pose.position.x, pose.position.y, pose.position.z);

		Dictionary entry;
		entry["transform"]     = transform;
		entry["width"]         = (float)loc.extents.width;
		entry["height"]        = (float)loc.extents.height;
		entry["orientation"]   = (int)loc.orientation;
		entry["semantic_type"] = (int)loc.semanticType;
		result.append(entry);
	}

	return result;
}

bool OpenXRExtPlaneDetectionExtension::initialize_ext(const XrInstance &p_instance) {
	GDEXTENSION_INIT_XR_FUNC_V(xrCreatePlaneDetectorEXT);
	GDEXTENSION_INIT_XR_FUNC_V(xrDestroyPlaneDetectorEXT);
	GDEXTENSION_INIT_XR_FUNC_V(xrBeginPlaneDetectionEXT);
	GDEXTENSION_INIT_XR_FUNC_V(xrGetPlaneDetectionStateEXT);
	GDEXTENSION_INIT_XR_FUNC_V(xrGetPlaneDetectionsEXT);
	return true;
}

void OpenXRExtPlaneDetectionExtension::cleanup() {
	plane_detection_ext = false;
}
