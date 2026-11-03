#ifndef XVT_RUNTIME_INPUT_TOUCH_LAYOUT_H
#define XVT_RUNTIME_INPUT_TOUCH_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XvtTouchLayoutArea {
	float left, top, right, bottom;
} XvtTouchLayoutArea;

typedef struct XvtTouchLayoutRect {
	float x, y, w, h;
	int placed;
} XvtTouchLayoutRect;

/* Control indices are the caller's. -1 means absent. */
typedef struct XvtTouchLayoutPlan {
	int esc;
	int left;
	int right;
	int fire;
	const int* throttle;
	int throttle_count;
	const int* row_top;
	int row_top_count;
	const int* row_low;
	int row_low_count;
	const int* extras;
	int extras_count;
} XvtTouchLayoutPlan;

/* scales and rects both have count entries indexed by control index. Returns 1 when no scaled control
   overlaps another placed control, 0 otherwise. Returns 0 and places nothing for invalid input. */
int XvtTouchLayout_Place(const XvtTouchLayoutArea* area, float radius, const XvtTouchLayoutPlan* plan,
						 const float* scales, XvtTouchLayoutRect* rects, int count);

#ifdef __cplusplus
}
#endif

#endif
