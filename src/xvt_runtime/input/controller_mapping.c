#include "xvt_runtime/input/controller_mapping.h"
#include "aeron/debug.h"
#include "aeron/log.h"
#include "xvt/flight/flight_input.h"
#include "xvt/flight/player/player.h"
#include "xvt_runtime/input/capture.h"
#include "xvt_runtime/runtime/dialog_task.h"
#include "xvt_runtime/runtime/flight_task.h"
#include "xvt_runtime/runtime/port.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { ACTION_QUEUE_CAPACITY = 256 };

typedef struct ControllerInstance {
	uint32_t id;
	int model;
	bool down[XVT_CONTROLLER_BINDING_CAP], armed[XVT_CONTROLLER_BINDING_CAP],
		dispatched[XVT_CONTROLLER_BINDING_CAP];
	uint8_t menu_blocked;
	bool primed;
} ControllerInstance;

static struct {
	XvtControllerOptions options, pending;
	ControllerInstance instances[AERON_CONTROLLER_MAX];
	uint32_t analog[XVT_CONTROLLER_MODEL_CAP];
	bool pending_options, suspended, present, throttle_valid;
	int axes[3], menu_axes[3];
	uint16_t holds[XVT_INPUT_ACTION_COUNT], throttle;
	uint8_t menu_buttons, menu_hat;
	uint32_t throttle_instance, generation;
	uint16_t queue[ACTION_QUEUE_CAPACITY];
	unsigned read, write;
	uint64_t frame;
} g_controller;

static void XvtControllerMapping_QueueKey(uint16_t key) {
	unsigned next = (g_controller.write + 1) % ACTION_QUEUE_CAPACITY;
	if (next == g_controller.read) {
		Aeron_LogWarn("xvt.input", "Controller command queue is full");
		return;
	}
	g_controller.queue[g_controller.write] = key;
	g_controller.write = next;
}

static void XvtControllerMapping_DispatchAction(XvtInputAction action, bool pressed) {
	if (!XvtFlightTask_IsActive() || XvtFlightTask_IsLoading())
		return;
	if (action == XVT_INPUT_ACTION_FIRE_WEAPON || action == XVT_INPUT_ACTION_TARGET_ROLL_MODIFIER)
		return;
	uint16_t key = XvtInputActions_Key(action);
	if (!pressed) {
		if (key >= FLIGHT_KEY_PAD_1 && key <= FLIGHT_KEY_PAD_9)
			key = FLIGHT_KEY_PAD_8;
		else if (key != FLIGHT_KEY_PAD_0)
			return;
	}
	if ((unsigned)g_localPlayer >= 8)
		return;
	bool chat = g_players[g_localPlayer].msgTypeId != FLIGHT_CHAT_RECIPIENT_INACTIVE;
	if ((action == XVT_INPUT_ACTION_CHAT_SEND || action == XVT_INPUT_ACTION_CHAT_CANCEL) && !chat)
		return;
	if (action == XVT_INPUT_ACTION_ESCAPE && !chat && !XvtDialog_IsActive()) {
		if (pressed)
			XvtPort_RequestSettings();
		return;
	}
	XvtControllerMapping_QueueKey(key);
}

static void XvtControllerMapping_Dispatch(XvtInputAction action, bool pressed) {
	uint16_t key = XvtInputActions_Key(action);
	bool held = action == XVT_INPUT_ACTION_FIRE_WEAPON || action == XVT_INPUT_ACTION_TARGET_ROLL_MODIFIER ||
				(key >= FLIGHT_KEY_PAD_1 && key <= FLIGHT_KEY_PAD_9) || key == FLIGHT_KEY_PAD_0;
	if (held) {
		if (pressed) {
			if (g_controller.holds[action]++)
				return;
		} else if (!g_controller.holds[action] || --g_controller.holds[action])
			return;
	}
	XvtControllerMapping_DispatchAction(action, pressed);
}

const AeronControllerSnapshot* XvtControllerMapping_Resolve(const XvtControllerModel* model,
															const AeronInputSnapshot* input,
															uint32_t preferred) {
	const AeronControllerSnapshot* best = NULL;
	if (!input)
		return NULL;
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		const AeronControllerSnapshot* d = &input->controllers[i];
		if (!d->connected || strcmp(model->guid, d->guid) || d->kind != model->kind)
			continue;
		if (d->instance_id == preferred)
			return d;
		if (!best || d->instance_id < best->instance_id)
			best = d;
	}
	return best;
}

static void ReleaseInstance(ControllerInstance* state) {
	if (state->id) {
		const XvtControllerProfile* p = &g_controller.options.models[state->model].profile;
		for (size_t i = 0; i < p->binding_count; ++i)
			if (state->dispatched[i])
				XvtControllerMapping_Dispatch(p->bindings[i].action, false);
	}
	memset(state, 0, sizeof *state);
}

static void XvtControllerMapping_Install(const XvtControllerOptions* options) {
	static const XvtControllerOptions empty = { 0 };
	if (!options)
		options = &empty;
	char error[128];
	if (!XvtControllerOptions_Validate(options, error, sizeof error)) {
		Aeron_LogWarn("xvt.input", "%s", error);
		return;
	}
	if (XvtControllerOptions_Equals(options, &g_controller.options))
		return;
	uint32_t analog[XVT_CONTROLLER_MODEL_CAP] = { 0 };
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		ControllerInstance* state = &g_controller.instances[i];
		if (!state->id)
			continue;
		const XvtControllerModel* old = &g_controller.options.models[state->model];
		int next = XvtControllerOptions_FindModel(options, old->guid);
		if (next < 0 || old->kind != options->models[next].kind ||
			!XvtControllerOptions_ProfileEqual(&old->profile, &options->models[next].profile))
			ReleaseInstance(state);
	}
	for (size_t i = 0; i < options->count; ++i) {
		int old = XvtControllerOptions_FindModel(&g_controller.options, options->models[i].guid);
		if (old >= 0 && options->models[i].kind == g_controller.options.models[old].kind)
			analog[i] = g_controller.analog[old];
	}
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		ControllerInstance* state = &g_controller.instances[i];
		if (state->id)
			state->model =
				XvtControllerOptions_FindModel(options, g_controller.options.models[state->model].guid);
	}
	/* Only changes to the effective throttle binding invalidate its movement baseline. */
	for (size_t i = 0; i < g_controller.options.count; ++i) {
		const XvtControllerModel* old = &g_controller.options.models[i];
		const XvtInputAxisBinding* a = &old->profile.mapping.axes[XVT_INPUT_AXIS_THROTTLE];
		if (a->source < 0)
			continue;
		int n = XvtControllerOptions_FindModel(options, old->guid);
		if (n < 0 || old->kind != options->models[n].kind ||
			a->source != options->models[n].profile.mapping.axes[XVT_INPUT_AXIS_THROTTLE].source ||
			a->invert != options->models[n].profile.mapping.axes[XVT_INPUT_AXIS_THROTTLE].invert)
			++g_controller.generation;
	}
	g_controller.options = *options;
	memcpy(g_controller.analog, analog, sizeof analog);
}

void XvtControllerMapping_Init(const XvtControllerOptions* options) {
	memset(&g_controller, 0, sizeof g_controller);
	g_controller.frame = UINT64_MAX;
	XvtControllerMapping_Install(options);
}

void XvtControllerMapping_SetOptions(const XvtControllerOptions* options) {
	char error[128];
	if (!XvtControllerOptions_Validate(options, error, sizeof error)) {
		Aeron_LogWarn("xvt.input", "%s", error);
		return;
	}
	g_controller.pending = *options;
	g_controller.pending_options = true;
}

void XvtControllerMapping_ApplyPending(void) {
	if (!g_controller.pending_options)
		return;
	XvtControllerMapping_Install(&g_controller.pending);
	g_controller.pending_options = false;
}

const XvtControllerOptions* XvtControllerMapping_Options(void) { return &g_controller.options; }

void XvtControllerMapping_Suspend(void) {
	if (g_controller.suspended)
		return;
	/* Drop the entire route without dispatching synthetic gameplay releases. */
	memset(g_controller.instances, 0, sizeof g_controller.instances);
	memset(g_controller.holds, 0, sizeof g_controller.holds);
	memset(g_controller.axes, 0, sizeof g_controller.axes);
	memset(g_controller.menu_axes, 0, sizeof g_controller.menu_axes);
	g_controller.read = g_controller.write = 0;
	g_controller.menu_buttons = g_controller.menu_hat = 0;
	g_controller.throttle_valid = false;
	g_controller.suspended = true;
	++g_controller.generation;
}

static void XvtControllerMapping_Resume(void) { g_controller.suspended = false; }

void XvtControllerMapping_Shutdown(void) { memset(&g_controller, 0, sizeof g_controller); }

static void SampleMenu(ControllerInstance* state, const AeronControllerSnapshot* d) {
	uint8_t buttons = d->kind == AERON_CONTROLLER_KIND_GAMEPAD
						  ? (uint8_t)(((d->gamepad_buttons >> AERON_GAMEPAD_BUTTON_SOUTH) & 1) |
									  (((d->gamepad_buttons >> AERON_GAMEPAD_BUTTON_EAST) & 1) << 1))
						  : (uint8_t)(d->raw_buttons[0] & 3);
	uint8_t hat = d->hat_count ? d->raw_hats[0] : 0;
	if (d->kind == AERON_CONTROLLER_KIND_GAMEPAD)
		hat = ((d->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_UP)) ? 1 : 0) |
			  ((d->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_RIGHT)) ? 2 : 0) |
			  ((d->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_DOWN)) ? 4 : 0) |
			  ((d->gamepad_buttons & (1u << AERON_GAMEPAD_BUTTON_DPAD_LEFT)) ? 8 : 0);
	uint8_t down = buttons | (hat << 2);
	if (!state->primed)
		state->menu_blocked = down;
	state->menu_blocked &= down;
	down &= (uint8_t)~state->menu_blocked;
	g_controller.menu_buttons |= down & 3;
	if (!g_controller.menu_hat)
		g_controller.menu_hat = down >> 2;
}

static void LogUnavailableControls(const XvtControllerModel* model, const AeronControllerSnapshot* device) {
	int missing = 0;
	for (int axis = 0; axis < XVT_INPUT_AXIS_COUNT; ++axis) {
		int source = model->profile.mapping.axes[axis].source;
		if (source >= 0 && !Aeron_ControllerAxisAvailable(device, source))
			++missing;
	}
	for (size_t i = 0; i < model->profile.binding_count; ++i) {
		const AeronControllerDigitalSource* source = &model->profile.bindings[i].source;
		bool available = false;
		if (source->kind == AERON_CONTROLLER_DIGITAL_BUTTON)
			available = model->kind == AERON_CONTROLLER_KIND_GAMEPAD
							? (device->gamepad_available_buttons & (1u << source->index)) != 0
							: source->index < device->button_count;
		else if (source->kind == AERON_CONTROLLER_DIGITAL_HAT)
			available = source->index < device->hat_count;
		else
			available = Aeron_ControllerAxisAvailable(device, source->index);
		if (!available)
			++missing;
	}
	if (missing || device->controls_truncated)
		Aeron_LogWarn("xvt.input", "controller '%s': %d unavailable configured controls%s", device->name,
					  missing, device->controls_truncated ? ", hardware controls truncated" : "");
}

static void SampleDigital(ControllerInstance* state, const AeronControllerSnapshot* device) {
	const XvtControllerModel* model = &g_controller.options.models[state->model];
	const XvtControllerProfile* profile = &model->profile;
	if (!state->primed)
		LogUnavailableControls(model, device);
	SampleMenu(state, device);
	for (size_t i = 0; i < profile->binding_count; ++i) {
		bool down = Aeron_ControllerDigitalSourceDown(device, &profile->bindings[i].source, state->down[i]);
		if (!state->primed)
			state->armed[i] = !down;
		else if (!down) {
			if (state->dispatched[i])
				XvtControllerMapping_Dispatch(profile->bindings[i].action, false);
			state->dispatched[i] = false;
			state->armed[i] = true;
		} else if (state->armed[i] && !state->down[i]) {
			XvtControllerMapping_Dispatch(profile->bindings[i].action, true);
			state->dispatched[i] = true;
		}
		state->down[i] = down;
	}
	state->primed = true;
}

uint16_t XvtControllerMapping_ThrottlePosition(int16_t raw, AeronControllerKind kind, int source,
											   bool invert) {
	const uint32_t end_tolerance = (uint32_t)ceilf(0.5f * UINT16_MAX / 100.0f);
	const uint32_t range = UINT16_MAX - 2 * end_tolerance;
	uint32_t position = (int32_t)raw + 32768;
	if (kind == AERON_CONTROLLER_KIND_GAMEPAD && source >= AERON_GAMEPAD_AXIS_LEFT_TRIGGER)
		position = ((uint32_t)(raw < 0 ? 0 : raw) * UINT16_MAX + 16383u) / 32767u;
	if (invert)
		position = UINT16_MAX - position;
	/* Saturate both endpoints and scale the remaining travel continuously. */
	if (position <= end_tolerance)
		return 0;
	if (position >= UINT16_MAX - end_tolerance)
		return UINT16_MAX;
	return (uint16_t)(((position - end_tolerance) * UINT16_MAX + range / 2) / range);
}

static void SampleAnalog(size_t index, const AeronControllerSnapshot* device) {
	const XvtControllerModel* model = &g_controller.options.models[index];
	for (int axis = 0; axis < XVT_INPUT_AXIS_COUNT; ++axis) {
		XvtInputAxisBinding binding = model->profile.mapping.axes[axis];
		if (!Aeron_ControllerAxisAvailable(device, binding.source))
			continue;
		int16_t raw = Aeron_ControllerAxisValue(device, binding.source);
		if (axis == XVT_INPUT_AXIS_THROTTLE) {
			g_controller.throttle =
				XvtControllerMapping_ThrottlePosition(raw, model->kind, binding.source, binding.invert);
			g_controller.throttle_valid = true;
			if (g_controller.throttle_instance != device->instance_id) {
				g_controller.throttle_instance = device->instance_id;
				++g_controller.generation;
			}
		} else {
			float normalized = (float)raw / 32768;
			int value = fabsf(normalized) > binding.deadzone ? (int)(normalized * 127) : 0;
			g_controller.menu_axes[axis] = binding.invert ? -value : value;
			g_controller.axes[axis] =
				XvtControllerOptions_EffectiveAxisInvert(model->kind, (XvtInputAxis)axis, binding.invert)
					? -value
					: value;
		}
	}
}

void XvtControllerMapping_Update(const AeronInputSnapshot* input) {
	if (!input || input->frame_id == g_controller.frame)
		return;
	g_controller.frame = input->frame_id;
	if (!input->has_focus || XvtInput_IsCaptured() || Aeron_DebugUiVisible())
		XvtControllerMapping_Suspend();
	else
		XvtControllerMapping_Resume();
	g_controller.present = false;
	g_controller.throttle_valid = false;
	memset(g_controller.axes, 0, sizeof g_controller.axes);
	memset(g_controller.menu_axes, 0, sizeof g_controller.menu_axes);
	g_controller.menu_buttons = g_controller.menu_hat = 0;
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		ControllerInstance* state = &g_controller.instances[i];
		bool found = false;
		for (int j = 0; j < AERON_CONTROLLER_MAX && state->id; ++j) {
			const AeronControllerSnapshot* d = &input->controllers[j];
			const XvtControllerModel* m = &g_controller.options.models[state->model];
			if (d->connected && d->instance_id == state->id && !strcmp(d->guid, m->guid) &&
				d->kind == m->kind)
				found = true;
		}
		if (!found)
			ReleaseInstance(state);
	}

	for (size_t model = 0; model < g_controller.options.count; ++model) {
		const AeronControllerSnapshot* analog = XvtControllerMapping_Resolve(
			&g_controller.options.models[model], input, g_controller.analog[model]);
		g_controller.analog[model] = analog ? analog->instance_id : 0;
		if (analog) {
			g_controller.present = true;
			if (!g_controller.suspended)
				SampleAnalog(model, analog);
		}
		if (g_controller.suspended)
			continue;
		uint32_t previous = 0;
		for (int n = 0; n < AERON_CONTROLLER_MAX; ++n) {
			const AeronControllerSnapshot* d = NULL;
			for (int j = 0; j < AERON_CONTROLLER_MAX; ++j) {
				const AeronControllerSnapshot* candidate = &input->controllers[j];
				if (candidate->connected && candidate->instance_id > previous &&
					!strcmp(candidate->guid, g_controller.options.models[model].guid) &&
					candidate->kind == g_controller.options.models[model].kind &&
					(!d || candidate->instance_id < d->instance_id))
					d = candidate;
			}
			if (!d)
				break;
			previous = d->instance_id;
			ControllerInstance* state = NULL;
			for (int j = 0; j < AERON_CONTROLLER_MAX; ++j)
				if (g_controller.instances[j].id == d->instance_id)
					state = &g_controller.instances[j];
			if (!state)
				for (int j = 0; j < AERON_CONTROLLER_MAX; ++j)
					if (!g_controller.instances[j].id) {
						state = &g_controller.instances[j];
						state->id = d->instance_id;
						state->model = (int)model;
						break;
					}
			if (state)
				SampleDigital(state, d);
		}
	}
	if (!g_controller.throttle_valid)
		g_controller.throttle_instance = 0;
}

uint32_t XvtControllerMapping_AnalogInstance(const char* guid) {
	int i = XvtControllerOptions_FindModel(&g_controller.options, guid);
	return i < 0 ? 0 : g_controller.analog[i];
}

bool XvtControllerMapping_ThrottleSample(uint16_t* position, uint32_t* generation) {
	*position = g_controller.throttle;
	*generation = g_controller.generation;
	return g_controller.throttle_valid;
}

int XvtControllerMapping_Present(void) { return g_controller.present; }

int XvtControllerMapping_Axis(XvtInputAxis axis) { return (unsigned)axis < 3 ? g_controller.axes[axis] : 0; }

int XvtControllerMapping_MenuAxis(XvtInputAxis axis) {
	return (unsigned)axis < 3 ? g_controller.menu_axes[axis] : 0;
}

uint8_t XvtControllerMapping_MenuButtons(void) { return g_controller.menu_buttons; }

uint8_t XvtControllerMapping_MenuHat(void) { return g_controller.menu_hat; }

uint16_t XvtControllerMapping_Modifiers(void) {
	return (g_controller.holds[XVT_INPUT_ACTION_FIRE_WEAPON] ? 1 : 0) |
		   (g_controller.holds[XVT_INPUT_ACTION_TARGET_ROLL_MODIFIER] ? 2 : 0);
}

uint16_t XvtControllerMapping_ReadKey(void) {
	if (g_controller.read == g_controller.write)
		return 0;
	uint16_t key = g_controller.queue[g_controller.read];
	g_controller.read = (g_controller.read + 1) % ACTION_QUEUE_CAPACITY;
	return key;
}

void XvtControllerMapping_ReleaseCommands(void) {
	g_controller.read = g_controller.write = 0;
	memset(g_controller.holds, 0, sizeof g_controller.holds);
	for (int i = 0; i < AERON_CONTROLLER_MAX; ++i) {
		ControllerInstance* state = &g_controller.instances[i];
		for (size_t j = 0; j < XVT_CONTROLLER_BINDING_CAP; ++j) {
			state->dispatched[j] = false;
			state->armed[j] = !state->down[j];
		}
	}
}
