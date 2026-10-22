if(NOT DEFINED XVT_BUNDLE_CONTENT_DIR OR NOT DEFINED XVT_SHADER_SOURCE_DIR
        OR NOT DEFINED XVT_RESOURCE_DIR OR NOT DEFINED XVT_FONT_DIR
        OR NOT DEFINED XVT_AERON_DEFAULTS)
    message(FATAL_ERROR "iOS bundle staging paths are incomplete")
endif()

# simctl treats a bundle-root folder named Resources, resources, or Contents
# as a macOS bundle and then reports a missing bundle ID. The iOS host
# therefore looks up XVT_IOS_RESOURCE_PATH at the bundle root. shaders is a
# safe name. Remove a Resources directory left by an older stage.
file(REMOVE_RECURSE "${XVT_BUNDLE_CONTENT_DIR}/Resources")
set(game_resources "${XVT_BUNDLE_CONTENT_DIR}/gamedata")
set(shader_dir "${XVT_BUNDLE_CONTENT_DIR}/shaders")

file(GLOB resource_yaml "${XVT_RESOURCE_DIR}/*.yaml")
if(NOT resource_yaml)
    message(FATAL_ERROR "No resource yaml files in ${XVT_RESOURCE_DIR}")
endif()
file(GLOB font_files "${XVT_FONT_DIR}/*")
if(NOT font_files)
    message(FATAL_ERROR "No UI fonts in ${XVT_FONT_DIR}")
endif()
if(NOT EXISTS "${XVT_AERON_DEFAULTS}")
    message(FATAL_ERROR "Aeron defaults are missing: ${XVT_AERON_DEFAULTS}")
endif()

file(GLOB shader_files "${XVT_SHADER_SOURCE_DIR}/*.msl")
if(NOT shader_files)
    message(FATAL_ERROR "No compiled MSL shaders found in ${XVT_SHADER_SOURCE_DIR}")
endif()

file(MAKE_DIRECTORY
    "${game_resources}/aeron"
    "${game_resources}/assets/fonts"
    "${shader_dir}")
file(COPY ${resource_yaml} DESTINATION "${game_resources}")
file(COPY "${XVT_AERON_DEFAULTS}" DESTINATION "${game_resources}/aeron")
file(COPY ${font_files} DESTINATION "${game_resources}/assets/fonts")
file(COPY ${shader_files} DESTINATION "${shader_dir}")
