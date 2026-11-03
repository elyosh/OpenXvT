#include "xvt_app/settings/controller_page.h"

#include "aeron/aeron.h"
#include "xvt_app/touch_controls.h"
#include "xvt_runtime/input/controller_mapping.h"

#include <stdio.h>
#include <string.h>

enum {
	CONTROLLER_PAGE_AXES = 0,
	CONTROLLER_PAGE_ACTIONS,
};

static const char* const k_axis_names[XVT_INPUT_AXIS_COUNT] = { "Yaw", "Pitch", "Roll", "Throttle" };

static XvtControllerProfile* XvtControllerPage_ActiveProfile(XvtControllerSettings* settings,
															 const AeronControllerSnapshot* controller) {
	int i = XvtControllerOptions_FindModel(&settings->draft, controller->guid);
	return i >= 0 ? &settings->draft.models[i].profile : &settings->unconfigured;
}

static const XvtControllerProfile*
XvtControllerPage_ActiveProfileConst(const XvtControllerSettings* settings,
									 const AeronControllerSnapshot* controller) {
	int i = XvtControllerOptions_FindModel(&settings->draft, controller->guid);
	return i >= 0 ? &settings->draft.models[i].profile : &settings->unconfigured;
}

static const AeronControllerSnapshot* XvtControllerPage_SelectedController(XvtControllerSettings* settings,
																		   const AeronInputSnapshot* input) {
	if (!input)
		return NULL;
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i)
		if (input->controllers[i].connected &&
			input->controllers[i].instance_id == settings->selected_instance &&
			!XvtTouchControls_IsVirtual(input->controllers[i].instance_id))
			return &input->controllers[i];
	return NULL;
}

static void XvtControllerSettings_ApplyDraft(XvtControllerSettings* settings) {
	if (settings->selected_guid[0] &&
		XvtControllerOptions_FindModel(&settings->draft, settings->selected_guid) < 0) {
		const AeronControllerSnapshot* device =
			XvtControllerPage_SelectedController(settings, Aeron_InputSnapshot());
		if (!device ||
			!XvtControllerOptions_AddModel(&settings->draft, device, settings->error, sizeof settings->error))
			return;
		settings->draft.models[settings->draft.count - 1].profile = settings->unconfigured;
	}
	settings->dirty = !XvtControllerOptions_Equals(&settings->draft, &settings->original);
	if (!XvtControllerOptions_Validate(&settings->draft, settings->error, sizeof settings->error))
		return;
	XvtControllerMapping_SetOptions(&settings->draft);
	settings->error[0] = 0;
}

static bool XvtControllerSettings_DigitalSourceEqual(const AeronControllerDigitalSource* left,
													 const AeronControllerDigitalSource* right) {
	return left->kind == right->kind && left->index == right->index &&
		   (left->kind != AERON_CONTROLLER_DIGITAL_HAT || left->hat_direction == right->hat_direction);
}

static const char* XvtControllerPage_GamepadAxisDisplay(int source) {
	static const char* const names[AERON_GAMEPAD_AXIS_COUNT] = { "Left X",  "Left Y",       "Right X",
																 "Right Y", "Left Trigger", "Right Trigger" };
	return source >= 0 && source < AERON_GAMEPAD_AXIS_COUNT ? names[source] : NULL;
}

static const char* XvtControllerPage_GamepadButtonDisplay(int source) {
	static const char* const names[AERON_GAMEPAD_BUTTON_COUNT] = {
		"South",          "East",          "West",        "North",         "Back",           "Guide",
		"Start",          "Left Stick",    "Right Stick", "Left Shoulder", "Right Shoulder", "D-pad Up",
		"D-pad Down",     "D-pad Left",    "D-pad Right", "Misc 1",        "Right Paddle 1", "Left Paddle 1",
		"Right Paddle 2", "Left Paddle 2", "Touchpad",    "Misc 2",        "Misc 3",         "Misc 4",
		"Misc 5",         "Misc 6"
	};
	return source >= 0 && source < AERON_GAMEPAD_BUTTON_COUNT ? names[source] : NULL;
}

static bool XvtControllerSettings_SourceAvailable(const AeronControllerSnapshot* controller,
												  const AeronControllerDigitalSource* source) {
	if (!controller)
		return false;
	if (source->kind == AERON_CONTROLLER_DIGITAL_BUTTON)
		return controller->kind == AERON_CONTROLLER_KIND_GAMEPAD
				   ? source->index < AERON_GAMEPAD_BUTTON_COUNT &&
						 (controller->gamepad_available_buttons & (1u << source->index))
				   : source->index < controller->button_count;
	if (source->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ||
		source->kind == AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE)
		return controller->kind == AERON_CONTROLLER_KIND_GAMEPAD
				   ? source->index < AERON_GAMEPAD_AXIS_COUNT &&
						 (controller->gamepad_available_axes & (1u << source->index))
				   : source->index < controller->axis_count;
	return source->kind == AERON_CONTROLLER_DIGITAL_HAT &&
		   controller->kind == AERON_CONTROLLER_KIND_JOYSTICK && source->index < controller->hat_count;
}

static void XvtControllerSettings_FormatAxisSource(char* buffer, size_t capacity, int source,
												   const AeronControllerSnapshot* controller) {
	if (source < 0) {
		snprintf(buffer, capacity, "Not Bound");
		return;
	}
	const char* name = controller->kind == AERON_CONTROLLER_KIND_GAMEPAD
						   ? XvtControllerPage_GamepadAxisDisplay(source)
						   : NULL;
	const bool available =
		controller->kind == AERON_CONTROLLER_KIND_GAMEPAD
			? source < AERON_GAMEPAD_AXIS_COUNT && (controller->gamepad_available_axes & (1u << source))
			: source < controller->axis_count;
	if (name)
		snprintf(buffer, capacity, "%s%s", name, available ? "" : " (Unavailable)");
	else
		snprintf(buffer, capacity, "Axis %d%s", source, available ? "" : " (Unavailable)");
}

static const char* XvtControllerPage_HatDirectionName(uint8_t direction) {
	switch (direction) {
		case AERON_CONTROLLER_HAT_UP:
			return "Up";
		case AERON_CONTROLLER_HAT_RIGHT:
			return "Right";
		case AERON_CONTROLLER_HAT_DOWN:
			return "Down";
		default:
			return "Left";
	}
}

static void XvtControllerSettings_FormatDigitalSource(char* buffer, size_t capacity,
													  const AeronControllerDigitalSource* source,
													  const AeronControllerSnapshot* controller) {
	const char* name = NULL;
	if (source->kind == AERON_CONTROLLER_DIGITAL_BUTTON) {
		name = controller->kind == AERON_CONTROLLER_KIND_GAMEPAD
				   ? XvtControllerPage_GamepadButtonDisplay(source->index)
				   : NULL;
		if (name)
			snprintf(buffer, capacity, "%s", name);
		else
			snprintf(buffer, capacity, "Button %u", (unsigned)source->index);
	} else if (source->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ||
			   source->kind == AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE) {
		name = controller->kind == AERON_CONTROLLER_KIND_GAMEPAD
				   ? XvtControllerPage_GamepadAxisDisplay(source->index)
				   : NULL;
		if (name)
			snprintf(buffer, capacity, "%s %c", name,
					 source->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ? '+' : '-');
		else
			snprintf(buffer, capacity, "Axis %u %c", (unsigned)source->index,
					 source->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ? '+' : '-');
	} else {
		snprintf(buffer, capacity, "Hat %u %s", (unsigned)source->index,
				 XvtControllerPage_HatDirectionName(source->hat_direction));
	}
	if (!XvtControllerSettings_SourceAvailable(controller, source)) {
		const size_t used = strlen(buffer);
		if (used < capacity)
			snprintf(buffer + used, capacity - used, " (Unavailable)");
	}
}

static void XvtControllerSettings_AppendText(char* buffer, size_t capacity, const char* text) {
	const size_t used = strlen(buffer);
	if (used < capacity)
		snprintf(buffer + used, capacity - used, "%s%s", used ? ", " : "", text);
}

static void XvtControllerSettings_DescribeActionBindings(char* buffer, size_t capacity,
														 const XvtControllerProfile* profile,
														 XvtInputAction action,
														 const AeronControllerSnapshot* controller) {
	buffer[0] = '\0';
	for (size_t index = 0; index < profile->binding_count; ++index) {
		if (profile->bindings[index].action != action)
			continue;
		char source[96];
		XvtControllerSettings_FormatDigitalSource(source, sizeof source, &profile->bindings[index].source,
												  controller);
		XvtControllerSettings_AppendText(buffer, capacity, source);
	}
	if (!buffer[0])
		snprintf(buffer, capacity, "Not Bound");
}

void XvtControllerSettings_Open(XvtControllerSettings* settings, const XvtSettings* config) {
	if (!settings || !config)
		return;
	memset(settings, 0, sizeof *settings);
	settings->original = config->controller;
	settings->draft = settings->original;
	settings->editor.action_selected = SIZE_MAX;
	settings->editor.binding_selected = SIZE_MAX;
	settings->editor.selected_action = XVT_INPUT_ACTION_NONE;
	settings->pending_axis = XVT_INPUT_AXIS_YAW;
	XvtControllerOptions_ClearProfile(&settings->unconfigured, AERON_CONTROLLER_KIND_JOYSTICK);
}

void XvtControllerSettings_CancelCapture(XvtControllerSettings* settings, AeronUiContext* ui) {
	if (ui)
		AeronUi_CancelControllerCapture(ui);
	if (settings) {
		settings->axis_conflict_open = 0;
		settings->binding_conflict_open = 0;
		settings->editor.binding_modal_open = 0;
	}
}

static void XvtControllerSettings_ResetDeviceEditState(XvtControllerSettings* settings, AeronUiContext* ui) {
	AeronUi_CancelControllerCapture(ui);
	settings->editor.action_selected = SIZE_MAX;
	settings->editor.binding_selected = SIZE_MAX;
	settings->editor.selected_action = XVT_INPUT_ACTION_NONE;
	settings->editor.binding_modal_open = 0;
	settings->axis_conflict_open = 0;
	settings->binding_conflict_open = 0;
}

bool XvtControllerSettings_Commit(XvtControllerSettings* settings, char* error, size_t error_capacity) {
	if (!settings || !XvtConfig_Settings()) {
		if (error && error_capacity)
			snprintf(error, error_capacity, "controller settings are unavailable");
		return false;
	}
	if (!settings->dirty)
		return true;
	const bool result = XvtConfig_SetController(&settings->draft, error, error_capacity);
	if (!result)
		return false;
	settings->draft = XvtConfig_Settings()->controller;
	settings->original = settings->draft;
	settings->dirty = false;
	return true;
}

static void XvtControllerSettings_DeviceSelector(XvtControllerSettings* settings, AeronUiContext* ui,
												 const AeronInputSnapshot* input) {
	enum { CAP = AERON_CONTROLLER_MAX };

	char labels[CAP][192];
	const char* options[CAP];
	const char* guids[CAP];
	uint32_t ids[CAP];
	int count = 0, selected = -1;
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		const AeronControllerSnapshot* d = &input->controllers[i];
		if (!d->connected || XvtTouchControls_IsVirtual(d->instance_id))
			continue;
		bool duplicate_name = false;
		for (int other = 0; other < AERON_CONTROLLER_MAX; ++other) {
			const AeronControllerSnapshot* candidate = &input->controllers[other];
			if (other != i && candidate->connected && !strcmp(candidate->name, d->name))
				duplicate_name = true;
		}
		if (duplicate_name)
			snprintf(labels[count], sizeof labels[count], "%s #%d", d->name, i + 1);
		else
			snprintf(labels[count], sizeof labels[count], "%s", d->name);
		options[count] = labels[count];
		guids[count] = d->guid;
		ids[count] = d->instance_id;
		if (settings->selected_instance == d->instance_id)
			selected = count;
		++count;
	}
	if (!count) {
		XvtControllerSettings_ResetDeviceEditState(settings, ui);
		settings->selected_instance = 0;
		settings->selected_guid[0] = 0;
		AeronUi_Help(ui, "Connect a controller to configure its controls.");
		return;
	}
	bool changed = selected < 0;
	if (selected < 0)
		selected = 0;
	AeronUi_Header(ui, "Device");
	changed |= AeronUi_Selector(ui, "##controller_device", &selected, options, count) != 0;
	if (changed || settings->selected_instance != ids[selected]) {
		XvtControllerSettings_ResetDeviceEditState(settings, ui);
		snprintf(settings->selected_guid, sizeof settings->selected_guid, "%s", guids[selected]);
		settings->selected_instance = ids[selected];
		const AeronControllerSnapshot* selected_device =
			XvtControllerPage_SelectedController(settings, input);
		XvtControllerOptions_ClearProfile(&settings->unconfigured, selected_device
																	   ? selected_device->kind
																	   : AERON_CONTROLLER_KIND_JOYSTICK);
	}
}

void XvtControllerSettings_Discover(XvtControllerSettings* settings, AeronUiContext* ui,
									const AeronInputSnapshot* input, const XvtControllerProfile* defaults) {
	AeronInputSnapshot filtered;
	size_t count = settings->draft.count;
	XvtTouchControls_Omit(&filtered, input);
	bool ok = XvtControllerOptions_InitializeGamepads(&settings->draft, defaults, &filtered, settings->error,
													  sizeof settings->error);
	if (settings->draft.count != count) {
		XvtControllerSettings_ResetDeviceEditState(settings, ui);
		settings->dirty = true;
		XvtControllerMapping_SetOptions(&settings->draft);
	}
	if (ok && settings->capacity_warned)
		settings->error[0] = 0;
	settings->capacity_warned = !ok;
}

static int XvtControllerSettings_ProfileMissingCount(const XvtControllerProfile* profile,
													 const AeronControllerSnapshot* controller) {
	int missing = 0;
	for (int axis = 0; axis < XVT_INPUT_AXIS_COUNT; ++axis) {
		const int source = profile->mapping.axes[axis].source;
		if (source < 0)
			continue;
		if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
			if (source >= AERON_GAMEPAD_AXIS_COUNT || !(controller->gamepad_available_axes & (1u << source)))
				++missing;
		} else if (source >= controller->axis_count) {
			++missing;
		}
	}
	for (size_t index = 0; index < profile->binding_count; ++index)
		if (!XvtControllerSettings_SourceAvailable(controller, &profile->bindings[index].source))
			++missing;
	return missing;
}

static void XvtControllerSettings_ControllerWarning(const XvtControllerSettings* settings, AeronUiContext* ui,
													const AeronControllerSnapshot* controller) {
	if (!controller) {
		AeronUi_Help(ui, "Select a connected device to assign controls.");
		return;
	}

	const int missing = XvtControllerSettings_ProfileMissingCount(
		XvtControllerPage_ActiveProfileConst(settings, controller), controller);
	if (missing && controller->controls_truncated) {
		char text[256];
		snprintf(text, sizeof text,
				 "%d configured controls are unavailable; this controller also exposes more controls than "
				 "the game supports.",
				 missing);
		AeronUi_Error(ui, text);
	} else if (missing) {
		char text[128];
		snprintf(text, sizeof text, "%d configured controls are unavailable.", missing);
		AeronUi_Error(ui, text);
	} else if (controller->controls_truncated)
		AeronUi_Error(ui, "This controller exposes more controls than the game supports.");
}

static float XvtControllerSettings_ControllerAxisValue(const AeronControllerSnapshot* controller,
													   int source) {
	if (!controller || source < 0)
		return 0.0f;
	int value;
	if (controller->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
		if (source >= AERON_GAMEPAD_AXIS_COUNT || !(controller->gamepad_available_axes & (1u << source)))
			return 0.0f;
		value = controller->gamepad_axes[source];
	} else {
		if (source >= controller->axis_count || source >= AERON_CONTROLLER_AXIS_MAX)
			return 0.0f;
		value = controller->raw_axes[source];
	}
	return value < 0 ? (float)value / 32768.0f : (float)value / 32767.0f;
}

static void XvtControllerSettings_InstallAxis(XvtControllerSettings* settings,
											  const AeronControllerSnapshot* controller, XvtInputAxis axis,
											  int source) {
	XvtControllerOptions candidate = settings->draft;
	int model = XvtControllerOptions_FindModel(&candidate, controller->guid);
	if (model < 0) {
		if (!XvtControllerOptions_AddModel(&candidate, controller, settings->error, sizeof settings->error))
			return;
		model = (int)candidate.count - 1;
		candidate.models[model].profile = settings->unconfigured;
	}
	XvtControllerProfile* p = &candidate.models[model].profile;
	for (size_t i = 0; i < candidate.count; ++i)
		candidate.models[i].profile.mapping.axes[axis].source = -1;
	for (int i = 0; i < XVT_INPUT_AXIS_COUNT; ++i)
		if (p->mapping.axes[i].source == source)
			p->mapping.axes[i].source = -1;
	p->mapping.axes[axis].source = (int8_t)source;
	if (!XvtControllerOptions_Validate(&candidate, settings->error, sizeof settings->error))
		return;
	settings->draft = candidate;
	XvtControllerSettings_ApplyDraft(settings);
}

static void XvtControllerSettings_AssignCapturedAxis(XvtControllerSettings* settings,
													 const AeronControllerSnapshot* controller,
													 XvtInputAxis axis, int source) {
	settings->conflict_text[0] = 0;
	const XvtControllerProfile* p = XvtControllerPage_ActiveProfileConst(settings, controller);
	for (size_t i = 0; i < settings->draft.count; ++i) {
		const XvtControllerModel* m = &settings->draft.models[i];
		if (strcmp(m->guid, controller->guid) && m->profile.mapping.axes[axis].source >= 0) {
			char text[192];
			snprintf(text, sizeof text, "%s on %s", k_axis_names[axis], m->name);
			XvtControllerSettings_AppendText(settings->conflict_text, sizeof settings->conflict_text, text);
		}
	}
	for (int i = 0; i < XVT_INPUT_AXIS_COUNT; ++i)
		if (i != (int)axis && p->mapping.axes[i].source == source)
			XvtControllerSettings_AppendText(settings->conflict_text, sizeof settings->conflict_text,
											 k_axis_names[i]);
	if (settings->conflict_text[0]) {
		settings->pending_axis = axis;
		settings->pending_axis_source = source;
		settings->axis_conflict_open = 1;
	} else
		XvtControllerSettings_InstallAxis(settings, controller, axis, source);
}

static void XvtControllerSettings_AxisEditor(XvtControllerSettings* settings, AeronUiContext* ui,
											 const AeronControllerSnapshot* controller, XvtInputAxis axis) {
	const AeronControllerKind kind = controller ? controller->kind : AERON_CONTROLLER_KIND_NONE;
	XvtControllerProfile* profile = XvtControllerPage_ActiveProfile(settings, controller);

	XvtInputAxisBinding* binding = &profile->mapping.axes[axis];
	char source[128];
	XvtControllerSettings_FormatAxisSource(source, sizeof source, binding->source, controller);
	AeronUi_PushId(ui, axis);
	const AeronUiControllerCaptureDesc desc = { controller->connected ? controller->instance_id : 0,
												AERON_UI_CONTROLLER_CAPTURE_ANALOG_AXIS };
	AeronUiControllerInput captured;
	const AeronUiControllerCaptureResult capture =
		AeronUi_ControllerCapture(ui, "Source", source, &desc, &captured);
	if (capture == AERON_UI_CONTROLLER_CAPTURE_CAPTURED && captured.controller_kind == kind &&
		captured.instance_id == controller->instance_id)
		XvtControllerSettings_AssignCapturedAxis(settings, controller, axis, captured.value.axis);
	profile = XvtControllerPage_ActiveProfile(settings, controller);
	binding = &profile->mapping.axes[axis];
	float live = XvtControllerSettings_ControllerAxisValue(controller, binding->source);
	if (XvtControllerOptions_EffectiveAxisInvert(kind, axis, binding->invert))
		live = -live;
	if (axis == XVT_INPUT_AXIS_THROTTLE) {
		if (Aeron_ControllerAxisAvailable(controller, binding->source)) {
			const int16_t raw = Aeron_ControllerAxisValue(controller, binding->source);
			const uint16_t position =
				XvtControllerMapping_ThrottlePosition(raw, kind, binding->source, binding->invert);
			AeronUi_PercentageMeter(ui, "Position", (float)position / UINT16_MAX);
		} else
			AeronUi_Help(ui, "Assign an available throttle axis to see its position.");
	} else {
		AeronUi_ControllerAxisMeter(ui, "Input", live, binding->deadzone);
	}
	int invert = binding->invert;
	if (AeronUi_Toggle(ui, "Invert", &invert)) {
		binding->invert = invert != 0;
		XvtControllerSettings_ApplyDraft(settings);
	}
	if (axis != XVT_INPUT_AXIS_THROTTLE) {
		float deadzone_percent = binding->deadzone * 100.0f;
		const float minimum = 0.0f;
		if (AeronUi_SliderFloat(ui, "Deadzone", &deadzone_percent, minimum, 100.0f, 1.0f, "%.1f%%")) {
			binding->deadzone = deadzone_percent / 100.0f;
			XvtControllerSettings_ApplyDraft(settings);
		}
	}
	if (AeronUi_ButtonEnabled(ui, "Clear Binding", binding->source >= 0)) {
		binding->source = -1;
		XvtControllerSettings_ApplyDraft(settings);
	}
	AeronUi_PopId(ui);
}

static void XvtControllerSettings_AxisPage(XvtControllerSettings* settings, AeronUiContext* ui,
										   const AeronControllerSnapshot* controller) {
	if (AeronUi_SegmentedSelector(ui, "Flight Axis", &settings->axis, k_axis_names, XVT_INPUT_AXIS_COUNT))
		AeronUi_CancelControllerCapture(ui);
	XvtControllerSettings_AxisEditor(settings, ui, controller, (XvtInputAxis)settings->axis);
}

static void XvtControllerSettings_AxisConflictModal(XvtControllerSettings* settings, AeronUiContext* ui,
													const AeronControllerSnapshot* controller) {
	if (!AeronUi_BeginModal(ui, "AXIS ALREADY ASSIGNED", &settings->axis_conflict_open, NULL))
		return;
	char text[640];
	snprintf(text, sizeof text, "Replace these assignments with %s: %s?",
			 k_axis_names[settings->pending_axis], settings->conflict_text);
	AeronUi_Error(ui, text);
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_Button(ui, "Replace")) {
		XvtControllerSettings_InstallAxis(settings, controller, settings->pending_axis,
										  settings->pending_axis_source);
		settings->axis_conflict_open = 0;
	}
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Cancel"))
		settings->axis_conflict_open = 0;
	AeronUi_EndColumns(ui);
	AeronUi_EndModal(ui);
}

static size_t XvtControllerSettings_FindSourceBinding(const XvtControllerProfile* profile,
													  const AeronControllerDigitalSource* source) {
	for (size_t index = 0; index < profile->binding_count; ++index)
		if (XvtControllerSettings_DigitalSourceEqual(&profile->bindings[index].source, source))
			return index;
	return SIZE_MAX;
}

static void XvtControllerSettings_RemoveBinding(XvtControllerProfile* profile, size_t index) {
	if (index >= profile->binding_count)
		return;
	if (index + 1 < profile->binding_count)
		memmove(&profile->bindings[index], &profile->bindings[index + 1],
				(profile->binding_count - index - 1) * sizeof profile->bindings[0]);
	--profile->binding_count;
}

static void XvtControllerSettings_AddCapturedBinding(XvtControllerSettings* settings,
													 const AeronControllerSnapshot* controller,
													 const AeronControllerDigitalSource* source) {
	XvtControllerProfile* profile = XvtControllerPage_ActiveProfile(settings, controller);
	const size_t existing = XvtControllerSettings_FindSourceBinding(profile, source);
	if (existing != SIZE_MAX) {
		if (profile->bindings[existing].action == settings->editor.selected_action) {
			size_t position = 0;
			for (size_t index = 0; index < existing; ++index)
				if (profile->bindings[index].action == settings->editor.selected_action)
					++position;
			settings->editor.binding_selected = position;
			return;
		}
		settings->pending_digital = *source;
		settings->conflicting_action = profile->bindings[existing].action;
		settings->binding_conflict_open = 1;
		return;
	}
	if (profile->binding_count >= XVT_CONTROLLER_BINDING_CAP) {
		snprintf(
			settings->error, sizeof settings->error,
			"You've reached the maximum number of control assignments. Remove one before adding another.");
		return;
	}
	profile->bindings[profile->binding_count++] =
		(XvtInputActionBinding) { .source = *source, .action = settings->editor.selected_action };
	settings->editor.binding_selected = SIZE_MAX;
	XvtControllerSettings_ApplyDraft(settings);
}

static void XvtControllerSettings_FindBindingCapture(XvtControllerSettings* settings, AeronUiContext* ui,
													 const AeronControllerSnapshot* controller) {
	const AeronControllerKind kind = controller->kind;
	const AeronUiControllerCaptureDesc desc = { controller->instance_id,
												AERON_UI_CONTROLLER_CAPTURE_DIGITAL };
	AeronUiControllerInput captured;
	const AeronUiControllerCaptureResult result =
		AeronUi_ControllerCapture(ui, "Find Binding...", "Press to identify", &desc, &captured);
	if (result != AERON_UI_CONTROLLER_CAPTURE_CAPTURED || captured.controller_kind != kind)
		return;
	const XvtControllerProfile* profile = XvtControllerPage_ActiveProfileConst(settings, controller);
	const size_t binding = XvtControllerSettings_FindSourceBinding(profile, &captured.value.digital);
	if (binding == SIZE_MAX) {
		snprintf(settings->error, sizeof settings->error, "This control is not bound.");
		return;
	}
	XvtBindingsEditor_Select(&settings->editor, profile->bindings[binding].action, false);
	settings->error[0] = '\0';
}

static void XvtControllerSettings_ActionsPage(XvtControllerSettings* settings, AeronUiContext* ui,
											  const AeronControllerSnapshot* controller,
											  float trailing_height_ref) {
	XvtBindingsEditor_Category(&settings->editor, ui);
	XvtControllerSettings_FindBindingCapture(settings, ui, controller);
	AeronUi_Spacer(ui, 8.0f);
	AeronUiListItem items[XVT_INPUT_ACTION_COUNT - 1];
	char details[XVT_INPUT_ACTION_COUNT - 1][256];
	size_t count = 0;
	const XvtControllerProfile* profile = XvtControllerPage_ActiveProfileConst(settings, controller);
	for (int action = XVT_INPUT_ACTION_NONE + 1; action < XVT_INPUT_ACTION_COUNT; ++action) {
		if ((int)XvtInputActions_Category((XvtInputAction)action) != settings->editor.category)
			continue;
		XvtControllerSettings_DescribeActionBindings(details[count], sizeof details[count], profile,
													 (XvtInputAction)action, controller);
		items[count] = (AeronUiListItem) { .id = (uint64_t)action,
										   .label = XvtInputActions_DisplayName((XvtInputAction)action),
										   .detail = details[count] };
		++count;
	}
	XvtBindingsEditor_Actions(&settings->editor, ui, items, count, trailing_height_ref);
}

static size_t XvtControllerSettings_BuildActionBindingItems(const XvtControllerSettings* settings,
															const AeronControllerSnapshot* controller,
															AeronUiListItem* items, char labels[][128],
															size_t* profile_indices) {
	const XvtControllerProfile* profile = XvtControllerPage_ActiveProfileConst(settings, controller);
	size_t count = 0;
	for (size_t index = 0; index < profile->binding_count; ++index) {
		if (profile->bindings[index].action != settings->editor.selected_action)
			continue;
		XvtControllerSettings_FormatDigitalSource(labels[count], 128, &profile->bindings[index].source,
												  controller);
		items[count] = (AeronUiListItem) { .id = index, .label = labels[count], .detail = NULL };
		profile_indices[count] = index;
		++count;
	}
	return count;
}

static void XvtControllerSettings_BindingDetailModal(XvtControllerSettings* settings, AeronUiContext* ui,
													 const AeronControllerSnapshot* controller) {
	if (!XvtBindingsEditor_BeginDetail(&settings->editor, ui))
		return;
	XvtControllerProfile* profile = XvtControllerPage_ActiveProfile(settings, controller);
	AeronUiListItem items[XVT_CONTROLLER_BINDING_CAP];
	char labels[XVT_CONTROLLER_BINDING_CAP][128];
	size_t profile_indices[XVT_CONTROLLER_BINDING_CAP];
	const size_t count =
		XvtControllerSettings_BuildActionBindingItems(settings, controller, items, labels, profile_indices);
	XvtBindingsEditor_List(&settings->editor, ui, items, count, "No bindings assigned.");
	if (settings->editor.binding_selected < count) {
		const size_t profile_index = profile_indices[settings->editor.binding_selected];
		AeronControllerDigitalSource* source = &profile->bindings[profile_index].source;
		if (source->kind == AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE ||
			source->kind == AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE) {
			int threshold = (int)(source->threshold * 100.0f + 0.5f);
			if (AeronUi_SliderInt(ui, "Axis Threshold", &threshold, 5, 100, 5, "%d%%")) {
				source->threshold = (float)threshold / 100.0f;
				XvtControllerSettings_ApplyDraft(settings);
			}
		}
		if (XvtBindingsEditor_Remove(ui)) {
			XvtControllerSettings_RemoveBinding(profile, profile_index);
			settings->editor.binding_selected = SIZE_MAX;
			XvtControllerSettings_ApplyDraft(settings);
		}
	}
	AeronUi_Separator(ui);
	const AeronControllerKind kind = controller->kind;
	const bool has_capacity = profile->binding_count < XVT_CONTROLLER_BINDING_CAP;
	const AeronUiControllerCaptureDesc desc = { has_capacity ? controller->instance_id : 0,
												AERON_UI_CONTROLLER_CAPTURE_DIGITAL };
	AeronUiControllerInput captured;
	const AeronUiControllerCaptureResult capture =
		AeronUi_ControllerCapture(ui, "Add Binding...", "Press to add", &desc, &captured);
	if (capture == AERON_UI_CONTROLLER_CAPTURE_CAPTURED && captured.controller_kind == kind)
		XvtControllerSettings_AddCapturedBinding(settings, controller, &captured.value.digital);
	if (!has_capacity)
		AeronUi_Error(
			ui,
			"You've reached the maximum number of control assignments. Remove one before adding another.");
	XvtBindingsEditor_EndDetail(&settings->editor, ui);
}

static void XvtControllerSettings_BindingConflictModal(XvtControllerSettings* settings, AeronUiContext* ui,
													   const AeronControllerSnapshot* controller) {
	char label[128];
	XvtControllerSettings_FormatDigitalSource(label, sizeof label, &settings->pending_digital, controller);
	if (XvtBindingsEditor_Conflict(ui, &settings->binding_conflict_open, label, settings->conflicting_action,
								   settings->editor.selected_action)) {
		XvtControllerProfile* profile = XvtControllerPage_ActiveProfile(settings, controller);
		size_t existing = XvtControllerSettings_FindSourceBinding(profile, &settings->pending_digital);
		if (existing != SIZE_MAX) {
			profile->bindings[existing].action = settings->editor.selected_action;
			settings->editor.binding_selected = SIZE_MAX;
			XvtControllerSettings_ApplyDraft(settings);
		}
	}
}

static void XvtControllerSettings_RestoreModal(XvtControllerSettings* settings, AeronUiContext* ui,
											   const XvtSettings* config) {
	const AeronControllerSnapshot* device =
		XvtControllerPage_SelectedController(settings, Aeron_InputSnapshot());
	if (!device) {
		settings->restore_modal_open = 0;
		return;
	}
	if (!AeronUi_BeginModal(ui, "RESTORE CONTROLLER DEFAULTS", &settings->restore_modal_open, NULL))
		return;
	AeronUi_Help(ui, "Reset this model's bindings? Other controllers keep their assignments.");
	AeronUi_BeginColumns(ui, 2, NULL);
	if (AeronUi_Button(ui, "Reset to defaults")) {
		XvtControllerOptions candidate = settings->draft;
		if (XvtControllerOptions_AddModel(&candidate, device, settings->error, sizeof settings->error)) {
			int model = XvtControllerOptions_FindModel(&candidate, device->guid);
			XvtControllerModel* selected = &candidate.models[model];
			selected->kind = device->kind;
			XvtControllerOptions_ClearProfile(&selected->profile, device->kind);
			if (device->kind == AERON_CONTROLLER_KIND_GAMEPAD) {
				selected->profile = config->gamepad_defaults;
				for (size_t i = 0; i < candidate.count; ++i)
					if (i != (size_t)model)
						for (int axis = 0; axis < XVT_INPUT_AXIS_COUNT; ++axis)
							if (candidate.models[i].profile.mapping.axes[axis].source >= 0)
								selected->profile.mapping.axes[axis].source = -1;
			}
			settings->draft = candidate;
			XvtControllerSettings_ApplyDraft(settings);
			XvtControllerSettings_ResetDeviceEditState(settings, ui);
		}
		settings->restore_modal_open = 0;
	}
	AeronUi_NextColumn(ui);
	if (AeronUi_Button(ui, "Cancel"))
		settings->restore_modal_open = 0;
	AeronUi_EndColumns(ui);
	AeronUi_EndModal(ui);
}

void XvtControllerSettings_Draw(XvtControllerSettings* settings, AeronUiContext* ui,
								const AeronInputSnapshot* input) {
	static const char* const pages[] = { "Axes", "Bindings" };
	if (!settings || !ui || !input)
		return;
	XvtControllerSettings_DeviceSelector(settings, ui, input);
	const AeronControllerSnapshot* controller = XvtControllerPage_SelectedController(settings, input);
	const uint32_t active_instance = controller ? controller->instance_id : 0;
	if (active_instance != settings->active_instance ||
		(controller && controller->kind != settings->active_kind)) {
		XvtControllerSettings_ResetDeviceEditState(settings, ui);
		settings->active_instance = active_instance;
		settings->active_kind = controller ? controller->kind : AERON_CONTROLLER_KIND_NONE;
	}
	if (controller) {
		int matches = 0;
		for (int i = 0; i < AERON_CONTROLLER_MAX; ++i)
			if (input->controllers[i].connected && !strcmp(input->controllers[i].guid, controller->guid))
				++matches;
		if (matches > 1) {
			AeronUi_Help(ui, "Bindings shared by all controllers of this model.");
			uint32_t preferred = XvtControllerMapping_AnalogInstance(controller->guid);
			int model = XvtControllerOptions_FindModel(&settings->draft, controller->guid);
			const AeronControllerSnapshot* analog =
				model >= 0 ? XvtControllerMapping_Resolve(&settings->draft.models[model], input, preferred)
						   : NULL;
			for (int i = 0; analog && i < AERON_CONTROLLER_MAX; ++i)
				if (analog == &input->controllers[i]) {
					char text[160];
					snprintf(text, sizeof text, "Analog input: %s #%d", analog->name, i + 1);
					AeronUi_Help(ui, text);
				}
		}
	}
	int model = controller ? XvtControllerOptions_FindModel(&settings->draft, controller->guid) : -1;
	bool compatible = !controller || model < 0 || settings->draft.models[model].kind == controller->kind;
	if (!compatible) {
		XvtControllerSettings_ResetDeviceEditState(settings, ui);
		AeronUi_Error(ui, "Saved bindings use a different device type. Restore Controller Defaults to "
						  "configure this device.");
	} else
		XvtControllerSettings_ControllerWarning(settings, ui, controller);
	if (controller && compatible &&
		AeronUi_SegmentedSelector(ui, "Controller Page", &settings->page, pages, 2)) {
		AeronUi_CancelControllerCapture(ui);
		settings->editor.binding_modal_open = 0;
	}
	float trailing_height = 143.0f;
	float available;
	if (settings->error[0])
		trailing_height += AeronUi_MeasureHelpHeight(ui, settings->error, 0.0f) + 60.0f;
	available = AeronUi_AvailableHeight(ui);
	if (controller && compatible && settings->page == CONTROLLER_PAGE_AXES && available <= 1.0f) {
		XvtControllerSettings_AxisPage(settings, ui, controller);
	} else if (controller && compatible && settings->page == CONTROLLER_PAGE_AXES) {
		float scroll_height = available - trailing_height;
		if (scroll_height < 180.0f)
			scroll_height = 180.0f;
		if (AeronUi_BeginScroll(ui, "Flight Axes", scroll_height)) {
			XvtControllerSettings_AxisPage(settings, ui, controller);
			AeronUi_EndScroll(ui);
		}
	} else if (controller && compatible) {
		XvtControllerSettings_ActionsPage(settings, ui, controller, trailing_height);
	}
	if (settings->error[0]) {
		AeronUi_Error(ui, settings->error);
		if (AeronUi_Button(ui, "Dismiss Error"))
			settings->error[0] = '\0';
	}
	if (AeronUi_ButtonEnabled(ui, "Restore Controller Defaults", controller != NULL))
		settings->restore_modal_open = 1;
}

void XvtControllerSettings_DrawModals(XvtControllerSettings* settings, AeronUiContext* ui,
									  const AeronInputSnapshot* input, const XvtSettings* config) {
	if (!settings || !ui || !input || !config)
		return;
	const AeronControllerSnapshot* controller = XvtControllerPage_SelectedController(settings, input);
	int model = controller ? XvtControllerOptions_FindModel(&settings->draft, controller->guid) : -1;
	bool compatible = controller && (model < 0 || settings->draft.models[model].kind == controller->kind);
	if (!compatible)
		XvtControllerSettings_CancelCapture(settings, ui);
	if (settings->axis_conflict_open) {
		XvtControllerSettings_AxisConflictModal(settings, ui, controller);
		return;
	}
	if (settings->binding_conflict_open) {
		XvtControllerSettings_BindingConflictModal(settings, ui, controller);
		return;
	}
	if (settings->restore_modal_open) {
		XvtControllerSettings_RestoreModal(settings, ui, config);
		return;
	}
	if (compatible)
		XvtControllerSettings_BindingDetailModal(settings, ui, controller);
}
