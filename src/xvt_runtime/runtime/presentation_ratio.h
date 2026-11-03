#ifndef XVT_RUNTIME_PRESENTATION_RATIO_H
#define XVT_RUNTIME_PRESENTATION_RATIO_H

#include <stdint.h>

/* Even logical width of a 480-tall frame for this pixel box. keep_floor holds
 * the classic 640 minimum used by an unsplit window. A split pane passes 0
 * so a tall pane may be narrower than 640 and still fill. */
static inline int XvtPresentation_WidthForBox(int box_width, int box_height, int keep_floor) {
	int w;
	int cap = (32 * 480 / 9) & ~1;
	if (box_width <= 0 || box_height <= 0)

		return 0;
	w = (int)(((int64_t)480 * box_width + box_height / 2) / box_height) & ~1;
	if (w < 2)
		w = 2;
	if (keep_floor && w < 640)
		w = 640;
	if (w > cap)
		w = cap;

	return w;
}

#endif
