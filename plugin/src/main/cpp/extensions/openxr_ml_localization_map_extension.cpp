/**************************************************************************/
/*  openxr_ml_localization_map_extension.cpp                             */
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

#include "extensions/openxr_ml_localization_map_extension.h"

#include <vector>

#include <godot_cpp/classes/open_xrapi_extension.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

OpenXRMlLocalizationMapExtension *OpenXRMlLocalizationMapExtension::singleton = nullptr;

OpenXRMlLocalizationMapExtension *OpenXRMlLocalizationMapExtension::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(OpenXRMlLocalizationMapExtension());
	}
	return singleton;
}

OpenXRMlLocalizationMapExtension::OpenXRMlLocalizationMapExtension() :
		OpenXRExtensionWrapper() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "An OpenXRMlLocalizationMapExtension singleton already exists.");

	request_extensions[XR_ML_LOCALIZATION_MAP_EXTENSION_NAME] = &ml_localization_map_ext;

	singleton = this;
}

OpenXRMlLocalizationMapExtension::~OpenXRMlLocalizationMapExtension() {
	cleanup();
}

void OpenXRMlLocalizationMapExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_localization_map_supported"), &OpenXRMlLocalizationMapExtension::is_localization_map_supported);
	ClassDB::bind_method(D_METHOD("get_localization_state"), &OpenXRMlLocalizationMapExtension::get_localization_state);
	ClassDB::bind_method(D_METHOD("get_localization_confidence"), &OpenXRMlLocalizationMapExtension::get_localization_confidence);
	ClassDB::bind_method(D_METHOD("get_localization_error_flags"), &OpenXRMlLocalizationMapExtension::get_localization_error_flags);
	ClassDB::bind_method(D_METHOD("get_localization_map_uuid"), &OpenXRMlLocalizationMapExtension::get_localization_map_uuid);
	ClassDB::bind_method(D_METHOD("get_localization_maps"), &OpenXRMlLocalizationMapExtension::get_localization_maps);
	ClassDB::bind_method(D_METHOD("export_localization_map", "uuid"), &OpenXRMlLocalizationMapExtension::export_localization_map);
	ClassDB::bind_method(D_METHOD("import_localization_map", "data"), &OpenXRMlLocalizationMapExtension::import_localization_map);

	ADD_SIGNAL(MethodInfo("localization_changed",
			PropertyInfo(Variant::INT, "state"),
			PropertyInfo(Variant::INT, "confidence"),
			PropertyInfo(Variant::INT, "error_flags")));

	BIND_ENUM_CONSTANT(LOCALIZATION_STATE_NOT_LOCALIZED);
	BIND_ENUM_CONSTANT(LOCALIZATION_STATE_LOCALIZED);
	BIND_ENUM_CONSTANT(LOCALIZATION_STATE_LOCALIZATION_PENDING);
	BIND_ENUM_CONSTANT(LOCALIZATION_STATE_LOCALIZATION_SLEEPING_BEFORE_RETRY);

	BIND_ENUM_CONSTANT(LOCALIZATION_CONFIDENCE_POOR);
	BIND_ENUM_CONSTANT(LOCALIZATION_CONFIDENCE_FAIR);
	BIND_ENUM_CONSTANT(LOCALIZATION_CONFIDENCE_GOOD);
	BIND_ENUM_CONSTANT(LOCALIZATION_CONFIDENCE_EXCELLENT);
}

void OpenXRMlLocalizationMapExtension::cleanup() {
	ml_localization_map_ext = false;
	current_state = XR_LOCALIZATION_MAP_STATE_NOT_LOCALIZED_ML;
	current_confidence = XR_LOCALIZATION_MAP_CONFIDENCE_POOR_ML;
	current_error_flags = 0;
}

Dictionary OpenXRMlLocalizationMapExtension::_get_requested_extensions(uint64_t p_xr_version) {
	Dictionary result;
	for (auto &ext : request_extensions) {
		uint64_t value = reinterpret_cast<uint64_t>(ext.value);
		result[ext.key] = (Variant)value;
	}
	return result;
}

void OpenXRMlLocalizationMapExtension::_on_instance_created(uint64_t instance) {
	if (ml_localization_map_ext) {
		bool result = initialize_ml_localization_map_extension((XrInstance)instance);
		if (!result) {
			UtilityFunctions::print("Failed to initialize XR_ML_localization_map extension");
			ml_localization_map_ext = false;
		}
	}
}

void OpenXRMlLocalizationMapExtension::_on_instance_destroyed() {
	cleanup();
}

void OpenXRMlLocalizationMapExtension::_on_session_created(uint64_t session) {
	cached_session = (XrSession)session;
}

void OpenXRMlLocalizationMapExtension::_on_state_focused() {
	UtilityFunctions::print("OpenXRMlLocalizationMapExtension: _on_state_focused called, ext=", ml_localization_map_ext, " session=", (uint64_t)cached_session);
	if (!ml_localization_map_ext || cached_session == XR_NULL_HANDLE) {
		return;
	}

	// Called when the session becomes focused (actively tracking). The ML2 spec
	// guarantees that the first xrEnableLocalizationEventsML(XR_TRUE) call fires
	// an immediate XrEventDataLocalizationChangedML event with the current state.
	// Calling from _on_session_created is too early (before xrBeginSession), so
	// no initial event is ever received. Re-calling on re-focus also refreshes state
	// after the headset is removed/replaced.
	XrLocalizationEnableEventsInfoML enable_info = {
		XR_TYPE_LOCALIZATION_ENABLE_EVENTS_INFO_ML,
		nullptr,
		XR_TRUE,
	};

	XrResult result = xrEnableLocalizationEventsML(cached_session, &enable_info);
	UtilityFunctions::print("OpenXRMlLocalizationMapExtension: xrEnableLocalizationEventsML result=", (int64_t)result);
	if (XR_FAILED(result)) {
		WARN_PRINT("xrEnableLocalizationEventsML failed — localization events will not be received.");
		WARN_PRINT(get_openxr_api()->get_error_string(result));
	}
}

void OpenXRMlLocalizationMapExtension::_on_session_destroyed() {
	cached_session = XR_NULL_HANDLE;
	current_state = XR_LOCALIZATION_MAP_STATE_NOT_LOCALIZED_ML;
	current_confidence = XR_LOCALIZATION_MAP_CONFIDENCE_POOR_ML;
	current_error_flags = 0;
}

String OpenXRMlLocalizationMapExtension::get_localization_map_uuid() const {
	const uint8_t *d = current_map_uuid.data;
	char buf[37];
	snprintf(buf, sizeof(buf),
			"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			d[0], d[1], d[2], d[3],
			d[4], d[5],
			d[6], d[7],
			d[8], d[9],
			d[10], d[11], d[12], d[13], d[14], d[15]);
	return String(buf);
}

bool OpenXRMlLocalizationMapExtension::_on_event_polled(const void *p_event) {
	const XrEventDataBaseHeader *event = reinterpret_cast<const XrEventDataBaseHeader *>(p_event);
	UtilityFunctions::print("OpenXRMlLocalizationMapExtension: event polled type=", (int64_t)event->type);
	if (event->type != XR_TYPE_EVENT_DATA_LOCALIZATION_CHANGED_ML) {
		return false;
	}

	const XrEventDataLocalizationChangedML *loc_event =
			reinterpret_cast<const XrEventDataLocalizationChangedML *>(p_event);

	current_state = loc_event->state;
	current_confidence = loc_event->confidence;
	current_error_flags = loc_event->errorFlags;
	current_map_uuid = loc_event->map.mapUuid;

	emit_signal("localization_changed",
			(int)current_state,
			(int)current_confidence,
			(int)current_error_flags);

	return true;
}

Array OpenXRMlLocalizationMapExtension::get_localization_maps() const {
	Array result;
	if (!ml_localization_map_ext) {
		UtilityFunctions::print("get_localization_maps: extension not supported");
		return result;
	}
	if (cached_session == XR_NULL_HANDLE) {
		UtilityFunctions::print("get_localization_maps: no active session");
		return result;
	}

	uint32_t count = 0;
	XrResult xr_result = xrQueryLocalizationMapsML(cached_session, nullptr, 0, &count, nullptr);
	UtilityFunctions::print("get_localization_maps: xrQueryLocalizationMapsML count_query result=", (int64_t)xr_result, " count=", (int64_t)count);
	if (XR_FAILED(xr_result) || count == 0) {
		return result;
	}

	std::vector<XrLocalizationMapML> maps(count, { XR_TYPE_LOCALIZATION_MAP_ML });
	xr_result = xrQueryLocalizationMapsML(cached_session, nullptr, count, &count, maps.data());
	if (XR_FAILED(xr_result)) {
		return result;
	}

	for (uint32_t i = 0; i < count; i++) {
		const uint8_t *d = maps[i].mapUuid.data;
		char uuid_buf[37];
		snprintf(uuid_buf, sizeof(uuid_buf),
				"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
				d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
				d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);

		Dictionary entry;
		entry["uuid"] = String(uuid_buf);
		entry["name"] = String(maps[i].name);
		result.append(entry);
	}

	return result;
}

PackedByteArray OpenXRMlLocalizationMapExtension::export_localization_map(const String &p_uuid) const {
	PackedByteArray result;
	if (!ml_localization_map_ext || cached_session == XR_NULL_HANDLE) {
		return result;
	}

	// Parse UUID string to XrUuidEXT
	String hex = p_uuid.replace("-", "");
	if (hex.length() != 32) {
		UtilityFunctions::printerr("export_localization_map: invalid UUID format");
		return result;
	}
	XrUuidEXT map_uuid = {};
	for (int i = 0; i < 16; i++) {
		map_uuid.data[i] = (uint8_t)hex.substr(i * 2, 2).hex_to_int();
	}

	// Create export handle
	XrExportedLocalizationMapML exported_map = XR_NULL_HANDLE;
	XrResult xr_result = xrCreateExportedLocalizationMapML(cached_session, &map_uuid, &exported_map);
	if (XR_FAILED(xr_result)) {
		UtilityFunctions::printerr("export_localization_map: xrCreateExportedLocalizationMapML failed result=", (int64_t)xr_result);
		return result;
	}

	// Two-call pattern to get data size then data
	uint32_t data_size = 0;
	xr_result = xrGetExportedLocalizationMapDataML(exported_map, 0, &data_size, nullptr);
	if (XR_FAILED(xr_result) || data_size == 0) {
		UtilityFunctions::printerr("export_localization_map: size query failed result=", (int64_t)xr_result);
		xrDestroyExportedLocalizationMapML(exported_map);
		return result;
	}

	std::vector<char> data(data_size);
	xr_result = xrGetExportedLocalizationMapDataML(exported_map, data_size, &data_size, data.data());
	xrDestroyExportedLocalizationMapML(exported_map);
	if (XR_FAILED(xr_result)) {
		UtilityFunctions::printerr("export_localization_map: data fetch failed result=", (int64_t)xr_result);
		return result;
	}

	result.resize(data_size);
	memcpy(result.ptrw(), data.data(), data_size);
	return result;
}

String OpenXRMlLocalizationMapExtension::import_localization_map(const PackedByteArray &p_data) {
	if (!ml_localization_map_ext || cached_session == XR_NULL_HANDLE) {
		return String();
	}

	XrLocalizationMapImportInfoML import_info = {
		XR_TYPE_LOCALIZATION_MAP_IMPORT_INFO_ML,
		nullptr,
		(uint32_t)p_data.size(),
		(char *)p_data.ptr(),
	};

	XrUuidEXT map_uuid = {};
	XrResult xr_result = xrImportLocalizationMapML(cached_session, &import_info, &map_uuid);
	if (XR_FAILED(xr_result)) {
		UtilityFunctions::printerr("import_localization_map: import failed result=", (int64_t)xr_result);
		return String();
	}

	// Request the device to localize into the imported map
	XrMapLocalizationRequestInfoML request_info = {
		XR_TYPE_MAP_LOCALIZATION_REQUEST_INFO_ML,
		nullptr,
		map_uuid,
	};
	xr_result = xrRequestMapLocalizationML(cached_session, &request_info);
	if (XR_FAILED(xr_result)) {
		UtilityFunctions::printerr("import_localization_map: xrRequestMapLocalizationML failed result=", (int64_t)xr_result);
	}

	const uint8_t *d = map_uuid.data;
	char buf[37];
	snprintf(buf, sizeof(buf),
			"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
			d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
	return String(buf);
}

bool OpenXRMlLocalizationMapExtension::initialize_ml_localization_map_extension(const XrInstance &p_instance) {
	GDEXTENSION_INIT_XR_FUNC_V(xrEnableLocalizationEventsML);
	GDEXTENSION_INIT_XR_FUNC_V(xrQueryLocalizationMapsML);
	GDEXTENSION_INIT_XR_FUNC_V(xrCreateExportedLocalizationMapML);
	GDEXTENSION_INIT_XR_FUNC_V(xrDestroyExportedLocalizationMapML);
	GDEXTENSION_INIT_XR_FUNC_V(xrGetExportedLocalizationMapDataML);
	GDEXTENSION_INIT_XR_FUNC_V(xrImportLocalizationMapML);
	GDEXTENSION_INIT_XR_FUNC_V(xrRequestMapLocalizationML);
	return true;
}
