#include "xvt_runtime/runtime/presentation.h"

#include "aeron/compat/host.h"
#include "xvt_runtime/runtime/movie_task.h"
#include "xvt_runtime/runtime/presentation_ratio.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
extern int Aeron_GetPresentationAvailablePixelSize(int* width, int* height);
#endif

static AeronRectI g_frame = { 0, 0, XVT_CLASSIC_WIDTH, XVT_CLASSIC_HEIGHT };

static AeronDx5Rect ClassicRect(void* context, int width, int height) {
	AeronRectI rect = XvtPresentation_ClassicRect();
	(void)context;
	(void)width;
	(void)height;
	return (AeronDx5Rect) { rect.x, rect.y, rect.width, rect.height };
}

void XvtPresentation_SyncToAvailable(int width, int height) {
	int avail_w = 0;
	int avail_h = 0;
	int box = 0;
	int w;
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
	box = Aeron_GetPresentationAvailablePixelSize(&avail_w, &avail_h);
#endif
	if (box) {
		width = avail_w;
		height = avail_h;
	}
	if (width <= 0 || height <= 0)

		return;
	w = XvtPresentation_WidthForBox(width, height, box != 2);
	if (w < 2)

		return;
	if (g_frame.width != w) {
		g_frame.width = w;
		Aeron_SetLogicalSize(w, 480);
	}
}

AeronRectI XvtPresentation_Frame(void) { return g_frame; }

AeronRectI XvtPresentation_ClassicRect(void) {
	int width = g_frame.width;
	int height;
	int y;
	if (width >= XVT_CLASSIC_WIDTH)
		return (AeronRectI) { (width - XVT_CLASSIC_WIDTH) / 2, 0, XVT_CLASSIC_WIDTH, XVT_CLASSIC_HEIGHT };
	if (width < 1)
		width = 1;
	height = (int)(((int64_t)width * XVT_CLASSIC_HEIGHT + XVT_CLASSIC_WIDTH / 2) / XVT_CLASSIC_WIDTH);
	if (height < 1)
		height = 1;
	if (height > XVT_CLASSIC_HEIGHT)
		height = XVT_CLASSIC_HEIGHT;
	y = (XVT_CLASSIC_HEIGHT - height) / 2;

	return (AeronRectI) { 0, y, width, height };
}

AeronRectI XvtPresentation_FromClassic(AeronRectI r) {
	AeronRectI classic = XvtPresentation_ClassicRect();
	r.x += classic.x;
	r.y += classic.y;

	return r;
}

int XvtPresentation_MouseToClassic(const AeronInputSnapshot* in, int* x, int* y) {
	AeronRectI classic;
	int px, py;
	if (!in || !x || !y || !in->mouse.inside_content)

		return 0;
	/* mouse.x and mouse.y already sit in the aspect-fitted content rect, which
	 * is the game pane while a Duo split is active. */
	classic = XvtPresentation_ClassicRect();
	px = in->mouse.x - classic.x;
	py = in->mouse.y - classic.y;
	if (px < 0 || py < 0 || px >= classic.width || py >= classic.height)

		return 0;
	if (classic.width != XVT_CLASSIC_WIDTH || classic.height != XVT_CLASSIC_HEIGHT) {
		px = (int)(((int64_t)px * XVT_CLASSIC_WIDTH) / classic.width);
		py = (int)(((int64_t)py * XVT_CLASSIC_HEIGHT) / classic.height);
	}
	*x = px;
	*y = py;

	return 1;
}

int XvtPresentation_WarpClassic(int x, int y) {
	AeronRectI classic = XvtPresentation_ClassicRect();
	int logical_x = x;
	int logical_y = y;
	if (classic.width != XVT_CLASSIC_WIDTH || classic.height != XVT_CLASSIC_HEIGHT) {
		logical_x = (int)(((int64_t)x * classic.width) / XVT_CLASSIC_WIDTH);
		logical_y = (int)(((int64_t)y * classic.height) / XVT_CLASSIC_HEIGHT);
	}

	return Aeron_WarpMouseLogical(logical_x + classic.x, logical_y + classic.y);
}

void XvtPresentation_RequireClassic(void) { AeronDx5_SetClassicFlightRenderingSuppressed(0); }

void XvtPresentation_Init(void) {
	AeronDx5Config config = { 0 };
	g_frame = (AeronRectI) { 0, 0, 640, 480 };
	config.presentation_rect = ClassicRect;
	AeronDx5_Configure(&config);
	AeronDx5_ResetPresentationState();
}

void XvtPresentation_EndFrame(int movie_presented) {
	if (!movie_presented && !XvtMovieTask_IsActive())
		AeronDx5_EndFrame();
}

void XvtPresentation_Shutdown(void) {
	AeronDx5_SetClassicFlightRenderingSuppressed(0);
	AeronDx5_Shutdown();
	AeronDx5_Configure(NULL);
}
