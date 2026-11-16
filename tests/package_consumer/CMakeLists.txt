cmake_minimum_required(VERSION 3.20)
project(raytracer_package_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raytracer 1.0 CONFIG REQUIRED)

add_executable(raytracer_package_consumer main.cpp)
target_link_libraries(raytracer_package_consumer PRIVATE RayTracer::raytracer_core)
