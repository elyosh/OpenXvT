#ifndef XVT_APP_DUO_POSE_H
#define XVT_APP_DUO_POSE_H

typedef struct XvtDuoSplit {
	int active;
	/* Drawable pixels. The game occupies y < game_bottom. Controls occupy
	 * y >= controls_top. The band between them is the hinge. */
	int game_bottom;
	int controls_top;
	int pixel_w;
	int pixel_h;
} XvtDuoSplit;

/* Nonzero when the iPhone Duo hinge is in the laptop split and the window
 * spans both halves. */
int XvtDuoPose_Query(XvtDuoSplit* out);

#endif
