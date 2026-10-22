#!/bin/bash

set -euo pipefail

if [[ -d /opt/homebrew/bin ]]; then
    export PATH="/opt/homebrew/bin:${PATH}"
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

sdl_version="${SDL_VERSION:-3.4.18}"
zstd_version="${ZSTD_VERSION:-1.5.7}"
ffmpeg_version="${FFMPEG_VERSION:-8.0.1}"
# curl 8.15 removed Secure Transport. 8.14.1 still has it, so HTTPS does not
# need a second TLS library on the simulator SDK.
curl_version="${CURL_VERSION:-8.14.1}"
curl_tag="${CURL_TAG:-curl-8_14_1}"
shadercross_commit="${SHADERCROSS_COMMIT:-6b06e55c7c5d7e7a09a8a14f76e866dcfad5ab99}"
deployment_target="${XVT_IOS_DEPLOYMENT_TARGET:-27.1}"
architecture="arm64"
host_architecture="$(uname -m)"
host_deployment_target="${XVT_MACOS_DEPLOYMENT_TARGET:-13.0}"
build_type="${XVT_BUILD_TYPE:-Release}"
bundle_identifier="${XVT_IOS_BUNDLE_IDENTIFIER:-org.openxvt.openxvt}"

case "${build_type}" in
    Release|Debug) ;;
    *) echo "XVT_BUILD_TYPE must be Release or Debug" >&2; exit 2 ;;
esac

deps_root="${XVT_IOS_DEPS_ROOT:-${repo_root}/build/ios-deps}"
sim_root="${XVT_IOS_SIM_BUILD:-${repo_root}/build/ios-sim}"
source_root="${deps_root}/sources"
ios_prefix="${deps_root}/iphonesimulator"
ios_build="${deps_root}/build"
stamp_dir="${ios_prefix}/stamps"
host_source="${deps_root}/host/sources"
host_build="${deps_root}/host/build"
host_prefix="${deps_root}/host/prefix"
shadercross_executable="${host_build}/shadercross/shadercross"
shadergen_executable="${host_build}/fsr3-shadergen/aeron_fsr3_shadergen"
log_dir="${deps_root}/logs"
jobs="$(sysctl -n hw.logicalcpu)"
sdk_path="$(xcrun --sdk iphonesimulator --show-sdk-path)"
target_triple="arm64-apple-ios${deployment_target}-simulator"

ios_cmake=(
    -G Ninja
    -DCMAKE_SYSTEM_NAME=iOS
    -DCMAKE_OSX_SYSROOT=iphonesimulator
    -DCMAKE_OSX_ARCHITECTURES="${architecture}"
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=${deployment_target}"
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_INSTALL_PREFIX=${ios_prefix}"
)

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

clone_tag() {
    local repository="$1"
    local tag="$2"
    local destination="$3"

    if [[ ! -d "${destination}/.git" ]]; then
        if [[ -e "${destination}" ]]; then
            echo "Source path exists but is not a Git checkout: ${destination}" >&2
            exit 1
        fi
        git clone --branch "${tag}" --depth 1 "${repository}" "${destination}"
    fi
}

stamp_current() {
    local stamp="$1"
    local expected="$2"
    [[ -f "${stamp}" && "$(<"${stamp}")" == "${expected}" ]]
}

write_stamp() {
    mkdir -p "${stamp_dir}"
    printf '%s\n' "$2" > "$1"
}

apply_repo_patch() {
    local repo="$1"
    local patch="$2"

    if git -C "${repo}" apply --reverse --check "${patch}" >/dev/null 2>&1; then
        return 0
    fi
    if ! git -C "${repo}" apply --check "${patch}" >/dev/null 2>&1; then
        echo "Patch does not apply: ${patch}" >&2
        exit 1
    fi
    git -C "${repo}" apply "${patch}"
}

load_shadercross_functions() {
    if declare -F build_shadercross >/dev/null; then
        return 0
    fi
    # shellcheck disable=SC1090
    eval "$(sed -n '/^prepare_shadercross()/,/^}/p; /^build_shadercross()/,/^}/p' \
        "${repo_root}/packaging/macos/build-package.sh")"
}

shadercross_is_host() {
    [[ -x "${shadercross_executable}" ]] || return 1
    vtool -show-build "${shadercross_executable}" | grep -q "platform MACOS"
}

build_host_shadercross() {
    if shadercross_is_host; then
        echo "Host shadercross already built: ${shadercross_executable}"
        return 0
    fi
    if ! command -v brew >/dev/null 2>&1; then
        echo "Host shadercross needs Homebrew SDL 3.4 on CMAKE_PREFIX_PATH" >&2
        exit 1
    fi

    local source_root="${host_source}"
    local build_root="${host_build}"
    local tool_prefix="${host_prefix}"
    local dependency_prefix
    dependency_prefix="$(brew --prefix)"
    local architecture="${host_architecture}"
    local deployment_target="${host_deployment_target}"
    local jobs="${jobs}"
    local repo_root="${repo_root}"
    local shadercross_commit="${shadercross_commit}"

    mkdir -p "${source_root}" "${build_root}" "${tool_prefix}"
    load_shadercross_functions
    build_shadercross
    if ! shadercross_is_host; then
        echo "shadercross is not a host macOS binary: ${shadercross_executable}" >&2
        exit 1
    fi
}

host_tool_is_macos() {
    local bin="$1"
    [[ -x "${bin}" ]] || return 1
    vtool -show-build "${bin}" | grep -q "platform MACOS"
}

build_host_shadergen() {
    if host_tool_is_macos "${shadergen_executable}"; then
        echo "Host FSR shader generator already built: ${shadergen_executable}"
        return 0
    fi
    if ! command -v brew >/dev/null 2>&1; then
        echo "Host FSR shader generator needs Homebrew SDL 3.4" >&2
        exit 1
    fi

    cmake -S "${repo_root}/aeron" \
        -B "${host_build}/fsr3-shadergen" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_PREFIX_PATH=$(brew --prefix)" \
        "-DCMAKE_OSX_ARCHITECTURES=${host_architecture}" \
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${host_deployment_target}" \
        -DAERON_HOST_TOOLS_ONLY=ON
    cmake --build "${host_build}/fsr3-shadergen" --target aeron_fsr3_shadergen \
        --parallel "${jobs}"
    if ! host_tool_is_macos "${shadergen_executable}"; then
        echo "aeron_fsr3_shadergen is not a host macOS binary: ${shadergen_executable}" >&2
        exit 1
    fi
}

build_zstd() {
    local expected="${zstd_version} static ${architecture} iphonesimulator ${deployment_target}"
    local stamp="${stamp_dir}/zstd"
    if stamp_current "${stamp}" "${expected}" && [[ -f "${ios_prefix}/lib/libzstd.a" ]]; then
        echo "zstd ${zstd_version} already installed"
        return 0
    fi

    clone_tag https://github.com/facebook/zstd.git \
        "v${zstd_version}" "${source_root}/zstd-${zstd_version}"
    cmake -S "${source_root}/zstd-${zstd_version}/build/cmake" \
        -B "${ios_build}/zstd" \
        "${ios_cmake[@]}" \
        -DZSTD_BUILD_PROGRAMS=OFF \
        -DZSTD_BUILD_SHARED=OFF \
        -DZSTD_BUILD_STATIC=ON \
        -DZSTD_BUILD_TESTS=OFF
    cmake --build "${ios_build}/zstd" --parallel "${jobs}"
    cmake --install "${ios_build}/zstd"
    if [[ ! -f "${ios_prefix}/lib/libzstd.a" ]]; then
        echo "zstd install did not produce libzstd.a" >&2
        exit 1
    fi
    write_stamp "${stamp}" "${expected}"
}

build_sdl() {
    local expected="${sdl_version} static ${architecture} iphonesimulator ${deployment_target} duo-display-scale"
    local stamp="${stamp_dir}/sdl"
    if stamp_current "${stamp}" "${expected}" \
            && [[ -f "${ios_prefix}/lib/libSDL3.a" ]] \
            && [[ -f "${ios_prefix}/lib/cmake/SDL3/SDL3staticTargets.cmake" ]]; then
        echo "SDL ${sdl_version} already installed"
        return 0
    fi

    clone_tag https://github.com/libsdl-org/SDL.git \
        "release-${sdl_version}" "${source_root}/SDL-${sdl_version}"
    apply_repo_patch "${source_root}/SDL-${sdl_version}" \
        "${repo_root}/packaging/ios/patches/sdl-duo-display-scale.patch"
    cmake -S "${source_root}/SDL-${sdl_version}" \
        -B "${ios_build}/sdl" \
        "${ios_cmake[@]}" \
        -DSDL_AUDIO=ON \
        -DSDL_EXAMPLES=OFF \
        -DSDL_FRAMEWORK=OFF \
        -DSDL_GPU=ON \
        -DSDL_HAPTIC=ON \
        -DSDL_HIDAPI=ON \
        -DSDL_HIDAPI_LIBUSB=OFF \
        -DSDL_INSTALL_DOCS=OFF \
        -DSDL_JOYSTICK=ON \
        -DSDL_METAL=ON \
        -DSDL_SHARED=OFF \
        -DSDL_STATIC=ON \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_VIDEO=ON \
        -DSDL_VULKAN=OFF
    cmake --build "${ios_build}/sdl" --parallel "${jobs}"
    cmake --install "${ios_build}/sdl"
    if [[ ! -f "${ios_prefix}/lib/libSDL3.a" ]]; then
        echo "SDL install did not produce libSDL3.a" >&2
        exit 1
    fi
    write_stamp "${stamp}" "${expected}"
}

build_curl() {
    local expected="${curl_version} static sectransp ${architecture} iphonesimulator ${deployment_target}"
    local stamp="${stamp_dir}/curl"
    if stamp_current "${stamp}" "${expected}" \
            && [[ -f "${ios_prefix}/lib/libcurl.a" ]] \
            && [[ -f "${ios_prefix}/lib/cmake/CURL/CURLConfig.cmake" ]]; then
        echo "curl ${curl_version} already installed"
        return 0
    fi

    clone_tag https://github.com/curl/curl.git \
        "${curl_tag}" "${source_root}/curl-${curl_version}"
    cmake -S "${source_root}/curl-${curl_version}" \
        -B "${ios_build}/curl" \
        "${ios_cmake[@]}" \
        -DBUILD_CURL_EXE=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_STATIC_LIBS=ON \
        -DBUILD_TESTING=OFF \
        -DCURL_DISABLE_LDAP=ON \
        -DCURL_ENABLE_SSL=ON \
        -DCURL_USE_LIBPSL=OFF \
        -DCURL_USE_LIBSSH2=OFF \
        -DCURL_USE_OPENSSL=OFF \
        -DCURL_USE_SECTRANSP=ON \
        -DENABLE_CURL_MANUAL=OFF \
        -DENABLE_THREADED_RESOLVER=ON \
        -DUSE_LIBIDN2=OFF \
        -DUSE_NGHTTP2=OFF
    cmake --build "${ios_build}/curl" --parallel "${jobs}"
    cmake --install "${ios_build}/curl"
    if [[ ! -f "${ios_prefix}/lib/libcurl.a" ]]; then
        echo "curl install did not produce libcurl.a" >&2
        exit 1
    fi
    write_stamp "${stamp}" "${expected}"
}

build_ffmpeg() {
    local expected="${ffmpeg_version} static ${architecture} iphonesimulator ${deployment_target}"
    local stamp="${stamp_dir}/ffmpeg"
    local pc="${ios_prefix}/lib/pkgconfig"
    if stamp_current "${stamp}" "${expected}" \
            && [[ -f "${ios_prefix}/lib/libavcodec.a" ]] \
            && [[ -f "${ios_prefix}/lib/libswscale.a" ]] \
            && [[ -f "${ios_prefix}/lib/libswresample.a" ]] \
            && [[ -f "${pc}/libavformat.pc" ]]; then
        echo "FFmpeg ${ffmpeg_version} already installed"
        return 0
    fi

    local cc cxx ar ranlib nm_tool strip_tool cflags feature
    clone_tag https://github.com/FFmpeg/FFmpeg.git \
        "n${ffmpeg_version}" "${source_root}/FFmpeg-${ffmpeg_version}"
    cc="$(xcrun --sdk iphonesimulator -f clang)"
    cxx="$(xcrun --sdk iphonesimulator -f clang++)"
    ar="$(xcrun --sdk iphonesimulator -f ar)"
    ranlib="$(xcrun --sdk iphonesimulator -f ranlib)"
    nm_tool="$(xcrun --sdk iphonesimulator -f nm)"
    strip_tool="$(xcrun --sdk iphonesimulator -f strip)"
    cflags="-arch ${architecture} -target ${target_triple} -isysroot ${sdk_path}"

    mkdir -p "${ios_build}/ffmpeg"
    (
        cd "${ios_build}/ffmpeg"
        export PKG_CONFIG_PATH=""
        export PKG_CONFIG_LIBDIR="/var/empty"
        "${source_root}/FFmpeg-${ffmpeg_version}/configure" \
            "--prefix=${ios_prefix}" \
            --target-os=darwin \
            "--arch=${architecture}" \
            --enable-cross-compile \
            "--sysroot=${sdk_path}" \
            "--cc=${cc}" \
            "--cxx=${cxx}" \
            "--ar=${ar}" \
            "--ranlib=${ranlib}" \
            "--nm=${nm_tool}" \
            "--strip=${strip_tool}" \
            --disable-autodetect \
            --disable-avdevice \
            --disable-avfilter \
            --disable-debug \
            --disable-doc \
            --disable-everything \
            --disable-network \
            --disable-programs \
            --disable-shared \
            --enable-static \
            --enable-decoder=smacker,smackaud,pcm_u8,pcm_s16le,vorbis,binkaudio_rdft,binkaudio_dct \
            --enable-parser=vorbis \
            --enable-demuxer=smacker,ogg,wav \
            --enable-pic \
            --enable-pthreads \
            "--extra-cflags=${cflags}" \
            "--extra-ldflags=${cflags}"
        for feature in SMACKER_DECODER SMACKAUD_DECODER VORBIS_DECODER \
            PCM_U8_DECODER PCM_S16LE_DECODER BINKAUDIO_RDFT_DECODER \
            BINKAUDIO_DCT_DECODER SMACKER_DEMUXER OGG_DEMUXER WAV_DEMUXER \
            SWSCALE SWRESAMPLE; do
            grep -q "^#define CONFIG_${feature} 1$" config.h config_components.h || {
                echo "FFmpeg is missing ${feature}" >&2
                exit 1
            }
        done
        make -j"${jobs}"
        make install
    )
    write_stamp "${stamp}" "${expected}"
}

build_application() {
    # ONLY rewrites CMAKE_PREFIX_PATH under the SDK root, so the iOS prefix
    # would be invisible. BOTH searches that prefix as given. Homebrew is
    # ignored so a host Vulkan header cannot define the iOS renderer.
    export PKG_CONFIG_PATH=""
    export PKG_CONFIG_LIBDIR="${ios_prefix}/lib/pkgconfig"
    # FindPkgConfig stores -framework as separate cache tokens. The next
    # configure then emits one -framework and bare framework names.
    rm -f "${sim_root}/CMakeCache.txt"
    apply_repo_patch "${repo_root}/aeron" \
        "${repo_root}/packaging/ios/patches/aeron-ios.patch"

    cmake -S "${repo_root}" \
        -B "${sim_root}" \
        -G Ninja \
        "-DCMAKE_BUILD_TYPE=${build_type}" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT=iphonesimulator \
        -DCMAKE_OSX_ARCHITECTURES="${architecture}" \
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${deployment_target}" \
        "-DCMAKE_PREFIX_PATH=${ios_prefix}" \
        -DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew \
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        "-DPKG_CONFIG_ARGN=--static" \
        "-DAERON_SHADERCROSS_EXECUTABLE=${shadercross_executable}" \
        "-DAERON_FSR3_SHADERGEN_EXECUTABLE=${shadergen_executable}" \
        -DXVT_BUILD_TOOLS=OFF \
        -DXVT_ENABLE_ASAN=OFF \
        -DXVT_ENABLE_UBSAN=OFF \
        -DXVT_ENABLE_DEBUG_UI=OFF \
        "-DXVT_IOS_BUNDLE_IDENTIFIER=${bundle_identifier}"
    cmake --build "${sim_root}" --target xvt --parallel "${jobs}"
}

verify_application() {
    local application="${sim_root}/OpenXvT.app"
    local binary="${application}/OpenXvT"
    local plist="${application}/Info.plist"
    local build_info sym

    test -x "${binary}"
    test -f "${plist}"
    test ! -d "${application}/Contents"
    test ! -d "${application}/Resources"
    test ! -d "${application}/resources"
    test -f "${application}/gamedata/config.yaml"
    test -f "${application}/gamedata/opt_alpha_overrides.yaml"
    test -f "${application}/gamedata/aeron/scene3d_defaults.yaml"
    test -f "${application}/gamedata/assets/fonts/menu.fnt"
    test -f "${application}/gamedata/assets/fonts/menu.png"
    test -f "${application}/shaders/aeron_dx5_projected_triangle.vert.msl"
    test -f "${application}/shaders/hyperspace_streak.vert.msl"
    test -f "${application}/shaders/hyperspace_streak.frag.msl"
    test -f "${application}/shaders/hyperspace_tunnel.vert.msl"
    test -f "${application}/shaders/hyperspace_tunnel.frag.msl"
    plutil -extract CFBundleSupportedPlatforms json -o - "${plist}" | grep -q iPhoneSimulator

    plutil -lint "${plist}" >/dev/null
    plutil -extract UILaunchScreen xml1 -o /dev/null "${plist}"
    plutil -extract MinimumOSVersion raw -o - "${plist}" | grep -q "^${deployment_target}$"
    plutil -extract CFBundleIdentifier raw -o - "${plist}" | grep -q "^${bundle_identifier}$"
    for sym in UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight; do
        plutil -extract UISupportedInterfaceOrientations json -o - "${plist}" | grep -q "${sym}"
        plutil -extract 'UISupportedInterfaceOrientations~ipad' json -o - "${plist}" | grep -q "${sym}"
    done
    if plutil -p "${plist}" | grep -E -q 'UIScreen|UIDevice|statusBarOrientation|UIRequiresFullScreen|LSMinimumSystemVersion'; then
        echo "Info.plist contains a macOS or forbidden iOS key" >&2
        plutil -p "${plist}" >&2
        exit 1
    fi

    file "${binary}" | grep -q "arm64"
    build_info="$(vtool -show-build "${binary}")"
    grep -q "platform IOSSIMULATOR" <<<"${build_info}"
    grep -q "minos ${deployment_target}" <<<"${build_info}"
    grep -q "sdk ${deployment_target}" <<<"${build_info}"
    if otool -L "${binary}" | grep -E -q "${ios_prefix}|/opt/homebrew|/usr/local"; then
        echo "OpenXvT references a non-system library" >&2
        otool -L "${binary}" >&2
        exit 1
    fi
    # grep -q in a pipe closes nm early, and pipefail then reports a miss.
    symbols="$(nm -gU "${binary}")"
    for sym in AeronDplay_Update AeronDplay_IsActive AeronDplay_Shutdown \
            AeronDplay_NextWakeDelayUs AeronWinmm_ConfigureCdAudio Aeron_VideoOpen; do
        if ! grep -F -q " _${sym}" <<<"${symbols}"; then
            echo "Linked binary is missing ${sym}" >&2
            exit 1
        fi
    done

    echo "Application bundle: ${application}"
    file "${binary}"
    vtool -show-build "${binary}"
}

main() {
    local command
    for command in clang cmake file git make ninja nm otool pkg-config plutil \
            sysctl vtool xcrun; do
        require_command "${command}"
    done
    mkdir -p "${source_root}" "${ios_prefix}" "${ios_build}" "${log_dir}" \
        "${host_source}" "${host_build}" "${host_prefix}"

    build_host_shadercross
    build_host_shadergen
    build_zstd
    build_sdl
    build_curl
    build_ffmpeg
    build_application
    verify_application
}

main "$@"
