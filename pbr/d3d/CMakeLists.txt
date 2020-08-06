cmake_minimum_required(VERSION 3.15.0 FATAL_ERROR)
project(pbr VERSION 0.1.0)

find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_executable(pbr
    src/d3dx12.h
    src/GeometryGenerator.h
    src/GeometryGenerator.cpp
    src/stb_image.h
    src/d3d_helper.h
    src/mesh.h
    src/constant.h
    src/renderer.h
    src/renderer.cpp
    src/glfw_lib.h
    src/stb_lib.h
    src/main.cpp
)

target_link_libraries(pbr
PRIVATE
    windowsapp
    dxgi
    d3dcompiler
    d3d12
    glfw
    spdlog::spdlog
)

set_target_properties(pbr
PROPERTIES
    CXX_STANDARD 17
)