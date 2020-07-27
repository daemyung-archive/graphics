#
# This file is part of the "graphics" project
# See "LICENSE" for license information.
#

cmake_minimum_required(VERSION 3.15)
project(skinning)

find_package(spdlog CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

add_executable(skinning
    src/renderer.h
    src/renderer.cpp
    src/skinning.metal
    src/document.h
    src/document.cpp
    src/glfw_lib.h
    src/gltf_lib.h
    src/main.cpp
)

target_include_directories(skinning
PRIVATE
    ${TINYGLTF_INCLUDE_DIRS}
)

target_compile_options(skinning
PRIVATE
    "-xobjective-c++"
)

target_link_libraries(skinning
PRIVATE
    "-framework QuartzCore"
    "-framework Metal"
    spdlog::spdlog
    glfw
    nlohmann_json
)

set_target_properties(skinning
PROPERTIES
    CXX_STANDARD 17
    MACOSX_BUNDLE TRUE
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC TRUE
)
