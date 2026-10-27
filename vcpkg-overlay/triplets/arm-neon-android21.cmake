# armeabi-v7a (32-bit ARM) triplet for the 1C mobile platform.
#
# Same as the upstream vcpkg "arm-neon-android" community triplet, but pinned
# to Android API level 21 (Android 5.0), which is the oldest release supported
# by the 1C mobile platform. The upstream triplet targets API level 28
# (Android 9) and would leave Android 5-8 devices without the component.
#
# All libraries are linked statically: the 1C mobile platform loads the
# component as a single .so with no companion shared objects.

set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 21)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=armv7a-linux-androideabi")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=armeabi-v7a -DANDROID_ARM_NEON=ON)
