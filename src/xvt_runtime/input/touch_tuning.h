#ifndef XVT_RUNTIME_INPUT_TOUCH_TUNING_H
#define XVT_RUNTIME_INPUT_TOUCH_TUNING_H

#ifdef __cplusplus
extern "C" {
#endif

#define XVT_TOUCH_STICK_DEADZONE_MAX 0.5
#define XVT_TOUCH_STICK_DEADZONE_DEFAULT 0.08f
#define XVT_TOUCH_STICK_SENSITIVITY_MIN 0.5
#define XVT_TOUCH_STICK_SENSITIVITY_DEFAULT 1.0
#define XVT_TOUCH_STICK_SENSITIVITY_MAX 2.0
#define XVT_TOUCH_SCALE_MIN 0.6
#define XVT_TOUCH_SCALE_DEFAULT 1.0
#define XVT_TOUCH_SCALE_MAX 1.6
#define XVT_TOUCH_PINCH_MIN_DISTANCE 24.0f

int XvtTouchTuning_ValidDeadzone(double value);
int XvtTouchTuning_ValidSensitivity(double value);
int XvtTouchTuning_ValidScale(double value);
float XvtTouchTuning_PinchScale(float start_scale, float start_distance, float distance);
void XvtTouchStick_Shape(float* x, float* y, float deadzone, float sensitivity);

#ifdef __cplusplus
}
#endif

#endif
