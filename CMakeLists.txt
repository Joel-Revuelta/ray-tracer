cmake_minimum_required(VERSION 3.16)
project(raytracer)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler options (host C++ only; nvcc uses its own flags)
add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-fno-gnu-unique>
    $<$<COMPILE_LANGUAGE:CXX>:-W>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Werror>
    $<$<COMPILE_LANGUAGE:CXX>:-g3>
)

# Include directories
include_directories(include)
include_directories(include/utils)
include_directories(include/scene)
include_directories(include/factory)
include_directories(include/exceptions)
include_directories(include/plugins/lights)
include_directories(include/plugins/materials)
include_directories(include/plugins/parser)
include_directories(include/plugins/primitives)
include_directories(include/plugins/transformations)
include_directories(include/plugins/GUI)
include_directories(include/plugins/renderers)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIG++ REQUIRED libconfig++)
include_directories(${LIBCONFIG++_INCLUDE_DIRS})
find_package(Threads REQUIRED)
find_package(SFML 3.0 COMPONENTS Graphics Window System Audio REQUIRED)

# Shared core runtime used by the executable and every plugin.
file(GLOB_RECURSE SOURCES_UTILS "src/utils/*.cpp")
file(GLOB_RECURSE SOURCES_SCENE "src/scene/*.cpp")
file(GLOB_RECURSE SOURCES_DLMANAGER "src/DLManager/*.cpp")
file(GLOB_RECURSE SOURCES_FACTORY "src/factory/*.cpp")
file(GLOB_RECURSE SOURCES_PARSER "src/plugins/parser/*.cpp")
file(GLOB_RECURSE SOURCES_RENDER_COMMON "src/render/*.cpp")

add_library(raytracer_core SHARED
    ${SOURCES_UTILS}
    ${SOURCES_SCENE}
    ${SOURCES_DLMANAGER}
    ${SOURCES_FACTORY}
    ${SOURCES_PARSER}
    ${SOURCES_RENDER_COMMON}
    src/Camera.cpp
)
target_link_libraries(raytracer_core PUBLIC
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    ${LIBCONFIG++_LIBRARIES}
)

# Core executable
add_executable(raytracer src/main.cpp src/App.cpp)
target_link_libraries(raytracer PRIVATE raytracer_core)

# CPU renderer plugin
add_library(raytracer_renderer_cpu SHARED
    src/plugins/renderers/CPU/CPURenderer.cpp
    src/plugins/renderers/CPU/Renderer.cpp
)
target_link_libraries(raytracer_renderer_cpu PRIVATE raytracer_core)

# CUDA renderer plugin (optional)
option(RAYTRACER_ENABLE_CUDA "Build the CUDA renderer when the toolkit is available" ON)
if (RAYTRACER_ENABLE_CUDA)
    find_package(CUDAToolkit QUIET)
    if (CUDAToolkit_FOUND)
        if ((NOT CMAKE_CUDA_COMPILER OR CMAKE_CUDA_COMPILER STREQUAL "CMAKE_CUDA_COMPILER-NOTFOUND")
            AND CUDAToolkit_NVCC_EXECUTABLE)
            set(CMAKE_CUDA_COMPILER "${CUDAToolkit_NVCC_EXECUTABLE}" CACHE FILEPATH "CUDA compiler" FORCE)
        endif()
        enable_language(CUDA)
        add_library(raytracer_renderer_cuda SHARED
            src/plugins/renderers/CUDA/CUDARenderer.cpp
            src/plugins/renderers/CUDA/Kernels.cu
        )
        target_link_libraries(raytracer_renderer_cuda PRIVATE raytracer_core CUDA::cudart_static)
        set_target_properties(raytracer_renderer_cuda PROPERTIES
            CUDA_STANDARD 20
            CUDA_STANDARD_REQUIRED ON
            CUDA_ARCHITECTURES 89
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../plugins"
            PREFIX ""
        )
        message(STATUS "CUDA renderer enabled (CUDA ${CUDAToolkit_VERSION})")
    else()
        message(STATUS "CUDA renderer disabled: CUDA toolkit not found")
    endif()
endif()

# CLI plugin
file(GLOB_RECURSE SOURCES_CLI "src/plugins/CLI/*.cpp")
add_library(raytracer_cli SHARED ${SOURCES_CLI})
target_link_libraries(raytracer_cli PRIVATE raytracer_core)

# GUI plugin
file(GLOB_RECURSE SOURCES_GUI "src/plugins/GUI/*.cpp")
add_library(raytracer_gui SHARED ${SOURCES_GUI})
target_link_libraries(raytracer_gui PRIVATE raytracer_core SFML::Graphics SFML::Window SFML::System SFML::Audio)

# Lights
file(GLOB_RECURSE SOURCES_AMBIENT "src/plugins/lights/AmbientLight.cpp")
add_library(raytracer_light_ambient SHARED ${SOURCES_AMBIENT})
target_link_libraries(raytracer_light_ambient PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_DIRECTIONAL "src/plugins/lights/DirectionalLight.cpp")
add_library(raytracer_light_directional SHARED ${SOURCES_DIRECTIONAL})
target_link_libraries(raytracer_light_directional PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_POINT "src/plugins/lights/PointLight.cpp")
add_library(raytracer_light_point SHARED ${SOURCES_POINT})
target_link_libraries(raytracer_light_point PRIVATE raytracer_core)

# Primitives
file(GLOB_RECURSE SOURCES_PLANE "src/plugins/primitives/Plane.cpp")
file(GLOB_RECURSE EXTERN_PLANE "src/plugins/primitives/ExternPlane.cpp")
add_library(raytracer_plane SHARED ${SOURCES_PLANE} ${EXTERN_PLANE})
target_link_libraries(raytracer_plane PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_SPHERE "src/plugins/primitives/Sphere.cpp")
file(GLOB_RECURSE EXTERN_SPHERE "src/plugins/primitives/ExternSphere.cpp")
add_library(raytracer_sphere SHARED ${SOURCES_SPHERE} ${EXTERN_SPHERE})
target_link_libraries(raytracer_sphere PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_CYLINDER "src/plugins/primitives/Cylinder.cpp")
file(GLOB_RECURSE EXTERN_CYLINDER "src/plugins/primitives/ExternCylinder.cpp")
add_library(raytracer_cylinder SHARED ${SOURCES_CYLINDER} ${EXTERN_CYLINDER})
target_link_libraries(raytracer_cylinder PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_CONE "src/plugins/primitives/Cone.cpp")
file(GLOB_RECURSE EXTERN_CONE "src/plugins/primitives/ExternCone.cpp")
add_library(raytracer_cone SHARED ${SOURCES_CONE} ${EXTERN_CONE})
target_link_libraries(raytracer_cone PRIVATE raytracer_core)

# Materials
file(GLOB_RECURSE SOURCES_CUSTOM_MATERIAL "src/plugins/materials/CustomMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_custom SHARED ${SOURCES_CUSTOM_MATERIAL})
target_link_libraries(raytracer_material_custom PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_EMISSIVE "src/plugins/materials/EmissiveMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_emissive SHARED ${SOURCES_EMISSIVE})
target_link_libraries(raytracer_material_emissive PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_FLAT "src/plugins/materials/FlatColorMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_flat SHARED ${SOURCES_FLAT})
target_link_libraries(raytracer_material_flat PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_METALLIC "src/plugins/materials/MetallicMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_metallic SHARED ${SOURCES_METALLIC})
target_link_libraries(raytracer_material_metallic PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_DIFFUSE "src/plugins/materials/DiffuseMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_diffuse SHARED ${SOURCES_DIFFUSE})
target_link_libraries(raytracer_material_diffuse PRIVATE raytracer_core)

# Transformations compile the concrete primitives they manipulate, since
# primitives are plugins themselves.
file(GLOB_RECURSE SOURCES_TRANSLATION "src/plugins/transformations/Translation.cpp")
add_library(raytracer_transformation_translation SHARED
    ${SOURCES_TRANSLATION}
    src/plugins/primitives/Sphere.cpp
    src/plugins/primitives/Plane.cpp
    src/plugins/primitives/Cylinder.cpp
    src/plugins/primitives/Cone.cpp
)
target_link_libraries(raytracer_transformation_translation PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_ROTATION "src/plugins/transformations/Rotation.cpp")
add_library(raytracer_transformation_rotation SHARED
    ${SOURCES_ROTATION}
    src/plugins/primitives/Sphere.cpp
    src/plugins/primitives/Plane.cpp
    src/plugins/primitives/Cylinder.cpp
    src/plugins/primitives/Cone.cpp
)
target_link_libraries(raytracer_transformation_rotation PRIVATE raytracer_core)

# Custom targets
add_custom_target(core DEPENDS raytracer)

add_custom_target(cli DEPENDS raytracer_cli)

add_custom_target(gui DEPENDS raytracer_gui)

add_custom_target(renderers DEPENDS raytracer_renderer_cpu)
if (TARGET raytracer_renderer_cuda)
    add_dependencies(renderers raytracer_renderer_cuda)
endif()

add_custom_target(lights DEPENDS raytracer_light_ambient raytracer_light_directional)

add_custom_target(materials DEPENDS raytracer_material_flat)

add_custom_target(primitives DEPENDS raytracer_plane raytracer_sphere raytracer_cylinder raytracer_cone)

add_custom_target(transformations DEPENDS raytracer_transformation_translation raytracer_transformation_rotation)

# Set output directories for binaries and libraries
set_target_properties(raytracer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/.."
)

set_target_properties(
    raytracer_cli
    raytracer_gui
    raytracer_light_ambient
    raytracer_light_directional
    raytracer_light_point
    raytracer_plane
    raytracer_sphere
    raytracer_cylinder
    raytracer_cone
    raytracer_material_custom
    raytracer_material_emissive
    raytracer_material_flat
    raytracer_material_metallic
    raytracer_material_diffuse
    raytracer_transformation_translation
    raytracer_transformation_rotation
    raytracer_renderer_cpu
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../plugins"
        PREFIX ""
)
