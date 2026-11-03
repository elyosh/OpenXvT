#include "xvt_runtime/input/touch_tuning.h"

#include <math.h>

static int XvtTouchTuning_InRange(double value, double low, double high) {
	if (!isfinite(value))

		return 0;
	if (value < low - 1e-6 || value > high + 1e-6)

		return 0;

	return 1;
}

static float XvtTouchTuning_ClampScale(float scale) {
	if (scale < (float)XVT_TOUCH_SCALE_MIN)

		return (float)XVT_TOUCH_SCALE_MIN;
	if (scale > (float)XVT_TOUCH_SCALE_MAX)

		return (float)XVT_TOUCH_SCALE_MAX;

	return scale;
}

int XvtTouchTuning_ValidDeadzone(double value) {
	return XvtTouchTuning_InRange(value, 0.0, XVT_TOUCH_STICK_DEADZONE_MAX);
}

int XvtTouchTuning_ValidSensitivity(double value) {
	return XvtTouchTuning_InRange(value, XVT_TOUCH_STICK_SENSITIVITY_MIN, XVT_TOUCH_STICK_SENSITIVITY_MAX);
}

int XvtTouchTuning_ValidScale(double value) {
	return XvtTouchTuning_InRange(value, XVT_TOUCH_SCALE_MIN, XVT_TOUCH_SCALE_MAX);
}

float XvtTouchTuning_PinchScale(float start_scale, float start_distance, float distance) {
	if (!isfinite(start_scale))
		start_scale = (float)XVT_TOUCH_SCALE_DEFAULT;
	if (!(start_distance >= XVT_TOUCH_PINCH_MIN_DISTANCE) || !isfinite(distance))

		return XvtTouchTuning_ClampScale(start_scale);

	return XvtTouchTuning_ClampScale(start_scale * distance / start_distance);
}

void XvtTouchStick_Shape(float* x, float* y, float deadzone, float sensitivity) {
	float ax;
	float ay;
	float magnitude;
	if (!isfinite(*x))
		*x = 0.0f;
	if (!isfinite(*y))
		*y = 0.0f;
	if (!isfinite(deadzone))
		deadzone = XVT_TOUCH_STICK_DEADZONE_DEFAULT;
	if (!isfinite(sensitivity))
		sensitivity = (float)XVT_TOUCH_STICK_SENSITIVITY_DEFAULT;
	if (deadzone < 0.0f)
		deadzone = 0.0f;
	if (deadzone > (float)XVT_TOUCH_STICK_DEADZONE_MAX)
		deadzone = (float)XVT_TOUCH_STICK_DEADZONE_MAX;
	if (sensitivity < (float)XVT_TOUCH_STICK_SENSITIVITY_MIN)
		sensitivity = (float)XVT_TOUCH_STICK_SENSITIVITY_MIN;
	if (sensitivity > (float)XVT_TOUCH_STICK_SENSITIVITY_MAX)
		sensitivity = (float)XVT_TOUCH_STICK_SENSITIVITY_MAX;
	ax = fabsf(*x) > deadzone ? *x * sensitivity : 0.0f;
	ay = fabsf(*y) > deadzone ? *y * sensitivity : 0.0f;
	magnitude = sqrtf(ax * ax + ay * ay);
	if (magnitude > 1.0f) {
		ax /= magnitude;
		ay /= magnitude;
	}
	*x = ax;
	*y = ay;
}
