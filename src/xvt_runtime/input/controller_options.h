#ifndef XVT_RUNTIME_INPUT_CONTROLLER_OPTIONS_H
#define XVT_RUNTIME_INPUT_CONTROLLER_OPTIONS_H
#include "aeron/input.h"
#include "xvt_runtime/input/actions.h"
#include <stddef.h>

typedef enum XvtInputAxis {
	XVT_INPUT_AXIS_YAW,
	XVT_INPUT_AXIS_PITCH,
	XVT_INPUT_AXIS_ROLL,
	XVT_INPUT_AXIS_THROTTLE,
	XVT_INPUT_AXIS_COUNT
} XvtInputAxis;

typedef struct XvtInputAxisBinding {
	int8_t source;
	bool invert;
	float deadzone;
} XvtInputAxisBinding;

typedef struct XvtInputMapping {
	XvtInputAxisBinding axes[XVT_INPUT_AXIS_COUNT];
} XvtInputMapping;

enum {
	XVT_CONTROLLER_BINDING_CAP =
		AERON_CONTROLLER_BUTTON_MAX + 2 * AERON_CONTROLLER_AXIS_MAX + 4 * AERON_CONTROLLER_HAT_MAX
};

typedef struct XvtInputActionBinding {
	AeronControllerDigitalSource source;
	XvtInputAction action;
} XvtInputActionBinding;

typedef struct XvtControllerProfile {
	XvtInputMapping mapping;
	XvtInputActionBinding bindings[XVT_CONTROLLER_BINDING_CAP];
	size_t binding_count;
} XvtControllerProfile;

enum { XVT_CONTROLLER_MODEL_CAP = 16 };

typedef struct XvtControllerModel {
	char guid[33];
	char name[AERON_CONTROLLER_NAME_CAPACITY];
	AeronControllerKind kind; /* Saved Aeron kind; must match the connected snapshot. */
	/* Set when the player changed this model, or when it was loaded from
	 * the configuration. Only these models are saved. The others hold the
	 * defaults, which discovery gives them again at the next start. */
	bool configured;
	XvtControllerProfile profile;
} XvtControllerModel;

/* The models are kept in the order they were last connected, most recent
 * first. When the list is full, AddModel() drops the last one that isn't
 * connected. Several models may bind the same flight axis
 * (controller_mapping.c merges the connected ones). */
typedef struct XvtControllerOptions {
	XvtControllerModel models[XVT_CONTROLLER_MODEL_CAP];
	size_t count;
} XvtControllerOptions;

bool XvtControllerOptions_Equals(const XvtControllerOptions* left, const XvtControllerOptions* right);
bool XvtControllerOptions_ValidateProfile(const XvtControllerProfile* profile, AeronControllerKind kind,
										  char* error, size_t capacity);
bool XvtControllerOptions_EffectiveAxisInvert(AeronControllerKind kind, XvtInputAxis axis, bool invert);
bool XvtControllerOptions_ProfileEqual(const XvtControllerProfile* left, const XvtControllerProfile* right);
void XvtControllerOptions_ClearProfile(XvtControllerProfile* profile, AeronControllerKind kind);
int XvtControllerOptions_FindModel(const XvtControllerOptions* options, const char* guid);
bool XvtControllerOptions_Validate(const XvtControllerOptions* options, char* error, size_t capacity);
bool XvtControllerOptions_AddModel(XvtControllerOptions* options, const AeronControllerSnapshot* device,
								   const AeronInputSnapshot* input, char* error, size_t capacity);
/* Moves the connected models to the front, keeping the order of the others. */
void XvtControllerOptions_MoveConnectedFirst(XvtControllerOptions* options, const AeronInputSnapshot* input);
/* Removes the models that aren't configured. */
void XvtControllerOptions_KeepConfigured(XvtControllerOptions* options);
bool XvtControllerOptions_InitializeGamepads(XvtControllerOptions* options,
											 const XvtControllerProfile* defaults,
											 const AeronInputSnapshot* input, char* error, size_t capacity);
#endif
