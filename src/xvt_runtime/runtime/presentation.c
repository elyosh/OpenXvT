#include "xvt_runtime/runtime/presentation.h"

#include "aeron/compat/host.h"
#include "xvt_runtime/runtime/movie_task.h"

static AeronRectI g_frame = { 0, 0, XVT_CLASSIC_WIDTH, XVT_CLASSIC_HEIGHT };

static AeronDx5Rect ClassicRect(void* context, int width, int height) {
	AeronRectI rect = XvtPresentation_ClassicRect();
	(void)context;
	(void)width;
	(void)height;
	return (AeronDx5Rect) { rect.x, rect.y, rect.width, rect.height };
}

void XvtPresentation_SyncToWindow(int width, int height) {
	if (width <= 0 || height <= 0)
		return;
	int w = (int)(((int64_t)480 * width + height / 2) / height) & ~1;
	if (w < 640)
		w = 640;
	if (w > 32 * 480 / 9)
		w = (32 * 480 / 9) & ~1;
	if (g_frame.width != w) {
		g_frame.width = w;
		Aeron_SetLogicalSize(w, 480);
	}
}

AeronRectI XvtPresentation_Frame(void) { return g_frame; }

AeronRectI XvtPresentation_ClassicRect(void) {
	return (AeronRectI) { (g_frame.width - 640) / 2, 0, 640, 480 };
}

AeronRectI XvtPresentation_FromClassic(AeronRectI r) {
	r.x += XvtPresentation_ClassicRect().x;
	return r;
}

int XvtPresentation_MouseToClassic(const AeronInputSnapshot* in, int* x, int* y) {
	AeronRectI classic;
	int px, py;
	if (!in || !x || !y || !in->mouse.inside_content)

		return 0;
	/* mouse.x/y already sit in the aspect-fitted content rect, including the
	 * upper-half slot used while the Duo is in the laptop pose. */
	classic = XvtPresentation_ClassicRect();
	px = in->mouse.x - classic.x;
	py = in->mouse.y - classic.y;
	if (px < 0 || py < 0 || px >= classic.width || py >= classic.height)

		return 0;
	*x = px;
	*y = py;

	return 1;
}

int XvtPresentation_WarpClassic(int x, int y) {
	return Aeron_WarpMouseLogical(x + XvtPresentation_ClassicRect().x, y);
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
