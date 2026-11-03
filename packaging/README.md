# Building OpenXvT packages

| Target | Graphics | Artifact | CI |
|---|---|---|---|
| macOS arm64 | Metal | `.app` in a DMG | Yes |
| macOS x86_64 | Metal | `.app` in a DMG | No |
| Windows x86_64 | D3D12 and Vulkan | ZIP | Yes |
| Linux x86_64 | Vulkan | tar.xz | Yes |
| iOS arm64 | Metal | `OpenXvT.app` for the iPhone Simulator | No |

- Output directory: `build/artifacts` for desktop packages, `build/ios-sim` for iOS.
- Runtime libraries: SDL3, zstd and FFmpeg; libjuice is linked statically.
- curl: bundled on Windows, system-provided on macOS and Linux.
- FFmpeg formats: Smacker, Ogg/Vorbis and WAV.
- Debug: ImGui overlay enabled; macOS includes `OpenXvT.dSYM`.
- Release: ImGui overlay disabled.
- Package builds disable ASAN and UBSan.
- Original game data is not included.

## macOS

Requirements: Xcode command-line tools, CMake, Ninja and pkg-config.
Run on the target architecture. Build variants sequentially; they share caches.

```sh
XVT_VERSION=0.0.0-dev ./packaging/macos/build-package.sh
XVT_BUILD_TYPE=Debug ./packaging/macos/build-package.sh
```

| Environment variable | Default |
|---|---|
| `XVT_VERSION` | `0.0.0-dev` |
| `XVT_BUILD_TYPE` | `Release` |
| `XVT_MACOS_ARCHITECTURE` | Host architecture |
| `XVT_MACOS_DEPLOYMENT_TARGET` | `13.0` |
| `XVT_MACOS_BUILD_ROOT` | `build/cache/macos-<arch>` |
| `XVT_MACOS_ARTIFACT_DIR` | `build/artifacts` |
| `XVT_MACOS_SHADERCROSS_EXECUTABLE` | Build the pinned shader compiler |
| `XVT_MACOS_BUILD_VERSION` | `1` |
| `XVT_MACOS_BUNDLE_IDENTIFIER` | `org.openxvt.openxvt` |
| `XVT_MACOS_SIGN_IDENTITY` | `-` (ad hoc) |

Compiler parallelism: `sysctl -n hw.logicalcpu`.
Bundled dylibs are relocated into `Contents/Frameworks`.

### Signing and notarization

Set `XVT_MACOS_SIGN_IDENTITY` to a Developer ID identity and
`XVT_MACOS_NOTARY_PROFILE` to a notarytool keychain profile.
Alternatively, supply all three direct-credential variables:
`XVT_MACOS_NOTARY_APPLE_ID`, `XVT_MACOS_NOTARY_TEAM_ID` and
`XVT_MACOS_NOTARY_PASSWORD`.

```sh
./packaging/macos/sign-package.sh input.dmg output.dmg
```

`XVT_MACOS_RELEASE_TAG` optionally replaces the signed DMG on a GitHub release.

## iOS

Requirements: Xcode with the iOS simulator SDK, CMake, Ninja, and pkg-config.
Run from a checkout that includes the Aeron submodule. The script builds the
pinned SDL, zstd, curl, FFmpeg, and shader tools, then applies the iOS patches.

iOS builds include full iPhone Duo support and on-screen touch controls for
flight, throttle, weapons, shields, targeting, and the map.

```sh
git submodule update --init --recursive
./packaging/ios/build-simulator.sh
XVT_BUILD_TYPE=Debug ./packaging/ios/build-simulator.sh
```

| Environment variable | Default |
|---|---|
| `XVT_BUILD_TYPE` | `Release` |
| `XVT_IOS_DEPLOYMENT_TARGET` | `27.1` |
| `XVT_IOS_BUNDLE_IDENTIFIER` | `org.openxvt.openxvt` |
| `XVT_IOS_DEPS_ROOT` | `build/ios-deps` |
| `XVT_IOS_SIM_BUILD` | `build/ios-sim` |

The simulator app is `build/ios-sim/OpenXvT.app`.

Pushing an IPA to your own iPhone or iPad requires Developer Mode enabled on
the device and an Apple Developer Account.

## Linux and Windows

Requirements: Docker Buildx with `linux/amd64` support. Run from the repository
root. ARM64 Docker hosts require amd64 emulation.

```sh
docker buildx build --platform linux/amd64 -f packaging/linux/Dockerfile \
  --build-arg XVT_BUILD_TYPE=Release \
  --target artifact --output type=local,dest=build/artifacts .
docker buildx build --platform linux/amd64 -f packaging/windows/Dockerfile \
  --build-arg XVT_BUILD_TYPE=Release \
  --target artifact --output type=local,dest=build/artifacts .
```

Use `--build-arg XVT_BUILD_TYPE=Debug` to build Debug packages.

| Build argument | Default |
|---|---|
| `XVT_VERSION` | `0.0.0-dev` |
| `XVT_BUILD_TYPE` | `Release` |

- Build environment: Debian Bookworm; Windows uses MinGW-w64.
- CMake parallelism: build-tool default; FFmpeg: `make -j"$(nproc)"`.
- BuildKit may run dependency stages concurrently.
- Linux: libraries in `lib/`, executable RPATH `$ORIGIN/lib`, library RPATH `$ORIGIN`.
- Linux host requirements: glibc 2.35+, GCC 12-compatible runtimes, libcurl and graphics/audio drivers.
- Windows: runtime DLLs beside the executable; Debug includes WinPixEventRuntime.
- Release: Windows and Linux debug symbols ship separately in `…-release-symbols` archives.
- A plain non-macOS `cmake --install --component Runtime` stages application files;
  Docker packaging adds runtime dependencies and their licenses.

## Crash reports

Logs, crash stacks and Windows crash dumps are written to the `diagnostics` folder in the
user directory (`%APPDATA%\TotallyOpen\OpenXvT` on Windows,
`~/.local/share/TotallyOpen/OpenXvT` on Linux).
`AERON_TEST_CRASH=access|abort|stack` forces a crash at startup.

Resolve frames from the log with the matching symbols archive. On Windows, add the
`OpenXvT.exe+<offset>` offset to `0x140000000`; on Linux, use the `OpenXvT(+<offset>)`
offset as is:

```sh
llvm-symbolizer --obj=OpenXvT.exe 0x1402d9dbb
```

## Headless multiplayer probe

```sh
git submodule update --init --recursive
docker buildx build --platform linux/amd64 -f packaging/linux/Dockerfile \
  --target netprobe-artifact --output type=local,dest=build/artifacts/netprobe .
docker buildx build --platform linux/amd64 -f packaging/windows/Dockerfile \
  --target netprobe-artifact --output type=local,dest=build/artifacts/netprobe .
```

Configuration and usage: [probe instructions](../aeron/tools/netprobe/README.md).

## CI

| Workflow | Trigger | Configurations | Output |
|---|---|---|---|
| `ci.yml` | Main push | Release and Debug | Workflow artifacts |
| `ci.yml` | Pull request or manual | Release | Workflow artifacts |
| `release.yml` | `v*` tag | Release and Debug | Draft GitHub release |

Package checks cover game configuration, selected shaders, required FFmpeg
codecs, Linux runtime resolution and ABI versions, Windows executable subsystem
and Debug symbols, and macOS plist syntax, dylib paths, signatures and dSYM UUIDs.
CI does not run gameplay tests or notarization.
