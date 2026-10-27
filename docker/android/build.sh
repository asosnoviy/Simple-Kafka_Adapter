#!/usr/bin/env sh
# Builds SimpleKafka1C for the 1C mobile platform (Android) inside a container
# and copies the archive to out/android on the host.
#
#   sh docker/android/build.sh
#   OUTPUT=dist VCPKG_REF=2025.06.13 sh docker/android/build.sh
#
# Diagnostics build: logs the lifecycle of the component to logcat, tag SK1C.
#
#   DIAGNOSTICS=ON OUTPUT=out/android-diag sh docker/android/build.sh

set -eu

OUTPUT="${OUTPUT:-out/android}"
BASE_IMAGE="${BASE_IMAGE:-ubuntu:22.04}"
VCPKG_REF="${VCPKG_REF:-master}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
API_LEVEL="${API_LEVEL:-21}"
DIAGNOSTICS="${DIAGNOSTICS:-OFF}"

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$repo_root"

docker build \
    -f docker/android/Dockerfile \
    --target export \
    --output "$OUTPUT" \
    --build-arg "BASE_IMAGE=$BASE_IMAGE" \
    --build-arg "VCPKG_REF=$VCPKG_REF" \
    --build-arg "BUILD_TYPE=$BUILD_TYPE" \
    --build-arg "API_LEVEL=$API_LEVEL" \
    --build-arg "DIAGNOSTICS=$DIAGNOSTICS" \
    .

echo
echo "Artifacts in $OUTPUT:"
ls -l "$OUTPUT"
