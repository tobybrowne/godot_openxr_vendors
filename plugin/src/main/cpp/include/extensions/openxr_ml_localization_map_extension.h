/**************************************************************************/
/*  openxr_ml_localization_map_extension.h                               */
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

using namespace godot;

// Wrapper for XR_ML_localization_map (Magic Leap 2).
// Subscribes to localization change events and exposes the current
// localization state, confidence, and error flags to GDScript.
class OpenXRMlLocalizationMapExtension : public OpenXRExtensionWrapper {
	GDCLASS(OpenXRMlLocalizationMapExtension, OpenXRExtensionWrapper);

public:
	// Mirrors XrLocalizationMapStateML
	enum LocalizationState {
		LOCALIZATION_STATE_NOT_LOCALIZED = XR_LOCALIZATION_MAP_STATE_NOT_LOCALIZED_ML,
		LOCALIZATION_STATE_LOCALIZED = XR_LOCALIZATION_MAP_STATE_LOCALIZED_ML,
		LOCALIZATION_STATE_LOCALIZATION_PENDING = XR_LOCALIZATION_MAP_STATE_LOCALIZATION_PENDING_ML,
		LOCALIZATION_STATE_LOCALIZATION_SLEEPING_BEFORE_RETRY = XR_LOCALIZATION_MAP_STATE_LOCALIZATION_SLEEPING_BEFORE_RETRY_ML,
	};

	// Mirrors XrLocalizationMapConfidenceML
	enum LocalizationConfidence {
		LOCALIZATION_CONFIDENCE_POOR = XR_LOCALIZATION_MAP_CONFIDENCE_POOR_ML,
		LOCALIZATION_CONFIDENCE_FAIR = XR_LOCALIZATION_MAP_CONFIDENCE_FAIR_ML,
		LOCALIZATION_CONFIDENCE_GOOD = XR_LOCALIZATION_MAP_CONFIDENCE_GOOD_ML,
		LOCALIZATION_CONFIDENCE_EXCELLENT = XR_LOCALIZATION_MAP_CONFIDENCE_EXCELLENT_ML,
	};

	Dictionary _get_requested_extensions(uint64_t p_xr_version) override;
	void _on_instance_created(uint64_t instance) override;
	void _on_instance_destroyed() override;
	void _on_session_created(uint64_t session) override;
	void _on_state_focused() override;
	void _on_session_destroyed() override;
	bool _on_event_polled(const void *p_event) override;

	bool is_localization_map_supported() const { return ml_localization_map_ext; }
	int get_localization_state() const { return (int)current_state; }
	int get_localization_confidence() const { return (int)current_confidence; }
	int get_localization_error_flags() const { return (int)current_error_flags; }
	String get_localization_map_uuid() const;
	Array get_localization_maps() const;

	static OpenXRMlLocalizationMapExtension *get_singleton();

	OpenXRMlLocalizationMapExtension();
	~OpenXRMlLocalizationMapExtension();

protected:
	static void _bind_methods();

private:
	EXT_PROTO_XRRESULT_FUNC2(xrEnableLocalizationEventsML,
			(XrSession), session,
			(const XrLocalizationEnableEventsInfoML *), info)

	EXT_PROTO_XRRESULT_FUNC5(xrQueryLocalizationMapsML,
			(XrSession), session,
			(const XrLocalizationMapQueryInfoBaseHeaderML *), query_info,
			(uint32_t), map_capacity_input,
			(uint32_t *), map_count_output,
			(XrLocalizationMapML *), maps)

	bool initialize_ml_localization_map_extension(const XrInstance &p_instance);
	void cleanup();

	XrSession cached_session = XR_NULL_HANDLE;
	XrLocalizationMapStateML current_state = XR_LOCALIZATION_MAP_STATE_NOT_LOCALIZED_ML;
	XrLocalizationMapConfidenceML current_confidence = XR_LOCALIZATION_MAP_CONFIDENCE_POOR_ML;
	XrLocalizationMapErrorFlagsML current_error_flags = 0;
	XrUuidEXT current_map_uuid = {};

	static OpenXRMlLocalizationMapExtension *singleton;

	bool ml_localization_map_ext = false;
	HashMap<String, bool *> request_extensions;
};

VARIANT_ENUM_CAST(OpenXRMlLocalizationMapExtension::LocalizationState);
VARIANT_ENUM_CAST(OpenXRMlLocalizationMapExtension::LocalizationConfidence);
