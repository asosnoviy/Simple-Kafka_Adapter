# Connect harness: emulates the sequence of calls the 1C platform performs
# while connecting an external component.
#
# The mobile platform may load the component without providing a memory
# manager. In that mode the component has to allocate the buffers for the
# values it returns on its own - otherwise the connect sequence crashes the
# process. The harness runs the sequence both with and without a memory
# manager.
#
# Build it with the same toolchain and triplet as the component:
#   cmake -S tests/connect-harness -B out/build/connect-harness -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build out/build/connect-harness
#
# Run it against the built component:
#   ./out/build/connect-harness/connect_harness out/rhel9/SimpleKafka1C.so
#   ./out/build/connect-harness/connect_harness out/rhel9/SimpleKafka1C.so --with-mem-manager
cmake_minimum_required(VERSION 3.20)
project(connect_harness CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(connect_harness main.cpp)
target_include_directories(connect_harness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

if(MSVC)
    target_compile_definitions(connect_harness PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(connect_harness PRIVATE ${CMAKE_DL_LIBS})
else()
    target_link_libraries(connect_harness PRIVATE ${CMAKE_DL_LIBS})
endif()
