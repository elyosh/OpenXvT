#ifndef XVT_APP_DUO_POSE_H
#define XVT_APP_DUO_POSE_H

typedef struct XvtDuoRect {
	int x, y, width, height;
} XvtDuoRect;

/* A fold band in drawable pixels. Distinct from a pane so a pane cannot be
 * passed back in as a band. */
typedef struct XvtDuoBand {
	int x, y, width, height;
} XvtDuoBand;

/* When active is 0, both rects are empty. pixel_w and pixel_h still name the
 * drawable. When active is 1, game is the upper screen and controls is the
 * lower screen. The two rects do not overlap. */
typedef struct XvtDuoSplit {
	int active;
	int pixel_w;
	int pixel_h;
	XvtDuoRect game;
	XvtDuoRect controls;
} XvtDuoSplit;

/* The last active split and how many frames the bands have been missing. */
typedef struct XvtDuoHold {
	XvtDuoSplit last;
	int missing;
} XvtDuoHold;

/* Nonzero when one band divides the window. upper_high is 1 when the upper
 * screen lies past the band (bottom or right) and 0 when it lies before the
 * band (top or left). A refusal writes a zero split. */
int XvtDuoSplit_FromBands(int pixel_w, int pixel_h, const XvtDuoBand* bands, int count, int upper_high,
						  XvtDuoSplit* out);
/* The complement of the game pane, in drawable pixels. Four zeros when the
 * split is missing, inactive, or the complement would be negative. */
void XvtDuoSplit_Insets(const XvtDuoSplit* split, int* left, int* top, int* right, int* bottom);
/* Keeps the last split through up to hold_frames frames without a band, or
 * until the drawable size changes. Writes the split to use this frame. */
void XvtDuoHold_Step(XvtDuoHold* hold, const XvtDuoSplit* raw, int hold_frames, XvtDuoSplit* out);
/* Nonzero when a fold band divides the window. Below iOS 27.1, returns 0. */
int XvtDuoPose_Query(XvtDuoSplit* out);

#endif
