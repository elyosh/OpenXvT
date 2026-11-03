/* Standalone host test, not part of the CMake build. Run from the repository root:
 *   cc -std=c11 -Wall -Wextra -Werror -Isrc tools/touch_tuning_test.c
 *      src/xvt_runtime/input/touch_tuning.c -lm -o build/touch_tuning_test
 *   ./build/touch_tuning_test
 */
#include "xvt_runtime/input/touch_tuning.h"

#include <math.h>
#include <stdio.h>

static int g_failed;

static void Expect(int ok, const char* name) {
	if (ok)

		return;
	fprintf(stderr, "FAIL %s\n", name);
	g_failed = 1;
}

static int Close(float value, float expected) {
	return fabsf(value - expected) <= 1e-6f;
}

static void ExpectShape(const char* name, float x, float y, float deadzone, float sensitivity, float ex,
						float ey) {
	XvtTouchStick_Shape(&x, &y, deadzone, sensitivity);
	Expect(Close(x, ex) && Close(y, ey), name);
}

int main(void) {
	float x;
	float y;

	ExpectShape("gate holds 0.08", 0.08f, 0.0f, 0.08f, 1.0f, 0.0f, 0.0f);
	ExpectShape("gate passes 0.0801", 0.0801f, 0.0f, 0.08f, 1.0f, 0.0801f, 0.0f);
	ExpectShape("inside unit disk stays", 0.5f, -0.3f, 0.08f, 1.0f, 0.5f, -0.3f);
	ExpectShape("diagonal inside the gate", 0.05f, 0.05f, 0.08f, 1.0f, 0.0f, 0.0f);
	ExpectShape("gain two", 0.3f, 0.0f, 0.08f, 2.0f, 0.6f, 0.0f);
	ExpectShape("normalize diagonal", 0.9f, 0.9f, 0.08f, 2.0f, 0.70710678f, 0.70710678f);
	ExpectShape("sensitivity half", 1.0f, 0.0f, 0.08f, 0.5f, 0.5f, 0.0f);

	x = NAN;
	y = 0.0f;
	XvtTouchStick_Shape(&x, &y, 0.08f, 1.0f);
	Expect(x == 0.0f && y == 0.0f, "nan x");
	ExpectShape("nan deadzone uses 0.08", 0.08f, 0.0f, NAN, 1.0f, 0.0f, 0.0f);
	ExpectShape("nan deadzone passes 0.0801", 0.0801f, 0.0f, NAN, 1.0f, 0.0801f, 0.0f);
	ExpectShape("nan sensitivity uses 1", 0.3f, 0.0f, 0.08f, NAN, 0.3f, 0.0f);
	ExpectShape("sensitivity 10 clamps to 2", 0.3f, 0.0f, 0.08f, 10.0f, 0.6f, 0.0f);

	Expect(XvtTouchTuning_ValidDeadzone(0.0), "deadzone 0");
	Expect(XvtTouchTuning_ValidDeadzone(0.5), "deadzone 0.5");
	Expect(XvtTouchTuning_ValidDeadzone((double)0.5f), "deadzone float 0.5");
	Expect(XvtTouchTuning_ValidSensitivity(0.5), "sensitivity 0.5");
	Expect(XvtTouchTuning_ValidSensitivity(2.0), "sensitivity 2");
	Expect(XvtTouchTuning_ValidSensitivity((double)2.0f), "sensitivity float 2");
	Expect(XvtTouchTuning_ValidScale(0.6), "scale 0.6");
	Expect(XvtTouchTuning_ValidScale(1.6), "scale 1.6");
	Expect(XvtTouchTuning_ValidScale((double)0.6f), "scale float 0.6");
	Expect(XvtTouchTuning_ValidScale((double)1.6f), "scale float 1.6");
	Expect(!XvtTouchTuning_ValidScale(1.61), "scale 1.61");
	Expect(!XvtTouchTuning_ValidScale(0.59), "scale 0.59");
	Expect(!XvtTouchTuning_ValidDeadzone(0.501), "deadzone 0.501");
	Expect(!XvtTouchTuning_ValidSensitivity(2.01), "sensitivity 2.01");
	Expect(!XvtTouchTuning_ValidDeadzone(-0.001), "deadzone -0.001");
	Expect(!XvtTouchTuning_ValidDeadzone(NAN), "deadzone nan");
	Expect(!XvtTouchTuning_ValidSensitivity(NAN), "sensitivity nan");
	Expect(!XvtTouchTuning_ValidScale(NAN), "scale nan");
	Expect(!XvtTouchTuning_ValidDeadzone(INFINITY), "deadzone inf");
	Expect(!XvtTouchTuning_ValidSensitivity(INFINITY), "sensitivity inf");
	Expect(!XvtTouchTuning_ValidScale(INFINITY), "scale inf");

	Expect(Close(XvtTouchTuning_PinchScale(1.0f, 100.0f, 200.0f), 1.6f), "pinch doubles to the cap");
	Expect(Close(XvtTouchTuning_PinchScale(1.0f, 100.0f, 50.0f), 0.6f), "pinch halves to the floor");
	Expect(Close(XvtTouchTuning_PinchScale(1.0f, 100.0f, 60.0f), 0.6f), "pinch 60");
	Expect(Close(XvtTouchTuning_PinchScale(1.2f, 120.0f, 96.0f), 0.96f), "pinch 0.96");
	Expect(Close(XvtTouchTuning_PinchScale(1.2f, 10.0f, 80.0f), 1.2f), "short pinch keeps the start scale");

	if (g_failed)

		return 1;

	return 0;
}
