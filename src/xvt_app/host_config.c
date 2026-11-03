#include "xvt_app/host_config.h"
#include "xvt_app/window_icon.h"

#include <stdio.h>
#include <string.h>

int XvtLaunchOptions_Parse(int argc, char* argv[], XvtLaunchOptions* options) {
	memset(options, 0, sizeof(*options));
	for (int index = 1; index < argc; ++index) {
		const char* argument = argv[index];
		const char* equals = strchr(argument, '=');
		size_t length = equals ? (size_t)(equals - argument) : strlen(argument);
		const char** destination = NULL;

		struct {
			const char* name;
			const char** value;
		} paths[] = { { "--resource-root", &options->resource_root },
					  { "--game-data", &options->game_data },
					  { "--import-config", &options->import_config },
					  { "--import-pilot", &options->import_pilot },
					  { "--pilot-name", &options->pilot_name } };

		struct {
			const char* name;
			int* value;
		} flags[] = { { "--help", &options->show_help },
					  { "-h", &options->show_help },
					  { "--setup", &options->setup },
					  { "--skip-intro", &options->skip_intro },
					  { "--save-config", &options->save_config },
					  { "--reset-config", &options->reset_config },
					  { "--check-installation", &options->check_installation } };

		int handled = 0;
		for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
			if (!strcmp(argument, flags[i].name)) {
				*flags[i].value = 1;
				handled = 1;
				break;
			}
		}
		if (handled)
			continue;
		for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
			if (strlen(paths[i].name) == length && !strncmp(argument, paths[i].name, length)) {
				destination = paths[i].value;
				break;
			}
		}
		if (!destination) {
			fprintf(stderr, "OpenXvT: unknown argument: %s\n", argument);
			return 0;
		}
		const char* value = equals ? equals + 1 : ++index < argc ? argv[index] : NULL;
		if (*destination || !value || !value[0] || value[0] == '-') {
			fprintf(stderr, "OpenXvT: %.*s requires one nonempty argument\n", (int)length, argument);
			return 0;
		}
		*destination = value;
	}
	if ((options->pilot_name && !options->import_pilot) ||
		(options->setup && (options->check_installation || options->game_data))) {
		fprintf(stderr, "OpenXvT: --pilot-name requires --import-pilot; --setup cannot be combined with "
						"--game-data or --check-installation.\n");
		return 0;
	}
	return 1;
}

void XvtHostConfig_InitAeron(const XvtLaunchOptions* options, AeronConfig* config) {
	memset(config, 0, sizeof(*config));
	config->org_name = "TotallyOpen";
	config->app_name = "OpenXvT";
	config->app_version = OPENXVT_VERSION;
	config->window_title = "OpenXvT";
	config->window_icon_bmp = xvt_window_icon_bmp;
	config->window_icon_bmp_size = sizeof xvt_window_icon_bmp;
	config->resource_root = options ? options->resource_root : NULL;
#ifdef XVT_IOS_RESOURCE_PATH
	config->resource_path = XVT_IOS_RESOURCE_PATH;
#else
	config->resource_path = "resources";
#endif
	config->shader_path = "shaders";
	config->logical_width = 640;
	config->logical_height = 480;
	config->presentation_mode = AERON_PRESENTATION_ASPECT_FIT;
	config->clear_color_enabled = 1;
	config->clear_color_rgba[3] = 1.0f;
}

int XvtHostConfig_ResolveResourceRoot(const XvtLaunchOptions* options, char* out, size_t capacity) {
	AeronConfig config;
	XvtHostConfig_InitAeron(options, &config);
	if (config.resource_root && config.resource_root[0]) {
		if (strlen(config.resource_root) >= capacity)
			return 0;
		strcpy(out, config.resource_root);
		return 1;
	}
	return Aeron_ApplicationPath(config.resource_path, out, capacity);
}
