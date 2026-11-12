# Microsoft's Linux DXC release binaries are available for x86-64 only.
ARG XVT_BUILD_PLATFORM=linux/amd64

FROM --platform=${XVT_BUILD_PLATFORM} debian:bookworm-slim AS build-base

RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils-mingw-w64-x86-64 \
        build-essential \
        ca-certificates \
        cmake \
        g++-mingw-w64-x86-64-posix \
        gcc-mingw-w64-x86-64-posix \
        git \
        libncurses-dev \
        libxml2-dev \
        ninja-build \
        nasm \
        pkg-config \
        python3 \
        unzip \
        zip \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

ARG SDL_VERSION=3.4.18
ARG ZSTD_VERSION=1.5.7
ARG FFMPEG_VERSION=8.0.1
ARG SHADERCROSS_COMMIT=6b06e55c7c5d7e7a09a8a14f76e866dcfad5ab99

FROM build-base AS host-tools

RUN git clone --branch "release-${SDL_VERSION}" --depth 1 \
        https://github.com/libsdl-org/SDL.git /deps/SDL \
    && cmake -S /deps/SDL -B /build/sdl-host -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-host \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_UNIX_CONSOLE_BUILD=ON \
    && cmake --build /build/sdl-host --parallel \
    && cmake --install /build/sdl-host

COPY packaging/common/shadercross-fsr3.patch /tmp/shadercross-fsr3.patch

# DXC 1.9 requires glibc 2.38; keep the Debian 12-compatible 1.8 release.
ARG DXC_VERSION=1.8.2502
ARG DXC_LINUX_X64_ARCHIVE=linux_dxc_2025_02_20.x86_64.tar.gz
ARG DXC_LINUX_X64_SHA256=e0580d90dbf6053a783ddd8d5153285f0606e5deaad17a7a6452f03acdf88c71

ADD --checksum=sha256:${DXC_LINUX_X64_SHA256} \
    https://github.com/microsoft/DirectXShaderCompiler/releases/download/v${DXC_VERSION}/${DXC_LINUX_X64_ARCHIVE} \
    /tmp/dxc.tar.gz

RUN git clone https://github.com/libsdl-org/SDL_shadercross.git /deps/SDL_shadercross \
    && git -C /deps/SDL_shadercross checkout "${SHADERCROSS_COMMIT}" \
    && git -C /deps/SDL_shadercross apply /tmp/shadercross-fsr3.patch \
    && git -C /deps/SDL_shadercross submodule update --init --depth 1 \
        external/SPIRV-Cross \
    && mkdir -p \
        /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux \
    && tar -xzf /tmp/dxc.tar.gz \
        -C /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux \
    && cmake -S /deps/SDL_shadercross/external/SPIRV-Cross \
        -B /build/spirv-cross -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-host \
        -DSPIRV_CROSS_CLI=OFF \
        -DSPIRV_CROSS_ENABLE_TESTS=OFF \
        -DSPIRV_CROSS_SHARED=ON \
        -DSPIRV_CROSS_STATIC=OFF \
    && cmake --build /build/spirv-cross --parallel \
    && cmake --install /build/spirv-cross \
    && cmake -S /deps/SDL_shadercross -B /build/shadercross -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-host \
        -DCMAKE_PREFIX_PATH="/opt/xvt-host" \
        -DDirectXShaderCompiler_ROOT=/deps/SDL_shadercross/external/DirectXShaderCompiler-binaries \
        -DSDLSHADERCROSS_CLI=ON \
        -DSDLSHADERCROSS_DXC=ON \
        -DSDLSHADERCROSS_INSTALL=ON \
        -DSDLSHADERCROSS_INSTALL_RUNTIME=OFF \
        -DSDLSHADERCROSS_SHARED=ON \
        -DSDLSHADERCROSS_STATIC=OFF \
        -DSDLSHADERCROSS_SPIRVCROSS_SHARED=ON \
        -DSDLSHADERCROSS_VENDORED=OFF \
    && cmake --build /build/shadercross --parallel \
    && cmake --install /build/shadercross \
    && cp /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux/lib/libdxcompiler.so \
        /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux/lib/libdxil.so \
        /opt/xvt-host/lib/

# The Windows target dependencies build as independent stages so that a
# version bump only rebuilds the affected dependency and so that cold builds
# run concurrently.

FROM build-base AS sdl-windows

COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xvt-toolchain.cmake
COPY packaging/common/sdl-gpu-d3d12-hdr.patch /tmp/sdl-gpu-d3d12-hdr.patch

# OpenXWA's fix allows HDR composition queries before replacing the SDR
# swapchain and restores the previous swapchain if replacement fails.
RUN git clone --branch "release-${SDL_VERSION}" --depth 1 \
        https://github.com/libsdl-org/SDL.git /deps/SDL \
    && git -C /deps/SDL apply /tmp/sdl-gpu-d3d12-hdr.patch \
    && cmake -S /deps/SDL -B /build/sdl-windows -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-windows \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xvt-toolchain.cmake \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF \
    && cmake --build /build/sdl-windows --parallel \
    && cmake --install /build/sdl-windows

FROM build-base AS zstd-windows

COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xvt-toolchain.cmake

RUN git clone --branch "v${ZSTD_VERSION}" --depth 1 \
        https://github.com/facebook/zstd.git /deps/zstd \
    && cmake -S /deps/zstd/build/cmake -B /build/zstd-windows -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-windows \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xvt-toolchain.cmake \
        -DZSTD_BUILD_PROGRAMS=OFF \
        -DZSTD_BUILD_SHARED=ON \
        -DZSTD_BUILD_STATIC=OFF \
        -DZSTD_BUILD_TESTS=OFF \
    && cmake --build /build/zstd-windows --parallel \
    && cmake --install /build/zstd-windows

FROM build-base AS ffmpeg-windows

RUN git clone --branch "n${FFMPEG_VERSION}" --depth 1 \
        https://github.com/FFmpeg/FFmpeg.git /deps/FFmpeg \
    && cd /deps/FFmpeg \
    && ./configure \
        --prefix=/opt/xvt-windows \
        --target-os=mingw32 \
        --arch=x86_64 \
        --cross-prefix=x86_64-w64-mingw32- \
        --disable-autodetect \
        --disable-avdevice \
        --disable-avfilter \
        --disable-debug \
        --disable-doc \
        --disable-everything \
        --disable-network \
        --disable-programs \
        --disable-static \
        --enable-shared \
        --enable-decoder=smacker,smackaud,pcm_u8,pcm_s16le,vorbis,binkaudio_rdft,binkaudio_dct \
        --enable-parser=vorbis \
        --enable-demuxer=smacker,ogg,wav \
        --enable-w32threads \
    && for feature in SMACKER_DECODER SMACKAUD_DECODER VORBIS_DECODER \
        PCM_U8_DECODER PCM_S16LE_DECODER SMACKER_DEMUXER OGG_DEMUXER WAV_DEMUXER; do \
        grep -q "^#define CONFIG_${feature} 1$" config_components.h || exit 1; \
    done \
    && make -j"$(nproc)" \
    && make install

FROM build-base AS curl-windows

ARG CURL_VERSION=8_22_0
COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xvt-toolchain.cmake

# Schannel uses the Windows certificate store; DNS runs on curl's worker threads.
RUN git clone --branch "curl-${CURL_VERSION}" --depth 1 https://github.com/curl/curl.git /deps/curl \
    && cmake -S /deps/curl -B /build/curl -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-windows \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xvt-toolchain.cmake \
        -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF \
        -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF \
        -DBUILD_LIBCURL_DOCS=OFF -DBUILD_MISC_DOCS=OFF \
        -DCURL_USE_SCHANNEL=ON -DCURL_USE_OPENSSL=OFF \
        -DENABLE_THREADED_RESOLVER=ON -DHTTP_ONLY=ON \
        -DCURL_USE_LIBPSL=OFF -DCURL_ZLIB=OFF -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF \
    && cmake --build /build/curl --parallel \
    && cmake --install /build/curl

FROM build-base AS winpix-windows

ARG WINPIX_VERSION=1.0.240308001
ARG WINPIX_NUPKG_SHA256=726acc93d6968e2146261a1e415521747d50ad69894c2b42b5d0d4c29fd66ec4

# SDL's D3D12 backend emits SDL_Push/PopGPUDebugGroup and
# SDL_InsertGPUDebugLabel through WinPixEventRuntime.dll, loaded at runtime.
# MIT-licensed since 1.0.240308001. Shipped with Debug packages only: Release
# builds compile the label calls out (AERON_GPU_DEBUG_LABELS), so the DLL is
# staged outside /opt/xvt-windows to keep it clear of the wildcard DLL copy.
ADD --checksum=sha256:${WINPIX_NUPKG_SHA256} \
    https://globalcdn.nuget.org/packages/winpixeventruntime.${WINPIX_VERSION}.nupkg \
    /tmp/winpix.nupkg

RUN mkdir -p /deps/winpix /opt/winpix/bin \
    && unzip -q /tmp/winpix.nupkg -d /deps/winpix \
    && install -m 644 /deps/winpix/bin/x64/WinPixEventRuntime.dll \
        /opt/winpix/bin/

FROM build-base AS xvt-host-tools

COPY --from=host-tools /opt/xvt-host /opt/xvt-host
COPY aeron /src/aeron

ENV LD_LIBRARY_PATH=/opt/xvt-host/lib

RUN cmake -S /src/aeron -B /build/aeron-host -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-host \
        -DCMAKE_PREFIX_PATH=/opt/xvt-host \
        -DAERON_HOST_TOOLS_ONLY=ON \
    && cmake --build /build/aeron-host --parallel \
    && cmake --install /build/aeron-host

FROM build-base AS xvt-build

ARG XVT_BUILD_TYPE=Release
ARG XVT_VERSION=0.0.0-dev

COPY --from=xvt-host-tools /opt/xvt-host /opt/xvt-host
COPY --from=sdl-windows /opt/xvt-windows /opt/xvt-windows
COPY --from=zstd-windows /opt/xvt-windows /opt/xvt-windows
COPY --from=ffmpeg-windows /opt/xvt-windows /opt/xvt-windows
COPY --from=curl-windows /opt/xvt-windows /opt/xvt-windows
COPY --from=sdl-windows /deps/SDL/LICENSE.txt /opt/xvt-licenses/SDL3.txt
COPY --from=zstd-windows /deps/zstd/LICENSE /opt/xvt-licenses/zstd.txt
COPY --from=ffmpeg-windows /deps/FFmpeg/COPYING.LGPLv2.1 /opt/xvt-licenses/FFmpeg-LGPL-2.1.txt
COPY --from=curl-windows /deps/curl/COPYING /opt/xvt-licenses/curl.txt
COPY --from=winpix-windows /opt/winpix /opt/winpix
COPY --from=winpix-windows /deps/winpix/license.txt /opt/xvt-licenses/WinPixEventRuntime.txt
COPY --from=winpix-windows /deps/winpix/ThirdPartyNotices.txt /opt/xvt-licenses/WinPixEventRuntime-ThirdPartyNotices.txt
COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xvt-toolchain.cmake
COPY CMakeLists.txt /src/CMakeLists.txt
COPY README.md /src/README.md
COPY aeron /src/aeron
COPY cmake /src/cmake
COPY licenses /src/licenses
COPY packaging /src/packaging
COPY resources /src/resources
COPY shaders /src/shaders
COPY src /src/src

ENV LD_LIBRARY_PATH=/opt/xvt-host/lib

RUN case "${XVT_BUILD_TYPE}" in \
        Release) xvt_debug_ui=OFF; xvt_release_flags="-O3 -DNDEBUG -g" ;; \
        Debug) xvt_debug_ui=ON; xvt_release_flags="-O3 -DNDEBUG" ;; \
        *) echo "XVT_BUILD_TYPE must be Release or Debug" >&2; exit 2 ;; \
    esac \
    && cmake -S /src -B /build/xvt -G Ninja \
        -DCMAKE_BUILD_TYPE="${XVT_BUILD_TYPE}" \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xvt-toolchain.cmake \
        -DCMAKE_C_FLAGS_RELEASE="${xvt_release_flags}" \
        -DCMAKE_CXX_FLAGS_RELEASE="${xvt_release_flags}" \
        -DAERON_FSR3_SHADERGEN_EXECUTABLE=/opt/xvt-host/bin/aeron_fsr3_shadergen \
        -DAERON_SHADERCROSS_EXECUTABLE=/opt/xvt-host/bin/shadercross \
        -DXVT_BUILD_TOOLS=OFF \
        -DXVT_ENABLE_ASAN=OFF \
        -DXVT_ENABLE_UBSAN=OFF \
        -DXVT_ENABLE_DEBUG_UI="${xvt_debug_ui}" \
        -DXVT_VERSION="${XVT_VERSION}" \
    && cmake --build /build/xvt --target xvt --parallel

# Release builds compile with -g so crash dumps can be symbolized; the DWARF
# is split into OpenXvT.exe.debug, published as a separate symbols archive,
# and the shipped exe keeps only a .gnu_debuglink to it. Stripping removes
# trailing sections only, so code addresses match the symbols exactly.
RUN cmake --install /build/xvt --component Runtime --prefix /build/package/openxvt-windows-x86_64 \
    && if [ "${XVT_BUILD_TYPE}" = Release ]; then \
        mkdir -p /build/symbols \
        && cd /build/package/openxvt-windows-x86_64 \
        && x86_64-w64-mingw32-objcopy --only-keep-debug OpenXvT.exe /build/symbols/OpenXvT.exe.debug \
        && cp /build/symbols/OpenXvT.exe.debug . \
        && x86_64-w64-mingw32-objcopy --strip-debug --add-gnu-debuglink=OpenXvT.exe.debug OpenXvT.exe \
        && rm OpenXvT.exe.debug \
        && cp OpenXvT.exe /build/symbols/OpenXvT.exe; \
    fi \
    && mkdir -p /build/package/openxvt-windows-x86_64/licenses \
    && cp /opt/xvt-licenses/SDL3.txt \
        /build/package/openxvt-windows-x86_64/licenses/SDL3.txt \
    && cp /opt/xvt-licenses/zstd.txt \
        /build/package/openxvt-windows-x86_64/licenses/zstd.txt \
    && cp /opt/xvt-licenses/FFmpeg-LGPL-2.1.txt \
        /build/package/openxvt-windows-x86_64/licenses/FFmpeg-LGPL-2.1.txt \
    && cp /opt/xvt-licenses/curl.txt \
        /build/package/openxvt-windows-x86_64/licenses/curl.txt \
    && cp -L /usr/share/doc/gcc-mingw-w64-x86-64-posix-runtime/copyright \
        /build/package/openxvt-windows-x86_64/licenses/GCC-MinGW-w64.txt \
    && cp -L /usr/share/doc/mingw-w64-x86-64-dev/copyright \
        /build/package/openxvt-windows-x86_64/licenses/MinGW-w64.txt \
    && cp /opt/xvt-windows/bin/*.dll /build/package/openxvt-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32-g++ -print-file-name=libgcc_s_seh-1.dll)" \
        /build/package/openxvt-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32-g++ -print-file-name=libstdc++-6.dll)" \
        /build/package/openxvt-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32-g++ -print-file-name=libwinpthread-1.dll)" \
        /build/package/openxvt-windows-x86_64/ \
    && if [ "${XVT_BUILD_TYPE}" = Debug ]; then \
        cp /opt/winpix/bin/WinPixEventRuntime.dll \
            /build/package/openxvt-windows-x86_64/ \
        && cp /opt/xvt-licenses/WinPixEventRuntime.txt \
            /build/package/openxvt-windows-x86_64/licenses/WinPixEventRuntime.txt \
        && cp /opt/xvt-licenses/WinPixEventRuntime-ThirdPartyNotices.txt \
            /build/package/openxvt-windows-x86_64/licenses/WinPixEventRuntime-ThirdPartyNotices.txt; \
    fi \
    # Bundled DLLs, notably Debian's libstdc++, carry DWARF; split it the same way.
    && if [ "${XVT_BUILD_TYPE}" = Release ]; then \
        cd /build/package/openxvt-windows-x86_64 \
        && for library in *.dll; do \
            if x86_64-w64-mingw32-objdump -h "$library" | grep -q '\.debug_'; then \
                x86_64-w64-mingw32-objcopy --only-keep-debug "$library" "/build/symbols/${library}.debug" \
                && cp "/build/symbols/${library}.debug" . \
                && x86_64-w64-mingw32-objcopy --strip-debug --add-gnu-debuglink="${library}.debug" "$library" \
                && rm "${library}.debug" \
                && cp "$library" /build/symbols/ \
                || exit 1; \
            fi; \
        done; \
    fi

RUN test -f /build/package/openxvt-windows-x86_64/shaders/aeron_dx5_projected_triangle.vert.dxil \
    && test -f /build/package/openxvt-windows-x86_64/shaders/aeron_dx5_projected_triangle.vert.spv \
    && test -f /build/package/openxvt-windows-x86_64/shaders/hyperspace_streak.vert.dxil \
    && test -f /build/package/openxvt-windows-x86_64/shaders/hyperspace_streak.frag.dxil \
    && test -f /build/package/openxvt-windows-x86_64/shaders/hyperspace_streak.vert.spv \
    && test -f /build/package/openxvt-windows-x86_64/shaders/hyperspace_streak.frag.spv \
    && test -f /build/package/openxvt-windows-x86_64/shaders/hyperspace_tunnel.vert.dxil \
    && test -f /build/package/openxvt-windows-x86_64/shaders/hyperspace_tunnel.frag.dxil \
    && test -f /build/package/openxvt-windows-x86_64/shaders/hyperspace_tunnel.vert.spv \
    && test -f /build/package/openxvt-windows-x86_64/shaders/hyperspace_tunnel.frag.spv \
    && test -f /build/package/openxvt-windows-x86_64/resources/aeron/scene3d_defaults.yaml \
    && test -f /build/package/openxvt-windows-x86_64/resources/config.yaml \
    && x86_64-w64-mingw32-objdump -p \
        /build/package/openxvt-windows-x86_64/OpenXvT.exe \
        | grep -Eq 'Subsystem[[:space:]]+00000002' \
    && x86_64-w64-mingw32-objdump -p \
        /build/package/openxvt-windows-x86_64/OpenXvT.exe \
        | grep -Eqi 'DLL Name:.*curl' \
    && find /build/package/openxvt-windows-x86_64 -maxdepth 1 \
        -type f -iname '*curl*.dll' -print -quit | grep -q . \
    && if [ "${XVT_BUILD_TYPE}" = Debug ]; then \
        x86_64-w64-mingw32-objdump -h \
            /build/package/openxvt-windows-x86_64/OpenXvT.exe \
            | grep -q '\.debug_info' \
        && test -f /build/package/openxvt-windows-x86_64/WinPixEventRuntime.dll; \
    else \
        ! x86_64-w64-mingw32-objdump -h \
            /build/package/openxvt-windows-x86_64/OpenXvT.exe \
            | grep -q '\.debug_info' \
        && x86_64-w64-mingw32-objdump -h \
            /build/package/openxvt-windows-x86_64/OpenXvT.exe \
            | grep -q '\.gnu_debuglink' \
        && x86_64-w64-mingw32-objdump -h /build/symbols/OpenXvT.exe.debug \
            | grep -q '\.debug_info' \
        && for library in /build/package/openxvt-windows-x86_64/*.dll; do \
            ! x86_64-w64-mingw32-objdump -h "$library" | grep -q '\.debug_' || exit 1; \
        done; \
    fi \
    && if [ "${XVT_BUILD_TYPE}" = Debug ]; then \
        artifact_variant=debug; \
    else \
        artifact_variant=release; \
    fi \
    && package_name="openxvt-${XVT_VERSION}-windows-x86_64-${artifact_variant}" \
    && mv /build/package/openxvt-windows-x86_64 "/build/package/${package_name}" \
    && find "/build/package/${package_name}" \
        -exec touch -h -d "@315532800" {} + \
    && mkdir -p /build/artifacts \
    && cd /build/package \
    && find "${package_name}" -print > /tmp/openxvt-windows-files \
    && LC_ALL=C sort -o /tmp/openxvt-windows-files /tmp/openxvt-windows-files \
    && TZ=UTC zip -X -9 -q "/build/artifacts/${package_name}.zip" \
        -@ < /tmp/openxvt-windows-files \
    && unzip -tq "/build/artifacts/${package_name}.zip" \
    && if [ "${XVT_BUILD_TYPE}" = Release ]; then \
        symbols_name="${package_name}-symbols" \
        && mv /build/symbols "/build/${symbols_name}" \
        && find "/build/${symbols_name}" -exec touch -h -d "@315532800" {} + \
        && cd /build \
        && find "${symbols_name}" -print | LC_ALL=C sort \
            | TZ=UTC zip -X -9 -q "/build/artifacts/${symbols_name}.zip" -@ \
        && unzip -tq "/build/artifacts/${symbols_name}.zip"; \
    fi

FROM build-base AS netprobe-build

COPY --from=xvt-host-tools /opt/xvt-host /opt/xvt-host
COPY --from=sdl-windows /opt/xvt-windows /opt/xvt-windows
COPY --from=zstd-windows /opt/xvt-windows /opt/xvt-windows
COPY --from=curl-windows /opt/xvt-windows /opt/xvt-windows
COPY --from=sdl-windows /deps/SDL/LICENSE.txt /opt/xvt-licenses/SDL3.txt
COPY --from=zstd-windows /deps/zstd/LICENSE /opt/xvt-licenses/zstd.txt
COPY aeron/third_party/libjuice/LICENSE /opt/xvt-licenses/libjuice.txt
COPY --from=curl-windows /deps/curl/COPYING /opt/xvt-licenses/curl.txt
COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xvt-toolchain.cmake
COPY aeron /src/aeron

ENV LD_LIBRARY_PATH=/opt/xvt-host/lib

RUN cmake -S /src/aeron -B /build/netprobe -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xvt-toolchain.cmake \
        -DAERON_FSR3_SHADERGEN_EXECUTABLE=/opt/xvt-host/bin/aeron_fsr3_shadergen \
        -DAERON_SHADERCROSS_EXECUTABLE=/opt/xvt-host/bin/shadercross \
        -DAERON_ENABLE_COMPAT=ON -DAERON_ENABLE_DPLAY=ON \
        -DAERON_ENABLE_VIDEO=OFF -DAERON_ENABLE_AUDIO_DECODE=OFF \
        -DAERON_BUILD_TOOLS=OFF \
    && cmake --build /build/netprobe --target aeron-netprobe --parallel

RUN mkdir -p /build/package/aeron-netprobe-windows-x86_64/licenses /build/artifacts \
    && cp /build/netprobe/aeron-netprobe.exe /opt/xvt-windows/bin/*.dll \
        /build/package/aeron-netprobe-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32-g++ -print-file-name=libgcc_s_seh-1.dll)" \
        "$(x86_64-w64-mingw32-g++ -print-file-name=libstdc++-6.dll)" \
        "$(x86_64-w64-mingw32-g++ -print-file-name=libwinpthread-1.dll)" \
        /build/package/aeron-netprobe-windows-x86_64/ \
    && cp /src/aeron/tools/netprobe/README.md /src/aeron/tools/netprobe/netprobe.example.yaml \
        /build/package/aeron-netprobe-windows-x86_64/ \
    && cp /opt/xvt-licenses/*.txt /build/package/aeron-netprobe-windows-x86_64/licenses/ \
    && cp /src/aeron/third_party/libyaml/License /build/package/aeron-netprobe-windows-x86_64/licenses/libyaml.txt \
    && cp /src/aeron/third_party/bc7enc/LICENSE /build/package/aeron-netprobe-windows-x86_64/licenses/bc7enc.txt \
    && cp /src/aeron/third_party/fidelityfx-fsr3/LICENSE.txt /build/package/aeron-netprobe-windows-x86_64/licenses/fidelityfx-fsr3.txt \
    && cp /usr/share/doc/gcc-mingw-w64-x86-64-posix-runtime/copyright \
        /build/package/aeron-netprobe-windows-x86_64/licenses/GCC-MinGW-w64.txt \
    && cp /usr/share/doc/mingw-w64-x86-64-dev/copyright \
        /build/package/aeron-netprobe-windows-x86_64/licenses/MinGW-w64.txt \
    && find /build/package/aeron-netprobe-windows-x86_64 -exec touch -h -d "@315532800" {} + \
    && cd /build/package \
    && find aeron-netprobe-windows-x86_64 -print | LC_ALL=C sort > /tmp/netprobe-files \
    && TZ=UTC zip -X -9 -q /build/artifacts/aeron-netprobe-windows-x86_64.zip -@ < /tmp/netprobe-files

FROM scratch AS netprobe-artifact
COPY --from=netprobe-build /build/artifacts/ /

FROM scratch AS artifact
COPY --from=xvt-build /build/artifacts/ /
