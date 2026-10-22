#include "xvt_app/application.h"

#include "aeron/main.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
	XvtLaunchOptions options;
	int valid = XvtLaunchOptions_Parse(argc, argv, &options);
	if (!valid || options.show_help) {
		fprintf(valid ? stdout : stderr,
				"OpenXvT " OPENXVT_VERSION "\n"
				"Usage: OpenXvT [options]\n"
				"  --game-data <directory>     XvT root containing BalanceOfPower\n"
				"  --setup                     Choose and remember an installation\n"
				"  --skip-intro                Start at the pilot concourse\n"
				"  --save-config               Persist the supplied game-data path\n"
				"  --reset-config              Explicitly replace config.yaml with defaults\n"
				"  --import-config <path>      Import a config.cfg/config2.cfg relative to XvT root\n"
				"  --import-pilot <path>       Copy a selected .plt/.pl2 and companion into USER\n"
				"  --pilot-name <basename>     Resolve an import filename collision\n"
				"  --check-installation       Validate/setup from CLI and exit without a window\n"
				"  --resource-root <directory> Override packaged resources\n"
				"  --help                      Show this help\n");
		return valid ? 0 : 2;
	}
	int code = XvtApplication_Run(&options);
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
	/* SDL returns to UIKit after main and leaves the process running. The
	 * window is already destroyed, so Exit Game would sit on a black scene. */
	exit(code);
#endif

	return code;
}
