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
    src/fastfile_iw4/fastfile_iw4.cpp
)
target_include_directories(iw4l_core PUBLIC src)

add_executable(port_tests
    tests/port_tests.cpp
)
target_link_libraries(port_tests PRIVATE iw4l_core)

enable_testing()
add_test(NAME port_tests COMMAND port_tests)
