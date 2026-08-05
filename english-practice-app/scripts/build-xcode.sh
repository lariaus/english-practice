#!/bin/sh
set -e

# Invoked from an Xcode "Run Script" build phase. Relies on Xcode's build
# environment variables (SRCROOT, PLATFORM_NAME, ARCHS, CONFIGURATION,
# IPHONEOS_DEPLOYMENT_TARGET, MACOSX_DEPLOYMENT_TARGET).
#
# Xcode's Run Script phases use a restricted PATH that excludes Homebrew, so
# cmake/ninja/npm (installed under /opt/homebrew on Apple Silicon) must be
# added explicitly here.
export PATH="/opt/homebrew/bin:/usr/local/bin:${PATH}"

APP_ROOT="${SRCROOT}/.."
NATIVE_SERVER_DIR="${APP_ROOT}/native-server"
PLATFORM_NAME="${PLATFORM_NAME:-macosx}"
CONFIGURATION="${CONFIGURATION:-Debug}"
ARCHS="${ARCHS:-arm64}"

npm --prefix "${APP_ROOT}" run build

BUILD_ROOT="${NATIVE_SERVER_DIR}/build-xcode/${PLATFORM_NAME}"
OUT_LIB_DIR="${BUILD_ROOT}/lib"
mkdir -p "${OUT_LIB_DIR}"

ARCH_LIBS=""

for ARCH in ${ARCHS}; do
    ARCH_BUILD_DIR="${BUILD_ROOT}/${ARCH}-${CONFIGURATION}"
    mkdir -p "${ARCH_BUILD_DIR}"

    CMAKE_ARGS="-S \"${NATIVE_SERVER_DIR}\" -B \"${ARCH_BUILD_DIR}\" -G Ninja -DCMAKE_BUILD_TYPE=${CONFIGURATION} -DCMAKE_OSX_ARCHITECTURES=${ARCH} -DNATIVE_SERVER_BUILD_TESTS=OFF"

    case "${PLATFORM_NAME}" in
        iphoneos)
            CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_DEPLOYMENT_TARGET=${IPHONEOS_DEPLOYMENT_TARGET:-16.4} -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
            ;;
        iphonesimulator)
            CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_DEPLOYMENT_TARGET=${IPHONEOS_DEPLOYMENT_TARGET:-16.4} -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
            ;;
        macosx)
            CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-14.0}"
            ;;
        *)
            echo "warning: unhandled PLATFORM_NAME '${PLATFORM_NAME}' (e.g. visionOS) - building natively, untested for this platform"
            ;;
    esac

    eval cmake ${CMAKE_ARGS}
    cmake --build "${ARCH_BUILD_DIR}" --target native_server_core

    # native_server_core now has real (non-header-only) static-library
    # dependencies - youtube_utils, https_client, pugixml, storage_map -
    # that Xcode's own linker has no way to discover on its own
    # (OTHER_LDFLAGS is a fixed, hand-maintained list in the pbxproj, not
    # derived from CMake's dependency graph). Merging them all into one
    # archive here means Xcode's existing "-lnative_server_core" keeps
    # working unchanged instead of needing every new C++ dependency added
    # there by hand.
    MERGED_LIB="${ARCH_BUILD_DIR}/libnative_server_core_merged.a"
    libtool -static -o "${MERGED_LIB}" \
        "${ARCH_BUILD_DIR}/native_server_core/libnative_server_core.a" \
        "${ARCH_BUILD_DIR}/youtube_utils/libyoutube_utils.a" \
        "${ARCH_BUILD_DIR}/https_client/libhttps_client.a" \
        "${ARCH_BUILD_DIR}/storage_map/libstorage_map.a" \
        "${ARCH_BUILD_DIR}/_deps/pugixml-build/libpugixml.a"

    ARCH_LIBS="${ARCH_LIBS} ${MERGED_LIB}"
done

# shellcheck disable=SC2086
if [ "$(echo ${ARCH_LIBS} | wc -w)" -gt 1 ]; then
    lipo -create ${ARCH_LIBS} -output "${OUT_LIB_DIR}/libnative_server_core.a"
else
    cp ${ARCH_LIBS} "${OUT_LIB_DIR}/libnative_server_core.a"
fi

echo "native_server_core: built ${OUT_LIB_DIR}/libnative_server_core.a for ${PLATFORM_NAME} (${ARCHS})"
