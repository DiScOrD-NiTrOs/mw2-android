cmake_minimum_required(VERSION 3.20)
project(iw4l_cpp LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
# The MinGW-inherited toolchain flags include `-Xclang -gcodeview`; strip those for gcc.
string(REPLACE "-Xclang -gcodeview" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
string(REPLACE "-Xclang -gcodeview" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

# Leaf crates first; dependencies added as later crates are ported.
add_library(iw4l_core STATIC
    src/math_iw4/math_iw4.cpp
    src/anim_iw4/anim_iw4.cpp
    src/playerstate_iw4/playerstate_iw4.cpp
    src/movement_iw4/movement_iw4.cpp
    src/movement_iw4/movement_impl.cpp
    src/movement_iw4/movement_events.cpp
    src/movement_iw4/movement_footstep_jump.cpp
    src/fastfile_iw4/fastfile_iw4.cpp
    src/fastfile_iw4/fastfile_wire.cpp
    src/trace_iw4/trace_iw4.cpp
    src/movement_iw4/movement_ground_walk_air.cpp
    src/input_iw4/input_iw4.cpp
    src/clipmap_iw4/clipmap_iw4.cpp
    src/clipmap_iw4/capsule_query.cpp
    src/entity_iw4/glass_packed_dir.cpp
)
target_include_directories(iw4l_core PUBLIC src)

add_executable(port_tests
    tests/port_tests.cpp
)
target_link_libraries(port_tests PRIVATE iw4l_core)

add_executable(port_tests_input
    tests/port_tests_input.cpp
)
target_link_libraries(port_tests_input PRIVATE iw4l_core)

enable_testing()
add_test(NAME port_tests COMMAND port_tests)
add_test(NAME port_tests_input COMMAND port_tests_input)
