#include "xvt_runtime/input/controller_options.h"

#include "aeron/log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool XvtControllerOptions_SameSource(const AeronControllerDigitalSource* left,
											const AeronControllerDigitalSource* right) {
	return left->kind == right->kind && left->index == right->index &&
		   (left->kind != AERON_CONTROLLER_DIGITAL_HAT || left->hat_direction == right->hat_direction);
}

bool XvtControllerOptions_ProfileEqual(const XvtControllerProfile* left, const XvtControllerProfile* right) {
	if (left->binding_count != right->binding_count)
		return false;
	for (int axis = 0; axis < XVT_INPUT_AXIS_COUNT; ++axis) {
		const XvtInputAxisBinding* a = &left->mapping.axes[axis];
		const XvtInputAxisBinding* b = &right->mapping.axes[axis];
		if (a->source != b->source || a->invert != b->invert || a->deadzone != b->deadzone)
			return false;
	}
	for (size_t index = 0; index < left->binding_count; ++index) {
		const XvtInputActionBinding* a = &left->bindings[index];
		const XvtInputActionBinding* b = &right->bindings[index];
		if (a->action != b->action || !XvtControllerOptions_SameSource(&a->source, &b->source) ||
			a->source.threshold != b->source.threshold)
			return false;
	}
	return true;
}

bool XvtControllerOptions_Equals(const XvtControllerOptions* left, const XvtControllerOptions* right) {
	if (!left || !right)
		return left == right;
	if (left->count != right->count)
		return false;
	for (size_t i = 0; i < left->count; ++i) {
		const XvtControllerModel *a = &left->models[i], *b = &right->models[i];
		if (strcmp(a->guid, b->guid) || strcmp(a->name, b->name) || a->kind != b->kind ||
			a->configured != b->configured || !XvtControllerOptions_ProfileEqual(&a->profile, &b->profile))
			return false;
	}
	return true;
}

bool XvtControllerOptions_EffectiveAxisInvert(AeronControllerKind kind, XvtInputAxis axis, bool invert) {
	/* Gamepad Y is positive downward, while positive flight pitch raises the nose. */
	return kind == AERON_CONTROLLER_KIND_GAMEPAD && axis == XVT_INPUT_AXIS_PITCH ? !invert : invert;
}

static bool XvtControllerOptions_ValidationError(char* error, size_t capacity, const char* message) {
	if (error && capacity)
		snprintf(error, capacity, "%s", message);
	return false;
}

bool XvtControllerOptions_ValidateProfile(const XvtControllerProfile* profile, AeronControllerKind kind,
										  char* error, size_t capacity) {
	const int axis_limit =
		kind == AERON_CONTROLLER_KIND_GAMEPAD ? AERON_GAMEPAD_AXIS_COUNT : AERON_CONTROLLER_AXIS_MAX;
	const int button_limit =
		kind == AERON_CONTROLLER_KIND_GAMEPAD ? AERON_GAMEPAD_BUTTON_COUNT : AERON_CONTROLLER_BUTTON_MAX;
	if (!profile || (kind != AERON_CONTROLLER_KIND_GAMEPAD && kind != AERON_CONTROLLER_KIND_JOYSTICK))
		return XvtControllerOptions_ValidationError(error, capacity, "invalid controller profile");
	if (profile->binding_count > XVT_CONTROLLER_BINDING_CAP)
		return XvtControllerOptions_ValidationError(error, capacity, "controller binding capacity exceeded");
	for (int axis = 0; axis < XVT_INPUT_AXIS_COUNT; ++axis) {
		const XvtInputAxisBinding* binding = &profile->mapping.axes[axis];
		if (binding->source < -1 || binding->source >= axis_limit || !isfinite(binding->deadzone) ||
			binding->deadzone < 0.0f || binding->deadzone > (axis == XVT_INPUT_AXIS_THROTTLE ? 0.0f : 1.0f))
			return XvtControllerOptions_ValidationError(error, capacity,
														"controller axis binding is invalid");
		if (binding->source < 0)
			continue;
		for (int prior = 0; prior < axis; ++prior)
			if (profile->mapping.axes[prior].source == binding->source)
				return XvtControllerOptions_ValidationError(
					error, capacity, "controller axis source is assigned more than once");
	}
	for (size_t index = 0; index < profile->binding_count; ++index) {
		const XvtInputActionBinding* binding = &profile->bindings[index];
		const AeronControllerDigitalSource* source = &binding->source;
		if (binding->action <= XVT_INPUT_ACTION_NONE || binding->action >= XVT_INPUT_ACTION_COUNT)
			return XvtControllerOptions_ValidationError(error, capacity,
														"controller action binding is invalid");
		if (!isfinite(source->threshold) || source->threshold <= 0.0f || source->threshold > 1.0f)
			return XvtControllerOptions_ValidationError(error, capacity,
														"controller digital threshold is invalid");
		switch (source->kind) {
			case AERON_CONTROLLER_DIGITAL_BUTTON:
				if (source->index >= button_limit)
					return XvtControllerOptions_ValidationError(error, capacity,
																"controller button index is out of range");
				break;
			case AERON_CONTROLLER_DIGITAL_AXIS_POSITIVE:
			case AERON_CONTROLLER_DIGITAL_AXIS_NEGATIVE:
				if (source->index >= axis_limit)
					return XvtControllerOptions_ValidationError(error, capacity,
																"controller digital axis is invalid");
				break;
			case AERON_CONTROLLER_DIGITAL_HAT:
				if (kind != AERON_CONTROLLER_KIND_JOYSTICK || source->index >= AERON_CONTROLLER_HAT_MAX ||
					(source->hat_direction != AERON_CONTROLLER_HAT_UP &&
					 source->hat_direction != AERON_CONTROLLER_HAT_RIGHT &&
					 source->hat_direction != AERON_CONTROLLER_HAT_DOWN &&
					 source->hat_direction != AERON_CONTROLLER_HAT_LEFT))
					return XvtControllerOptions_ValidationError(error, capacity,
																"controller hat binding is invalid");
				break;
			default:
				return XvtControllerOptions_ValidationError(error, capacity,
															"controller source kind is invalid");
		}
		for (size_t prior = 0; prior < index; ++prior)
			if (XvtControllerOptions_SameSource(source, &profile->bindings[prior].source))
				return XvtControllerOptions_ValidationError(error, capacity,
															"controller source is bound more than once");
	}
	return true;
}

void XvtControllerOptions_ClearProfile(XvtControllerProfile* profile, AeronControllerKind kind) {
	memset(profile, 0, sizeof *profile);
	for (int i = 0; i < XVT_INPUT_AXIS_COUNT; ++i)
		profile->mapping.axes[i].source = -1;
	/* Raw HOTAS levers commonly decrease their reported value toward full power. */
	profile->mapping.axes[XVT_INPUT_AXIS_THROTTLE].invert = kind == AERON_CONTROLLER_KIND_JOYSTICK;
}

int XvtControllerOptions_FindModel(const XvtControllerOptions* options, const char* guid) {
	if (options && guid)
		for (size_t i = 0; i < options->count; ++i)
			if (!strcmp(options->models[i].guid, guid))
				return (int)i;
	return -1;
}

static bool GuidValid(const char* guid) {
	bool nonzero = false;
	for (int i = 0; i < 32; ++i) {
		if (!((guid[i] >= '0' && guid[i] <= '9') || (guid[i] >= 'a' && guid[i] <= 'f')))
			return false;
		nonzero |= guid[i] != '0';
	}
	return nonzero && guid[32] == 0;
}

bool XvtControllerOptions_Validate(const XvtControllerOptions* options, char* error, size_t capacity) {
	if (!options || options->count > XVT_CONTROLLER_MODEL_CAP)
		return XvtControllerOptions_ValidationError(error, capacity, "controller model capacity exceeded");
	for (size_t i = 0; i < options->count; ++i) {
		const XvtControllerModel* m = &options->models[i];
		if (!GuidValid(m->guid) || !memchr(m->name, 0, sizeof m->name))
			return XvtControllerOptions_ValidationError(error, capacity, "invalid controller model identity");
		if (!XvtControllerOptions_ValidateProfile(&m->profile, m->kind, error, capacity))
			return false;
		for (size_t j = 0; j < i; ++j)
			if (!strcmp(m->guid, options->models[j].guid))
				return XvtControllerOptions_ValidationError(error, capacity,
															"duplicate controller model GUID");
	}
	return true;
}

static bool XvtControllerOptions_ModelConnected(const XvtControllerModel* model,
												const AeronInputSnapshot* input) {
	for (int slot = 0; input && slot < AERON_CONTROLLER_MAX; ++slot) {
		const AeronControllerSnapshot* d = &input->controllers[slot];
		if (d->connected && d->kind == model->kind && !strcmp(d->guid, model->guid))
			return true;
	}
	return false;
}

static void XvtControllerOptions_RemoveModel(XvtControllerOptions* options, size_t index) {
	memmove(&options->models[index], &options->models[index + 1],
			(options->count - index - 1) * sizeof options->models[0]);
	--options->count;
}

/* Frees one entry. A model that isn't configured goes first, since nothing
 * is lost. Then the configured model that was connected the longest ago. */
static bool XvtControllerOptions_MakeRoom(XvtControllerOptions* options, const AeronInputSnapshot* input) {
	for (int pass = 0; pass < 2; ++pass) {
		for (size_t i = options->count; i-- > 0;) {
			const XvtControllerModel* m = &options->models[i];
			if ((pass == 0 && m->configured) || XvtControllerOptions_ModelConnected(m, input))
				continue;
			if (m->configured)
				Aeron_LogInfo("xvt.input",
							  "forgetting the settings of '%s' to make room for a new controller", m->name);
			XvtControllerOptions_RemoveModel(options, i);
			return true;
		}
	}
	return false;
}

bool XvtControllerOptions_AddModel(XvtControllerOptions* options, const AeronControllerSnapshot* device,
								   const AeronInputSnapshot* input, char* error, size_t capacity) {
	if (!device || !GuidValid(device->guid))
		return XvtControllerOptions_ValidationError(error, capacity, "controller has no usable model GUID");
	if (XvtControllerOptions_FindModel(options, device->guid) >= 0)
		return true;
	if (options->count >= XVT_CONTROLLER_MODEL_CAP && !XvtControllerOptions_MakeRoom(options, input))
		return XvtControllerOptions_ValidationError(error, capacity, "controller model capacity exceeded");
	XvtControllerModel* m = &options->models[options->count++];
	memset(m, 0, sizeof *m);
	memcpy(m->guid, device->guid, sizeof m->guid);
	snprintf(m->name, sizeof m->name, "%s", device->name);
	m->kind = device->kind;
	XvtControllerOptions_ClearProfile(&m->profile, device->kind);
	return true;
}

void XvtControllerOptions_MoveConnectedFirst(XvtControllerOptions* options, const AeronInputSnapshot* input) {
	static XvtControllerOptions sorted;
	size_t count = 0;
	if (!input)
		return;
	for (int pass = 0; pass < 2; ++pass)
		for (size_t i = 0; i < options->count; ++i)
			if (XvtControllerOptions_ModelConnected(&options->models[i], input) == (pass == 0))
				sorted.models[count++] = options->models[i];
	memcpy(options->models, sorted.models, count * sizeof options->models[0]);
}

void XvtControllerOptions_KeepConfigured(XvtControllerOptions* options) {
	for (size_t i = options->count; i-- > 0;)
		if (!options->models[i].configured)
			XvtControllerOptions_RemoveModel(options, i);
}

bool XvtControllerOptions_InitializeGamepads(XvtControllerOptions* options,
											 const XvtControllerProfile* defaults,
											 const AeronInputSnapshot* input, char* error, size_t capacity) {
	const AeronControllerSnapshot* sorted[AERON_CONTROLLER_MAX];
	int count = 0;
	if (!input)
		return true;
	for (int slot = 0; slot < AERON_CONTROLLER_MAX; ++slot) {
		const AeronControllerSnapshot* d = &input->controllers[slot];
		if (!d->connected || d->kind != AERON_CONTROLLER_KIND_GAMEPAD)
			continue;
		int j = count++;
		while (j > 0 && strcmp(sorted[j - 1]->guid, d->guid) > 0) {
			sorted[j] = sorted[j - 1];
			--j;
		}
		sorted[j] = d;
	}
	for (int i = 0; i < count; ++i) {
		if (XvtControllerOptions_FindModel(options, sorted[i]->guid) >= 0)
			continue;
		if (!XvtControllerOptions_AddModel(options, sorted[i], input, error, capacity))
			return false;
		/* Other models may bind the same axes; the mapping merges them. */
		options->models[options->count - 1].profile = *defaults;
	}
	return true;
}
