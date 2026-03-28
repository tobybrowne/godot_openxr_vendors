#pragma once

#include <openxr/openxr.h>
#include <godot_cpp/classes/open_xr_extension_wrapper.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include "util.h"

using namespace godot;

class OpenXRExtPlaneDetectionExtension : public OpenXRExtensionWrapper {
	GDCLASS(OpenXRExtPlaneDetectionExtension, OpenXRExtensionWrapper);

public:
	enum PlaneOrientation {
		ORIENTATION_HORIZONTAL_UPWARD  = XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_UPWARD_EXT,
		ORIENTATION_HORIZONTAL_DOWNWARD = XR_PLANE_DETECTOR_ORIENTATION_HORIZONTAL_DOWNWARD_EXT,
		ORIENTATION_VERTICAL           = XR_PLANE_DETECTOR_ORIENTATION_VERTICAL_EXT,
		ORIENTATION_ARBITRARY          = XR_PLANE_DETECTOR_ORIENTATION_ARBITRARY_EXT,
	};

	enum PlaneSemanticType {
		SEMANTIC_TYPE_UNDEFINED = XR_PLANE_DETECTOR_SEMANTIC_TYPE_UNDEFINED_EXT,
		SEMANTIC_TYPE_CEILING   = XR_PLANE_DETECTOR_SEMANTIC_TYPE_CEILING_EXT,
		SEMANTIC_TYPE_FLOOR     = XR_PLANE_DETECTOR_SEMANTIC_TYPE_FLOOR_EXT,
		SEMANTIC_TYPE_WALL      = XR_PLANE_DETECTOR_SEMANTIC_TYPE_WALL_EXT,
		SEMANTIC_TYPE_PLATFORM  = XR_PLANE_DETECTOR_SEMANTIC_TYPE_PLATFORM_EXT,
	};

	enum PlaneDetectionState {
		STATE_NONE    = XR_PLANE_DETECTION_STATE_NONE_EXT,
		STATE_PENDING = XR_PLANE_DETECTION_STATE_PENDING_EXT,
		STATE_DONE    = XR_PLANE_DETECTION_STATE_DONE_EXT,
		STATE_ERROR   = XR_PLANE_DETECTION_STATE_ERROR_EXT,
		STATE_FATAL   = XR_PLANE_DETECTION_STATE_FATAL_EXT,
	};

	Dictionary _get_requested_extensions(uint64_t p_xr_version) override;
	void _on_instance_created(uint64_t instance) override;
	void _on_instance_destroyed() override;
	void _on_session_created(uint64_t session) override;
	void _on_session_destroyed() override;
	void _on_state_focused() override;

	bool is_supported() const { return plane_detection_ext; }
	bool begin_detection();
	int get_state();
	Array get_planes();

	static OpenXRExtPlaneDetectionExtension *get_singleton();
	OpenXRExtPlaneDetectionExtension();
	~OpenXRExtPlaneDetectionExtension();

protected:
	static void _bind_methods();

private:
	EXT_PROTO_XRRESULT_FUNC3(xrCreatePlaneDetectorEXT,
			(XrSession), session,
			(const XrPlaneDetectorCreateInfoEXT *), create_info,
			(XrPlaneDetectorEXT *), plane_detector)

	EXT_PROTO_XRRESULT_FUNC1(xrDestroyPlaneDetectorEXT,
			(XrPlaneDetectorEXT), plane_detector)

	EXT_PROTO_XRRESULT_FUNC2(xrBeginPlaneDetectionEXT,
			(XrPlaneDetectorEXT), plane_detector,
			(const XrPlaneDetectorBeginInfoEXT *), begin_info)

	EXT_PROTO_XRRESULT_FUNC2(xrGetPlaneDetectionStateEXT,
			(XrPlaneDetectorEXT), plane_detector,
			(XrPlaneDetectionStateEXT *), state)

	EXT_PROTO_XRRESULT_FUNC3(xrGetPlaneDetectionsEXT,
			(XrPlaneDetectorEXT), plane_detector,
			(const XrPlaneDetectorGetInfoEXT *), info,
			(XrPlaneDetectorLocationsEXT *), locations)

	bool initialize_ext(const XrInstance &p_instance);
	void cleanup();

	XrSession cached_session = XR_NULL_HANDLE;
	XrPlaneDetectorEXT plane_detector = XR_NULL_HANDLE;

	bool plane_detection_ext = false;
	HashMap<String, bool *> request_extensions;

	static OpenXRExtPlaneDetectionExtension *singleton;
};

VARIANT_ENUM_CAST(OpenXRExtPlaneDetectionExtension::PlaneOrientation);
VARIANT_ENUM_CAST(OpenXRExtPlaneDetectionExtension::PlaneSemanticType);
VARIANT_ENUM_CAST(OpenXRExtPlaneDetectionExtension::PlaneDetectionState);
