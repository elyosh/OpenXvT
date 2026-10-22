cmake_minimum_required(VERSION 3.20)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin"
        AND (NOT DEFINED CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version" FORCE)
endif()
project(OpenXvT C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Diagnostics.cmake)

# Apple SDKs deprecate sprintf, which recovered code retains for matching.
if(APPLE AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-deprecated-declarations)
endif()

option(XVT_BUILD_TOOLS "Build mission diagnostics and Aeron asset tools" OFF)
set(AERON_BUILD_TOOLS ${XVT_BUILD_TOOLS})
set(AERON_HOST_TOOLS_ONLY OFF)
option(XVT_ENABLE_DEBUG_UI "Build the Aeron ImGui debug overlay" OFF)
set(AERON_DEBUG_UI ${XVT_ENABLE_DEBUG_UI})
set(AERON_ENABLE_COMPAT ON)
set(AERON_ENABLE_DPLAY ON)
set(AERON_ENABLE_VIDEO ON)
set(AERON_ENABLE_AUDIO_DECODE ON)
add_subdirectory(aeron)

if(XVT_BUILD_TOOLS)
    add_executable(mission_dump tools/mission_dump.c)
    target_include_directories(mission_dump PRIVATE src aeron/include)
    if(MSVC)
        target_compile_options(mission_dump PRIVATE /W4)
    else()
        target_compile_options(mission_dump PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# Recovered original code. Every file here carries DecompMatch address tags
# and is byte-matched against z_bop__.exe.
set(XVT_RECOVERED_SOURCES
    src/xvt/assets/endian.c
    src/xvt/assets/file.c
    src/xvt/assets/inventor_ascii.c
    src/xvt/assets/memory_buffer.c
    src/xvt/assets/model_bounds.c
    src/xvt/assets/model_mesh.c
    src/xvt/assets/model_preview.c
    src/xvt/assets/model_texture.c
    src/xvt/assets/model_type.c
    src/xvt/assets/opt_model.c
    src/xvt/assets/string_table.c
    src/xvt/audio/cd_audio.c
    src/xvt/audio/direct_sound.c
    src/xvt/audio/frontend_sound.c
    src/xvt/audio/fsfx.c
    src/xvt/audio/music_cd.c
    src/xvt/audio/sound.c
    src/xvt/flight/ai/pai.c
    src/xvt/flight/ai/paifight.c
    src/xvt/flight/ai/paiman.c
    src/xvt/flight/ai/paiorder.c
    src/xvt/flight/craft.c
    src/xvt/flight/fediskio.c
    src/xvt/flight/flight.c
    src/xvt/flight/flight_display.c
    src/xvt/flight/flight_hyperspace.c
    src/xvt/flight/flight_input.c
    src/xvt/flight/flight_loading.c
    src/xvt/flight/flight_object.c
    src/xvt/flight/flight_render.c
    src/xvt/flight/flight_surface.c
    src/xvt/flight/flight_view.c
    src/xvt/flight/fview.c
    src/xvt/flight/hud/flight_alert.c
    src/xvt/flight/hud/flight_map.c
    src/xvt/flight/hud/flight_text.c
    src/xvt/flight/hud/hud.c
    src/xvt/flight/hud/mfd.c
    src/xvt/flight/hud/msg.c
    src/xvt/flight/mission/goals.c
    src/xvt/flight/mission/mission.c
    src/xvt/flight/object/collide.c
    src/xvt/flight/object/damage.c
    src/xvt/flight/object/laser.c
    src/xvt/flight/object/object.c
    src/xvt/flight/object/static.c
    src/xvt/flight/player/flight_player.c
    src/xvt/flight/player/player.c
    src/xvt/flight/proving_grounds.c
    src/xvt/flight/targeting.c
    src/xvt/flight/transfm2.c
    src/xvt/frontend/briefing_map.c
    src/xvt/frontend/briefing_script.c
    src/xvt/frontend/briefing_text.c
    src/xvt/frontend/concourse.c
    src/xvt/frontend/config.c
    src/xvt/frontend/credits.c
    src/xvt/frontend/cutscene.c
    src/xvt/frontend/ddutil.c
    src/xvt/frontend/front_image.c
    src/xvt/frontend/frontend.c
    src/xvt/frontend/frontend_bootstrap.c
    src/xvt/frontend/frontend_button.c
    src/xvt/frontend/frontend_cursor.c
    src/xvt/frontend/frontend_dialog.c
    src/xvt/frontend/frontend_display.c
    src/xvt/frontend/frontend_draw.c
    src/xvt/frontend/frontend_file_list.c
    src/xvt/frontend/frontend_flight.c
    src/xvt/frontend/frontend_joystick.c
    src/xvt/frontend/frontend_mission.c
    src/xvt/frontend/frontend_mission_list.c
    src/xvt/frontend/frontend_mouse.c
    src/xvt/frontend/frontend_screen.c
    src/xvt/frontend/frontend_scrollbar.c
    src/xvt/frontend/frontend_state.c
    src/xvt/frontend/frontend_string.c
    src/xvt/frontend/frontend_text.c
    src/xvt/frontend/mission_briefing.c
    src/xvt/frontend/mission_debrief.c
    src/xvt/frontend/mission_setup.c
    src/xvt/frontend/movie.c
    src/xvt/frontend/pilot.c
    src/xvt/frontend/pilot_record.c
    src/xvt/frontend/tech_library.c
    src/xvt/input/dinput.c
    src/xvt/input/input.c
    src/xvt/input/joystick.c
    src/xvt/input/keyboard.c
    src/xvt/input/mouse.c
    src/xvt/input/win_mouse.c
    src/xvt/math/math.c
    src/xvt/math/math2.c
    src/xvt/math/math3d.c
    src/xvt/math/trig2.c
    src/xvt/net/flight_net.c
    src/xvt/net/flight_sync.c
    src/xvt/net/frontend_net.c
    src/xvt/net/net.c
    src/xvt/net/net_reliable.c
    src/xvt/net/net_session.c
    src/xvt/render/backdrop.c
    src/xvt/render/color.c
    src/xvt/render/flight_light.c
    src/xvt/render/flight_palette.c
    src/xvt/render/flight_sw.c
    src/xvt/render/image_quantizer.c
    src/xvt/render/render_clip.c
    src/xvt/render/render_list.c
    src/xvt/render/render_quad.c
    src/xvt/render/render_scene.c
    src/xvt/render/render_texture.c
    src/xvt/render/renderer.c
    src/xvt/render/scene_billboard.c
    src/xvt/render/std3d.c
    src/xvt/render/sw3d.c
    src/xvt/render/tex_level.c
    src/xvt/util/debug_console.c
    src/xvt/util/game_rand.c
    src/xvt/util/memory.c
    src/xvt/util/time.c
    src/xvt/util/win32.c
)

set(XVT_RUNTIME_SOURCES
    src/xvt_runtime/assets/opt_relocate.c
    src/xvt_runtime/assets/opt_native.c
    src/xvt_runtime/config/config.c
    src/xvt_runtime/config/settings.c
    src/xvt_runtime/config/controller_config.c
    src/xvt_runtime/config/video_config.c
    src/xvt_runtime/config/mouse_config.c
    src/xvt_runtime/input/keyboard_mapping.c
    src/xvt_runtime/config/keyboard_config.c
    src/xvt_runtime/input/actions.c
    src/xvt_runtime/input/controller_options.c
    src/xvt_runtime/input/controller_mapping.c
    src/xvt_runtime/storage/storage.c
    src/xvt_runtime/storage/file_io.c
    src/xvt_runtime/compat/frontend_file_list_port.c
    src/xvt_runtime/compat/pilot_port.c
    src/xvt_runtime/runtime/port.c
    src/xvt_runtime/runtime/presentation.c
    src/xvt_runtime/snapshot/render_snapshot.c
    src/xvt_runtime/snapshot/render_capture.c
    src/xvt_runtime/snapshot/render_camera.c
    src/xvt_runtime/snapshot/cockpit_capture.c
    src/xvt_runtime/snapshot/cockpit_instruments.c
    src/xvt_runtime/snapshot/cockpit_readouts.c
    src/xvt_runtime/snapshot/cockpit_pages.c
    src/xvt_runtime/snapshot/cockpit_messages.c
    src/xvt_runtime/snapshot/cockpit_text.c
    src/xvt_runtime/snapshot/render_hud.c
    src/xvt_runtime/snapshot/render_map.c
    src/xvt_runtime/snapshot/render_frontend.c
    src/xvt_runtime/snapshot/render_assets.c
    src/xvt_runtime/snapshot/render_cockpit_assets.c
    src/xvt_runtime/snapshot/records.c
    src/xvt_runtime/snapshot/world_state.c
    src/xvt_runtime/runtime/movie_task.c
    src/xvt_runtime/runtime/cutscene_task.c
    src/xvt_runtime/runtime/campaign_task.c
    src/xvt_runtime/runtime/movie_sync.c
    src/xvt_runtime/runtime/frontend_task.c
    src/xvt_runtime/runtime/frontend_cleanup.c
    src/xvt_runtime/runtime/cd_task.c
    src/xvt_runtime/runtime/dialog_task.c
    src/xvt_runtime/runtime/mission_dialogs.c
    src/xvt_runtime/runtime/network_dialogs.c
    src/xvt_runtime/runtime/network_session.c
    src/xvt_runtime/runtime/network_metadata.c
    src/xvt_runtime/runtime/network_browser.c
    src/xvt_runtime/runtime/network_task.c
    src/xvt_runtime/runtime/frontend_actions.c
    src/xvt_runtime/runtime/launch_task.c
    src/xvt_runtime/runtime/flight_task.c
    src/xvt_runtime/runtime/flight_diag.c
    src/xvt_runtime/runtime/flight_checkpoint.c
    src/xvt_runtime/runtime/flight_wire.c
    src/xvt_runtime/runtime/flight_messages.c
    src/xvt_runtime/runtime/flight_network.c
    src/xvt_runtime/runtime/flight_packets.c
    src/xvt_runtime/runtime/resync_task.c
    src/xvt_runtime/runtime/flight_entry.c
    src/xvt_runtime/runtime/flight_loading.c
    src/xvt_runtime/runtime/flight_frame.c
    src/xvt_runtime/runtime/flight_sim.c
    src/xvt_runtime/runtime/flight_prediction.c
    src/xvt_runtime/runtime/frontend_movies.c
    src/xvt_runtime/input/input_bridge.c
    src/xvt_runtime/input/capture.c
    src/xvt_runtime/input/flight_controls.c
    src/xvt_runtime/input/mouse_flight.c
    src/xvt_runtime/hooks/orientation_hook.c
    src/xvt_runtime/hooks/orientation_fixed.c
    src/xvt_runtime/timing/host_clock.c
    src/xvt_runtime/timing/flight_timing.c
    src/xvt_runtime/timing/reference_motion.c
    src/xvt_runtime/timing/flight_integration.c
    src/xvt_runtime/timing/player_timing.c
)

add_library(xvt_core STATIC ${XVT_RECOVERED_SOURCES} ${XVT_RUNTIME_SOURCES})
target_include_directories(xvt_core PUBLIC src)
target_link_libraries(xvt_core PUBLIC aeron_compat aeron_video aeron_audio_decode aeron_scene)
if(NOT MSVC)
    target_link_libraries(xvt_core PUBLIC m)
endif()
target_compile_definitions(xvt_core PUBLIC XVT_MODERN)

add_library(xvt_remaster STATIC
    src/xvt_remaster/hud_layout.c
    src/xvt_remaster/hud_assets.c
    src/xvt_remaster/undither.c
    src/xvt_remaster/cockpit_loading.c
    src/xvt_remaster/hud_renderer.c
    src/xvt_remaster/hud_draw.c
    src/xvt_remaster/hud_text.c
    src/xvt_remaster/hud_panes.c
    src/xvt_remaster/hud_instruments.c
    src/xvt_remaster/crt.c
    src/xvt_remaster/ui_draw.c
    src/xvt_remaster/frontend.c
    src/xvt_remaster/flight_map.c
    src/xvt_remaster/effects.c
    src/xvt_remaster/sky.c
    src/xvt_remaster/sky_stars.c
    src/xvt_remaster/hyperspace.c
    src/xvt_remaster/flight.c
    src/xvt_remaster/flight_scene.c
    src/xvt_remaster/flight_pipeline.c
    src/xvt_remaster/lighting.c
    src/xvt_remaster/engine_glows.c
    src/xvt_remaster/ship.c
    src/xvt_remaster/component_animation.c
    src/xvt_remaster/preview.c
    src/xvt_remaster/render_math.c
    src/xvt_remaster/assets.c
    src/xvt_remaster/cockpit_assets.c
    src/xvt_remaster/opt_mesh.c
    src/xvt_remaster/original_2d.c
    src/xvt_remaster/ship_assets.c
    src/xvt_remaster/config.c
    src/xvt_remaster/xvt_remaster.c
    src/xvt_remaster/view_mode.c
)
target_include_directories(xvt_remaster PUBLIC src)
target_link_libraries(xvt_remaster PUBLIC xvt_core aeron_scene aeron_asset)

add_executable(xvt
    src/xvt_app/main.c
    src/xvt_app/application.c
    src/xvt_app/host_config.c
    src/xvt_app/setup.c
    src/xvt_app/setup_ui.c
    src/xvt_app/settings/video_options.c
    src/xvt_app/settings/video_page.c
    src/xvt_app/settings/mouse_page.c
    src/xvt_app/ui.c
    src/xvt_app/settings/settings.c
    src/xvt_app/settings/installation_page.c
    src/xvt_app/settings/keyboard_page.c
    src/xvt_app/settings/bindings_editor.c
    src/xvt_app/settings/controller_page.c
)
set_target_properties(xvt PROPERTIES
    OUTPUT_NAME "OpenXvT"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<0:>")
target_link_libraries(xvt PRIVATE xvt_remaster)
if(WIN32)
    set_target_properties(xvt PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

aeron_compile_hlsl(XVT_SHADER_OUTPUTS ${CMAKE_BINARY_DIR}/shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/frontend_copy.frag.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud_crt.vert.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hud_crt.frag.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sky_stars.vert.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sky_stars.frag.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hyperspace_streak.vert.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hyperspace_streak.frag.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hyperspace_tunnel.vert.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hyperspace_tunnel.frag.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hyperspace_environment.comp.hlsl)
add_custom_target(xvt_shaders DEPENDS ${XVT_SHADER_OUTPUTS})
add_dependencies(xvt_remaster xvt_shaders)
include(cmake/Packaging.cmake)

if(MSVC)
    target_compile_options(xvt_core PRIVATE /W4)
    target_compile_options(xvt_remaster PRIVATE /W4)
    target_compile_options(xvt PRIVATE /W4)
else()
    target_compile_options(xvt_core PRIVATE -Wall -Wextra -Wpedantic $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration> -fno-common)
    target_compile_options(xvt_remaster PRIVATE -Wall -Wextra -Wpedantic $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration> -fno-common)
    target_compile_options(xvt PRIVATE -Wall -Wextra -Wpedantic $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration> -fno-common)
    # Recovered MSVC 4.2-era code is not strict-aliasing clean; keep the
    # original shapes and opt these translation units out of TBAA.
    set_source_files_properties(${XVT_RECOVERED_SOURCES}
        PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing")
endif()

file(GLOB_RECURSE XVT_FORMAT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.h"
)

find_program(XVT_CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(XVT_CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
        COMMAND "${XVT_CLANG_FORMAT_EXECUTABLE}" -i --style=file ${XVT_FORMAT_SOURCES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Formatting OpenXvT C sources and headers"
        VERBATIM
    )
else()
    add_custom_target(format
        COMMAND "${CMAKE_COMMAND}" -E echo "clang-format was not found"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

add_dependencies(format aeron-format)
