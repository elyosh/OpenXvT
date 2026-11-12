# Microsoft's Linux DXC release binaries are available for x86-64 only.
ARG XVT_BUILD_PLATFORM=linux/amd64

FROM --platform=${XVT_BUILD_PLATFORM} debian:bookworm-slim AS build-base

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        file \
        git \
        libasound2-dev \
        libdbus-1-dev \
        libdecor-0-dev \
        libdrm-dev \
        libegl1-mesa-dev \
        libgbm-dev \
        libgl-dev \
        libibus-1.0-dev \
        libncurses-dev \
        libpipewire-0.3-dev \
        libpulse-dev \
        libudev-dev \
        liburing-dev \
        libvulkan-dev \
        libwayland-dev \
        libx11-dev \
        libxcursor-dev \
        libxext-dev \
        libxfixes-dev \
        libxi-dev \
        libxkbcommon-dev \
        libxml2-dev \
        libxrandr-dev \
        libxss-dev \
        libxtst-dev \
        nasm \
        ninja-build \
        patchelf \
        pkg-config \
        python3 \
        wayland-protocols \
        xz-utils \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# The versions and patches of aeron's dependencies come from
# aeron/packaging/dependencies; set these to build another version.
ARG SDL_VERSION=
ARG ZSTD_VERSION=
ARG FFMPEG_VERSION=
ARG SHADERCROSS_COMMIT=

# The dependencies build as independent stages so that a version bump only
# rebuilds the affected dependency and so that cold builds run concurrently.

FROM build-base AS network-build-base

RUN apt-get update && apt-get install -y --no-install-recommends libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

FROM build-base AS sdl-build

COPY aeron/packaging/dependencies/apply-patches.sh /tmp/aeron/apply-patches.sh
COPY aeron/packaging/dependencies/sdl /tmp/aeron/sdl

RUN git clone --branch "release-${SDL_VERSION:-$(cat /tmp/aeron/sdl/version)}" --depth 1 \
        https://github.com/libsdl-org/SDL.git /deps/SDL \
    && /tmp/aeron/apply-patches.sh /tmp/aeron/sdl /deps/SDL \
    && cmake -S /deps/SDL -B /build/sdl -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-linux \
        -DSDL_INSTALL_DOCS=OFF \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF \
    && cmake --build /build/sdl --parallel \
    && cmake --install /build/sdl

FROM build-base AS zstd-build

COPY aeron/packaging/dependencies/zstd /tmp/aeron/zstd

RUN git clone --branch "v${ZSTD_VERSION:-$(cat /tmp/aeron/zstd/version)}" --depth 1 \
        https://github.com/facebook/zstd.git /deps/zstd \
    && cmake -S /deps/zstd/build/cmake -B /build/zstd -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-linux \
        -DZSTD_BUILD_PROGRAMS=OFF \
        -DZSTD_BUILD_SHARED=ON \
        -DZSTD_BUILD_STATIC=OFF \
        -DZSTD_BUILD_TESTS=OFF \
    && cmake --build /build/zstd --parallel \
    && cmake --install /build/zstd

FROM build-base AS ffmpeg-build

COPY aeron/packaging/dependencies/ffmpeg /tmp/aeron/ffmpeg

RUN git clone --branch "n${FFMPEG_VERSION:-$(cat /tmp/aeron/ffmpeg/version)}" --depth 1 \
        https://github.com/FFmpeg/FFmpeg.git /deps/FFmpeg \
    && cd /deps/FFmpeg \
    && ./configure \
        --prefix=/opt/xvt-linux \
        --disable-autodetect \
        --disable-avdevice \
        --disable-avfilter \
        --disable-debug \
        --disable-doc \
        --disable-everything \
        --disable-network \
        --disable-programs \
        --disable-static \
        --enable-decoder=smacker,smackaud,pcm_u8,pcm_s16le,vorbis,binkaudio_rdft,binkaudio_dct \
        --enable-parser=vorbis \
        --enable-demuxer=smacker,ogg,wav \
        --enable-pic \
        --enable-pthreads \
        --enable-shared \
    && for feature in SMACKER_DECODER SMACKAUD_DECODER VORBIS_DECODER \
        PCM_U8_DECODER PCM_S16LE_DECODER SMACKER_DEMUXER OGG_DEMUXER WAV_DEMUXER; do \
        grep -q "^#define CONFIG_${feature} 1$" config_components.h || exit 1; \
    done \
    && make -j"$(nproc)" \
    && make install

FROM build-base AS shader-tools

COPY aeron/packaging/dependencies/apply-patches.sh /tmp/aeron/apply-patches.sh
COPY aeron/packaging/dependencies/shadercross /tmp/aeron/shadercross

# DXC 1.9 requires glibc 2.38; keep the Debian 12-compatible 1.8 release.
ARG DXC_VERSION=1.8.2502
ARG DXC_LINUX_X64_ARCHIVE=linux_dxc_2025_02_20.x86_64.tar.gz
ARG DXC_LINUX_X64_SHA256=e0580d90dbf6053a783ddd8d5153285f0606e5deaad17a7a6452f03acdf88c71

ADD --checksum=sha256:${DXC_LINUX_X64_SHA256} \
    https://github.com/microsoft/DirectXShaderCompiler/releases/download/v${DXC_VERSION}/${DXC_LINUX_X64_ARCHIVE} \
    /tmp/dxc.tar.gz

# shadercross links SDL but needs neither zstd nor FFmpeg.
COPY --from=sdl-build /opt/xvt-linux /opt/xvt-linux

RUN git clone https://github.com/libsdl-org/SDL_shadercross.git /deps/SDL_shadercross \
    && git -C /deps/SDL_shadercross checkout "${SHADERCROSS_COMMIT:-$(cat /tmp/aeron/shadercross/version)}" \
    && /tmp/aeron/apply-patches.sh /tmp/aeron/shadercross /deps/SDL_shadercross \
    && git -C /deps/SDL_shadercross submodule update --init --depth 1 \
        external/SPIRV-Cross \
    && mkdir -p \
        /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux \
    && tar -xzf /tmp/dxc.tar.gz \
        -C /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux \
    && cmake -S /deps/SDL_shadercross/external/SPIRV-Cross \
        -B /build/spirv-cross -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-tools \
        -DSPIRV_CROSS_CLI=OFF \
        -DSPIRV_CROSS_ENABLE_TESTS=OFF \
        -DSPIRV_CROSS_SHARED=ON \
        -DSPIRV_CROSS_STATIC=OFF \
    && cmake --build /build/spirv-cross --parallel \
    && cmake --install /build/spirv-cross \
    && cmake -S /deps/SDL_shadercross -B /build/shadercross -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xvt-tools \
        -DCMAKE_PREFIX_PATH="/opt/xvt-linux;/opt/xvt-tools" \
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
        /opt/xvt-tools/lib/

FROM network-build-base AS xvt-build

ARG XVT_BUILD_TYPE=Release
ARG XVT_VERSION=0.0.0-dev

COPY --from=sdl-build /opt/xvt-linux /opt/xvt-linux
COPY --from=zstd-build /opt/xvt-linux /opt/xvt-linux
COPY --from=ffmpeg-build /opt/xvt-linux /opt/xvt-linux
COPY --from=sdl-build /deps/SDL/LICENSE.txt /opt/xvt-licenses/SDL3.txt
COPY --from=zstd-build /deps/zstd/LICENSE /opt/xvt-licenses/zstd.txt
COPY --from=ffmpeg-build /deps/FFmpeg/COPYING.LGPLv2.1 /opt/xvt-licenses/FFmpeg-LGPL-2.1.txt
COPY --from=shader-tools /opt/xvt-tools /opt/xvt-tools
COPY CMakeLists.txt /src/CMakeLists.txt
COPY README.md /src/README.md
COPY aeron /src/aeron
COPY cmake /src/cmake
COPY licenses /src/licenses
COPY packaging /src/packaging
COPY resources /src/resources
COPY shaders /src/shaders
COPY src /src/src

ENV LD_LIBRARY_PATH=/opt/xvt-tools/lib:/opt/xvt-linux/lib
ENV PKG_CONFIG_PATH=/opt/xvt-linux/lib/pkgconfig

RUN case "${XVT_BUILD_TYPE}" in \
        Release) xvt_debug_ui=OFF; xvt_release_flags="-O3 -DNDEBUG -g" ;; \
        Debug) xvt_debug_ui=ON; xvt_release_flags="-O3 -DNDEBUG" ;; \
        *) echo "XVT_BUILD_TYPE must be Release or Debug" >&2; exit 2 ;; \
    esac \
    && cmake -S /src -B /build/xvt -G Ninja \
        -DCMAKE_BUILD_TYPE="${XVT_BUILD_TYPE}" \
        -DCMAKE_PREFIX_PATH=/opt/xvt-linux \
        -DCMAKE_C_FLAGS_RELEASE="${xvt_release_flags}" \
        -DCMAKE_CXX_FLAGS_RELEASE="${xvt_release_flags}" \
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,--build-id" \
        -DAERON_SHADERCROSS_EXECUTABLE=/opt/xvt-tools/bin/shadercross \
        -DXVT_BUILD_TOOLS=OFF \
        -DXVT_ENABLE_ASAN=OFF \
        -DXVT_ENABLE_UBSAN=OFF \
        -DXVT_ENABLE_DEBUG_UI="${xvt_debug_ui}" \
        -DXVT_VERSION="${XVT_VERSION}" \
    && cmake --build /build/xvt --target xvt --parallel

RUN cmake --install /build/xvt --component Runtime --prefix /build/package/openxvt-linux-x86_64 \
    && mkdir -p /build/package/openxvt-linux-x86_64/licenses /build/package/openxvt-linux-x86_64/lib \
    && cp /opt/xvt-licenses/SDL3.txt \
        /build/package/openxvt-linux-x86_64/licenses/SDL3.txt \
    && cp /opt/xvt-licenses/zstd.txt \
        /build/package/openxvt-linux-x86_64/licenses/zstd.txt \
    && cp /opt/xvt-licenses/FFmpeg-LGPL-2.1.txt \
        /build/package/openxvt-linux-x86_64/licenses/FFmpeg-LGPL-2.1.txt \
    && cp -L /opt/xvt-linux/lib/libSDL3.so.0 \
        /build/package/openxvt-linux-x86_64/lib/ \
    && cp -L /opt/xvt-linux/lib/libavcodec.so.62 \
        /build/package/openxvt-linux-x86_64/lib/ \
    && cp -L /opt/xvt-linux/lib/libavformat.so.62 \
        /build/package/openxvt-linux-x86_64/lib/ \
    && cp -L /opt/xvt-linux/lib/libavutil.so.60 \
        /build/package/openxvt-linux-x86_64/lib/ \
    && cp -L /opt/xvt-linux/lib/libswresample.so.6 \
        /build/package/openxvt-linux-x86_64/lib/ \
    && cp -L /opt/xvt-linux/lib/libswscale.so.9 \
        /build/package/openxvt-linux-x86_64/lib/ \
    && cp -L /opt/xvt-linux/lib/libzstd.so.1 \
        /build/package/openxvt-linux-x86_64/lib/ \
    && patchelf --set-rpath '$ORIGIN/lib' \
        /build/package/openxvt-linux-x86_64/OpenXvT \
    && for library in /build/package/openxvt-linux-x86_64/lib/*.so*; do \
        patchelf --set-rpath '$ORIGIN' "$library"; \
    done \
    # Release symbols ship separately, matched to the binary by its build ID.
    && if [ "${XVT_BUILD_TYPE}" = Release ]; then \
        mkdir -p /build/symbols \
        && cd /build/package/openxvt-linux-x86_64 \
        && objcopy --only-keep-debug OpenXvT /build/symbols/OpenXvT.debug \
        && cp /build/symbols/OpenXvT.debug . \
        && objcopy --strip-debug --add-gnu-debuglink=OpenXvT.debug OpenXvT \
        && rm OpenXvT.debug \
        && cp OpenXvT /build/symbols/OpenXvT; \
    fi

RUN test -f /build/package/openxvt-linux-x86_64/shaders/aeron_dx5_projected_triangle.vert.spv \
    && test -f /build/package/openxvt-linux-x86_64/shaders/hyperspace_streak.vert.spv \
    && test -f /build/package/openxvt-linux-x86_64/shaders/hyperspace_streak.frag.spv \
    && test -f /build/package/openxvt-linux-x86_64/shaders/hyperspace_tunnel.vert.spv \
    && test -f /build/package/openxvt-linux-x86_64/shaders/hyperspace_tunnel.frag.spv \
    && test -f /build/package/openxvt-linux-x86_64/resources/aeron/scene3d_defaults.yaml \
    && test -f /build/package/openxvt-linux-x86_64/resources/config.yaml \
    && file /build/package/openxvt-linux-x86_64/OpenXvT \
    && env -u LD_LIBRARY_PATH ldd /build/package/openxvt-linux-x86_64/OpenXvT \
    && env -u LD_LIBRARY_PATH ldd /build/package/openxvt-linux-x86_64/OpenXvT \
        | grep -q 'libcurl\.so\.4' \
    && ! env -u LD_LIBRARY_PATH \
        ldd /build/package/openxvt-linux-x86_64/OpenXvT | grep -q "not found" \
    # The GCC runtime must resolve from the host, never from the package: the
    # host's Mesa Vulkan ICDs dlopen into the process and bind whichever
    # libstdc++ is already loaded, so a bundled (older) copy breaks Vulkan
    # device enumeration on newer distros.
    && ! env -u LD_LIBRARY_PATH \
        ldd /build/package/openxvt-linux-x86_64/OpenXvT \
        | grep -E 'libstdc[+][+]|libgcc_s' | grep -q '/build/package' \
    # Host baseline contract: glibc 2.35 and the GCC 12 runtime (Ubuntu 22.04
    # era). Fail packaging if any shipped object requires newer symbol
    # versions, or the glibc-2.36-only DT_RELR ABI, so a toolchain or
    # dependency bump cannot silently raise the floor.
    && for object in /build/package/openxvt-linux-x86_64/OpenXvT \
        /build/package/openxvt-linux-x86_64/lib/*.so*; do \
        if readelf -V "$object" | grep -q 'GLIBC_ABI_'; then \
            echo "$object requires a GLIBC_ABI_* version (DT_RELR needs glibc 2.36+)" >&2; \
            exit 1; \
        fi; \
        for bound in GLIBC_2.35 GLIBCXX_3.4.30 CXXABI_1.3.13; do \
            required=$(readelf -V "$object" \
                | grep -oE "${bound%_*}_[0-9][0-9.]*" | sort -uV | tail -n 1); \
            if [ -n "$required" ] && \
                [ "$(printf '%s\n%s\n' "$required" "$bound" | sort -V | tail -n 1)" != "$bound" ]; then \
                echo "$object requires $required, above the $bound host baseline" >&2; \
                exit 1; \
            fi; \
        done; \
    done \
    && readelf -n /build/package/openxvt-linux-x86_64/OpenXvT | grep -q 'Build ID' \
    && if [ "${XVT_BUILD_TYPE}" = Debug ]; then \
        readelf --sections /build/package/openxvt-linux-x86_64/OpenXvT \
            | grep -q '\.debug_info'; \
    else \
        ! readelf --sections /build/package/openxvt-linux-x86_64/OpenXvT \
            | grep -q '\.debug_info' \
        && readelf --sections /build/symbols/OpenXvT.debug | grep -q '\.debug_info'; \
    fi \
    && if [ "${XVT_BUILD_TYPE}" = Debug ]; then \
        artifact_variant=debug; \
    else \
        artifact_variant=release; \
    fi \
    && package_name="openxvt-${XVT_VERSION}-linux-x86_64-${artifact_variant}" \
    && mv /build/package/openxvt-linux-x86_64 "/build/package/${package_name}" \
    && mkdir -p /build/artifacts \
    && tar \
        --sort=name \
        --mtime="@0" \
        --owner=0 \
        --group=0 \
        --numeric-owner \
        -C /build/package \
        -cJf "/build/artifacts/${package_name}.tar.xz" \
        "${package_name}" \
    && tar -tJf "/build/artifacts/${package_name}.tar.xz" \
    && if [ "${XVT_BUILD_TYPE}" = Release ]; then \
        mv /build/symbols "/build/${package_name}-symbols" \
        && tar \
            --sort=name \
            --mtime="@0" \
            --owner=0 \
            --group=0 \
            --numeric-owner \
            -C /build \
            -cJf "/build/artifacts/${package_name}-symbols.tar.xz" \
            "${package_name}-symbols" \
        && tar -tJf "/build/artifacts/${package_name}-symbols.tar.xz"; \
    fi

FROM network-build-base AS netprobe-build

COPY --from=sdl-build /opt/xvt-linux /opt/xvt-linux
COPY --from=zstd-build /opt/xvt-linux /opt/xvt-linux
COPY --from=shader-tools /opt/xvt-tools /opt/xvt-tools
COPY --from=sdl-build /deps/SDL/LICENSE.txt /opt/xvt-licenses/SDL3.txt
COPY --from=zstd-build /deps/zstd/LICENSE /opt/xvt-licenses/zstd.txt
COPY aeron /src/aeron

ENV LD_LIBRARY_PATH=/opt/xvt-tools/lib:/opt/xvt-linux/lib

RUN cmake -S /src/aeron -B /build/netprobe -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH=/opt/xvt-linux \
        -DAERON_SHADERCROSS_EXECUTABLE=/opt/xvt-tools/bin/shadercross \
        -DAERON_ENABLE_COMPAT=ON -DAERON_ENABLE_DPLAY=ON \
        -DAERON_ENABLE_VIDEO=OFF -DAERON_ENABLE_AUDIO_DECODE=OFF \
        -DAERON_BUILD_TOOLS=OFF \
    && cmake --build /build/netprobe --target aeron-netprobe --parallel

RUN mkdir -p /build/package/aeron-netprobe-linux-x86_64/lib \
        /build/package/aeron-netprobe-linux-x86_64/licenses /build/artifacts \
    && cp /build/netprobe/aeron-netprobe /build/package/aeron-netprobe-linux-x86_64/ \
    && cp /src/aeron/tools/netprobe/README.md /src/aeron/tools/netprobe/netprobe.example.yaml \
        /build/package/aeron-netprobe-linux-x86_64/ \
    && cp /opt/xvt-licenses/*.txt /build/package/aeron-netprobe-linux-x86_64/licenses/ \
    && cp /src/aeron/third_party/libjuice/LICENSE /build/package/aeron-netprobe-linux-x86_64/licenses/libjuice.txt \
    && cp /src/aeron/third_party/libyaml/License /build/package/aeron-netprobe-linux-x86_64/licenses/libyaml.txt \
    && cp /src/aeron/third_party/bc7enc/LICENSE /build/package/aeron-netprobe-linux-x86_64/licenses/bc7enc.txt \
    && cp /src/aeron/third_party/fidelityfx-fsr3/LICENSE.txt /build/package/aeron-netprobe-linux-x86_64/licenses/fidelityfx-fsr3.txt \
    && cp -L /opt/xvt-linux/lib/libSDL3.so.0 /opt/xvt-linux/lib/libzstd.so.1 \
        /build/package/aeron-netprobe-linux-x86_64/lib/ \
    && patchelf --set-rpath '$ORIGIN/lib' /build/package/aeron-netprobe-linux-x86_64/aeron-netprobe \
    && for library in /build/package/aeron-netprobe-linux-x86_64/lib/*.so*; do \
        patchelf --set-rpath '$ORIGIN' "$library"; \
    done \
    && tar --sort=name --mtime="@0" --owner=0 --group=0 --numeric-owner \
        -C /build/package -cJf /build/artifacts/aeron-netprobe-linux-x86_64.tar.xz \
        aeron-netprobe-linux-x86_64

FROM scratch AS netprobe-artifact
COPY --from=netprobe-build /build/artifacts/ /

FROM scratch AS artifact
COPY --from=xvt-build /build/artifacts/ /
