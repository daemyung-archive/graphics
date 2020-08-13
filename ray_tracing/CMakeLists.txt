cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(ray_tracing VERSION 0.1.0)

add_executable(ray_tracing
    source/rtweekend.h
    source/vec3.h
    source/color.h
    source/ray.h
    source/hittable.h
    source/sphere.h
    source/hittable_list.h
    source/camera.h
    source/material.h
    source/main.cc
)

set_target_properties(ray_tracing
PROPERTIES
    CXX_STANDARD 17
)
