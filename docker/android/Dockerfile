# Reproducible Android build of SimpleKafka1C for the 1C mobile platform.
#
# The 1C mobile platform loads an external component as a set of .so files
# packaged into an archive together with MANIFEST.XML. This build produces
# that archive:
#
#   SimpleKafka1C-android.zip
#     MANIFEST.XML                       - description of the archive content
#     ANDROID_MANIFEST_EXTENSIONS.XML    - INTERNET permission for the host app
#     libSimpleKafka1C_ARM64.so          - arm64-v8a
#     libSimpleKafka1C_ARM.so            - armeabi-v7a
#
# The build is the full one: Kafka, AVRO, Protobuf and Schema Registry, with
# the encrypted transport. OpenSSL comes from vcpkg, because the platform has
# no system one, so TLS, SASL SCRAM and OAUTHBEARER work as well.
#
# All third-party dependencies (librdkafka, zlib, snappy, zstd, OpenSSL,
# Boost.JSON, Boost.Algorithm, Boost.PropertyTree, Boost.Container, AVRO,
# Protobuf, curl and fmt) are built from source by vcpkg as static libraries
# and linked into the .so, so the component has no dependencies besides the
# Android runtime itself.

ARG BASE_IMAGE=ubuntu:22.04

FROM ${BASE_IMAGE} AS toolchain

# Android NDK. The toolchain file that drives the cross-compilation lives in
# $ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake.
ARG ANDROID_NDK_DIR=android-ndk-r27c
ARG ANDROID_NDK_ZIP=android-ndk-r27c-linux.zip

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        ninja-build \
        patch \
        pkg-config \
        python3 \
        unzip \
        wget \
        zip \
    && rm -rf /var/lib/apt/lists/*

ENV ANDROID_NDK_HOME=/opt/${ANDROID_NDK_DIR}

RUN wget -q "https://dl.google.com/android/repository/${ANDROID_NDK_ZIP}" -O /tmp/ndk.zip \
    && unzip -q /tmp/ndk.zip -d /opt \
    && rm /tmp/ndk.zip

# ---------------------------------------------------------------------------
# vcpkg + dependencies
# ---------------------------------------------------------------------------
FROM toolchain AS deps

# Pin to a specific vcpkg commit/tag for reproducible dependency versions.
ARG VCPKG_REF=master
ENV VCPKG_ROOT=/opt/vcpkg \
    VCPKG_DISABLE_METRICS=1

RUN for attempt in 1 2 3 4 5; do \
        git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
        && break \
        || { echo "vcpkg clone failed (attempt ${attempt}), retrying"; rm -rf ${VCPKG_ROOT}; sleep $((attempt * 10)); }; \
    done \
    && test -d ${VCPKG_ROOT}/.git \
    && git -C ${VCPKG_ROOT} checkout ${VCPKG_REF} \
    && git -C ${VCPKG_ROOT} rev-parse HEAD > /opt/vcpkg-commit.txt \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

# Overlay ports (librdkafka 2.16.0) and overlay triplets (Android API 21).
COPY vcpkg-overlay /overlay/vcpkg-overlay

ARG TRIPLET_ARM64=arm64-android21
ARG TRIPLET_ARM=arm-neon-android21

# Dependency set for the mobile platform:
#   librdkafka[ssl,curl,zlib,zstd] - Kafka client with the encrypted transport
#   openssl                       - TLS runtime, the platform has no system one
#   boost-json                    - JSON formatting of produced and consumed messages
#   boost-algorithm               - Boost.Algorithm string helpers
#   boost-property-tree           - Boost.PropertyTree JSON reader
#   boost-container               - Boost.Container, needed by the schema sources
#   zlib                          - compression codec for librdkafka
#   snappy, zstd                  - compression codecs for the Kafka messages
#   avro-cpp                      - AVRO serialization
#   protobuf                      - Protobuf runtime parsing, no protoc needed
#   abseil, utf8-range            - dependencies of protobuf, referenced by its config
#   curl                          - Schema Registry over HTTP and https
#   fmt                           - AVRO error formatting
#
# vcpkg splits Boost into one port per library, so the header-only libraries
# the component uses have to be requested explicitly: they are not pulled in by
# boost-json.
#
# Cache mounts keep vcpkg's binary cache and downloads between builds on the
# same host, so a rebuild does not recompile every dependency from scratch.
RUN --mount=type=cache,target=/root/.cache/vcpkg \
    --mount=type=cache,target=/opt/vcpkg/downloads \
    set -eux; \
    PKGS="librdkafka[ssl,curl,zlib,zstd] openssl boost-json boost-algorithm boost-property-tree boost-container zlib snappy zstd avro-cpp[core,snappy] protobuf abseil utf8-range curl[core,ssl] fmt"; \
    for triplet in ${TRIPLET_ARM64} ${TRIPLET_ARM}; do \
        ${VCPKG_ROOT}/vcpkg install --triplet ${triplet} --clean-after-build \
            --overlay-ports=/overlay/vcpkg-overlay \
            --overlay-triplets=/overlay/vcpkg-overlay/triplets \
            ${PKGS}; \
    done

# ---------------------------------------------------------------------------
# Component build
# ---------------------------------------------------------------------------
FROM deps AS build

WORKDIR /src
COPY CMakeLists.txt /src/CMakeLists.txt
COPY include /src/include
COPY src /src/src

# Build-time only changes for the copied sources; the repository is left
# untouched. The patch skips the cyrus-sasl block on Android, fixes the POSIX
# clock and adds the missing <unistd.h>.
COPY docker/android/patches /patches
# The patch is written with LF endings while the sources may come from a
# working tree with CRLF, so the endings are normalized before patching.
RUN find /src -type f \( -name '*.cpp' -o -name '*.h' -o -name 'CMakeLists.txt' \) -exec sed -i 's/\r$//' {} +
RUN for p in /patches/*.patch; do patch -p1 -d /src -i "$p"; done

# Optional diagnostics build. It reports the lifecycle of the component to
# logcat under the tag SK1C and is meant for one-off investigations only.
ARG DIAGNOSTICS=OFF
COPY docker/android/diagnostics /diagnostics
RUN if [ "$DIAGNOSTICS" = "ON" ]; then \
        for p in /diagnostics/*.patch; do patch -p1 -d /src -i "$p"; done; \
    fi

ARG BUILD_TYPE=Release
ARG API_LEVEL=21
ARG TRIPLET_ARM64=arm64-android21
ARG TRIPLET_ARM=arm-neon-android21

# The NDK toolchain is chain-loaded after the vcpkg toolchain: vcpkg resolves
# the dependencies for the target triplet, the NDK toolchain then provides the
# cross-compiler. ANDROID_STL=c++_static keeps the C++ runtime inside the .so,
# so the component does not depend on the runtime of the host application.
#
# avro-cpp headers call fmt::format() but include only <fmt/base.h>. Since
# fmt 11 that declaration lives in <fmt/format.h>, so it is force included
# ahead of every translation unit.
RUN set -eux; \
    EXTRA_CXX_FLAGS="-include fmt/format.h"; \
    cmake -S /src -B /build/arm64 -G Ninja \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=${TRIPLET_ARM64} \
        -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-${API_LEVEL} \
        -DANDROID_NATIVE_API_LEVEL=${API_LEVEL} \
        -DANDROID_STL=c++_static \
        -DCMAKE_CXX_FLAGS="-isystem ${VCPKG_ROOT}/installed/${TRIPLET_ARM64}/include ${EXTRA_CXX_FLAGS}" \
    && cmake --build /build/arm64 \
    && cmake -S /src -B /build/arm -G Ninja \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=${TRIPLET_ARM} \
        -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake \
        -DANDROID_ABI=armeabi-v7a \
        -DANDROID_PLATFORM=android-${API_LEVEL} \
        -DANDROID_NATIVE_API_LEVEL=${API_LEVEL} \
        -DANDROID_STL=c++_static \
        -DCMAKE_CXX_FLAGS="-isystem ${VCPKG_ROOT}/installed/${TRIPLET_ARM}/include ${EXTRA_CXX_FLAGS}" \
    && cmake --build /build/arm

# Collect the artifacts and the build provenance next to them.
RUN mkdir -p /artifact \
    && cp /build/arm64/libSimpleKafka1C.so /artifact/libSimpleKafka1C_ARM64.so \
    && cp /build/arm/libSimpleKafka1C.so /artifact/libSimpleKafka1C_ARM.so \
    && ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip --strip-unneeded /artifact/*.so \
    && { \
        echo "ndk             : $(cat ${ANDROID_NDK_HOME}/source.properties)"; \
        echo "vcpkg commit    : $(cat /opt/vcpkg-commit.txt)"; \
        echo "api level       : ${API_LEVEL}"; \
        echo "build type      : ${BUILD_TYPE}"; \
        echo "triplet arm64   : ${TRIPLET_ARM64}"; \
        echo "triplet arm     : ${TRIPLET_ARM}"; \
        echo "sha256 arm64    : $(sha256sum /artifact/libSimpleKafka1C_ARM64.so | cut -d' ' -f1)"; \
        echo "sha256 arm      : $(sha256sum /artifact/libSimpleKafka1C_ARM.so | cut -d' ' -f1)"; \
        echo ""; \
        echo "== arm64: exported symbols =="; \
        ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm -D --defined-only /artifact/libSimpleKafka1C_ARM64.so | awk '{print $NF}' | sort; \
        echo ""; \
        echo "== arm64: needed libraries =="; \
        ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf -d /artifact/libSimpleKafka1C_ARM64.so | grep NEEDED || echo "(none)"; \
        echo ""; \
        echo "== arm: exported symbols =="; \
        ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm -D --defined-only /artifact/libSimpleKafka1C_ARM.so | awk '{print $NF}' | sort; \
        echo ""; \
        echo "== arm: needed libraries =="; \
        ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf -d /artifact/libSimpleKafka1C_ARM.so | grep NEEDED || echo "(none)"; \
    } > /artifact/build-info.txt 2>&1

# ---------------------------------------------------------------------------
# Package stage: add the manifest and pack the archive for the 1C platform.
# ---------------------------------------------------------------------------
FROM build AS package

COPY docker/android/MANIFEST.XML /artifact/MANIFEST.XML
COPY docker/android/ANDROID_MANIFEST_EXTENSIONS.XML /artifact/ANDROID_MANIFEST_EXTENSIONS.XML

RUN cd /artifact \
    && zip -9 -r SimpleKafka1C-android.zip \
        MANIFEST.XML \
        ANDROID_MANIFEST_EXTENSIONS.XML \
        libSimpleKafka1C_ARM64.so \
        libSimpleKafka1C_ARM.so

# ---------------------------------------------------------------------------
# Export stage: `docker build --target export --output <dir>` writes the
# artifact to the host without creating an image.
# ---------------------------------------------------------------------------
FROM scratch AS export
COPY --from=package /artifact/ /
