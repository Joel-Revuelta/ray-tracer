cmake_minimum_required(VERSION 3.20)
project(raytracer)

include(GNUInstallDirs)

# Optimised builds are the sensible default for a CPU/GPU renderer. Users can
# still request Debug/RelWithDebInfo explicitly.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

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

option(RAYTRACER_ENABLE_GUI "Build the SFML GUI plugin" ON)
option(RAYTRACER_ENABLE_OPENGL_GUI "Build the GLFW/OpenGL GUI plugin" ON)
if (RAYTRACER_ENABLE_GUI)
    find_package(SFML 3.0 COMPONENTS Graphics Window System REQUIRED)
endif()
if (RAYTRACER_ENABLE_OPENGL_GUI)
    find_package(OpenGL QUIET)
    find_package(glfw3 3.3 QUIET)
    find_package(GLEW QUIET)
endif()

# Shared core runtime used by the executable and every plugin.
file(GLOB_RECURSE SOURCES_UTILS CONFIGURE_DEPENDS "src/utils/*.cpp")
file(GLOB_RECURSE SOURCES_SCENE CONFIGURE_DEPENDS "src/scene/*.cpp")
file(GLOB_RECURSE SOURCES_DLMANAGER CONFIGURE_DEPENDS "src/DLManager/*.cpp")
file(GLOB_RECURSE SOURCES_FACTORY CONFIGURE_DEPENDS "src/factory/*.cpp")
file(GLOB_RECURSE SOURCES_PARSER CONFIGURE_DEPENDS "src/plugins/parser/*.cpp")
file(GLOB_RECURSE SOURCES_RENDER_COMMON CONFIGURE_DEPENDS "src/render/*.cpp")

add_library(raytracer_core SHARED
    ${SOURCES_UTILS}
    ${SOURCES_SCENE}
    ${SOURCES_DLMANAGER}
    ${SOURCES_FACTORY}
    ${SOURCES_PARSER}
    ${SOURCES_RENDER_COMMON}
    src/Camera.cpp
    src/plugins/materials/AMaterial.cpp
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
set(RAYTRACER_CUDA_ARCH "89" CACHE STRING
    "CUDA architectures to compile for (for example 86, 89, or 86;89)")
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
        string(REGEX MATCH "[0-9]+" RAYTRACER_CUDA_MIN_CC "${RAYTRACER_CUDA_ARCH}")
        if (RAYTRACER_CUDA_MIN_CC STREQUAL "")
            message(FATAL_ERROR "RAYTRACER_CUDA_ARCH must start with a numeric compute capability")
        endif()
        target_compile_definitions(raytracer_renderer_cuda PRIVATE
            RAYTRACER_CUDA_MIN_CC=${RAYTRACER_CUDA_MIN_CC}
        )
        set_target_properties(raytracer_renderer_cuda PROPERTIES
            CUDA_STANDARD 20
            CUDA_STANDARD_REQUIRED ON
            CUDA_ARCHITECTURES "${RAYTRACER_CUDA_ARCH}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../plugins"
            PREFIX ""
        )
        message(STATUS "CUDA renderer enabled (CUDA ${CUDAToolkit_VERSION}, architectures ${RAYTRACER_CUDA_ARCH})")
    else()
        message(STATUS "CUDA renderer disabled: CUDA toolkit not found")
    endif()
endif()

# GUI plugin (optional)
if (RAYTRACER_ENABLE_GUI)
    file(GLOB_RECURSE SOURCES_GUI CONFIGURE_DEPENDS "src/plugins/GUI/SFML.cpp")
    add_library(raytracer_gui SHARED ${SOURCES_GUI})
    target_link_libraries(raytracer_gui PRIVATE raytracer_core SFML::Graphics SFML::Window SFML::System)
    set_target_properties(raytracer_gui PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../plugins"
        PREFIX ""
    )
endif()

# OpenGL GUI plugin (GLFW + GLEW)
if (RAYTRACER_ENABLE_OPENGL_GUI AND OpenGL_FOUND AND glfw3_FOUND AND GLEW_FOUND)
    add_library(raytracer_gui_opengl SHARED src/plugins/GUI/OpenGL.cpp)
    target_link_libraries(raytracer_gui_opengl PRIVATE raytracer_core glfw GLEW::GLEW OpenGL::GL)
    set_target_properties(raytracer_gui_opengl PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../plugins"
        PREFIX ""
    )
    message(STATUS "OpenGL GUI enabled (GLFW ${glfw3_VERSION}, GLEW ${GLEW_VERSION})")
elseif (RAYTRACER_ENABLE_OPENGL_GUI)
    message(STATUS "OpenGL GUI disabled: install libglfw3-dev and libglew-dev to enable it")
endif()

# Lights
file(GLOB_RECURSE SOURCES_AMBIENT CONFIGURE_DEPENDS "src/plugins/lights/AmbientLight.cpp")
add_library(raytracer_light_ambient SHARED ${SOURCES_AMBIENT})
target_link_libraries(raytracer_light_ambient PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_DIRECTIONAL CONFIGURE_DEPENDS "src/plugins/lights/DirectionalLight.cpp")
add_library(raytracer_light_directional SHARED ${SOURCES_DIRECTIONAL})
target_link_libraries(raytracer_light_directional PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_POINT CONFIGURE_DEPENDS "src/plugins/lights/PointLight.cpp")
add_library(raytracer_light_point SHARED ${SOURCES_POINT})
target_link_libraries(raytracer_light_point PRIVATE raytracer_core)

# Primitives
file(GLOB_RECURSE SOURCES_PLANE CONFIGURE_DEPENDS "src/plugins/primitives/Plane.cpp")
file(GLOB_RECURSE EXTERN_PLANE CONFIGURE_DEPENDS "src/plugins/primitives/ExternPlane.cpp")
add_library(raytracer_plane SHARED ${SOURCES_PLANE} ${EXTERN_PLANE})
target_link_libraries(raytracer_plane PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_SPHERE CONFIGURE_DEPENDS "src/plugins/primitives/Sphere.cpp")
file(GLOB_RECURSE EXTERN_SPHERE CONFIGURE_DEPENDS "src/plugins/primitives/ExternSphere.cpp")
add_library(raytracer_sphere SHARED ${SOURCES_SPHERE} ${EXTERN_SPHERE})
target_link_libraries(raytracer_sphere PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_CYLINDER CONFIGURE_DEPENDS "src/plugins/primitives/Cylinder.cpp")
file(GLOB_RECURSE EXTERN_CYLINDER CONFIGURE_DEPENDS "src/plugins/primitives/ExternCylinder.cpp")
add_library(raytracer_cylinder SHARED ${SOURCES_CYLINDER} ${EXTERN_CYLINDER})
target_link_libraries(raytracer_cylinder PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_CONE CONFIGURE_DEPENDS "src/plugins/primitives/Cone.cpp")
file(GLOB_RECURSE EXTERN_CONE CONFIGURE_DEPENDS "src/plugins/primitives/ExternCone.cpp")
add_library(raytracer_cone SHARED ${SOURCES_CONE} ${EXTERN_CONE})
target_link_libraries(raytracer_cone PRIVATE raytracer_core)

# Materials
file(GLOB_RECURSE SOURCES_CUSTOM_MATERIAL CONFIGURE_DEPENDS "src/plugins/materials/CustomMaterial.cpp")
add_library(raytracer_material_custom SHARED ${SOURCES_CUSTOM_MATERIAL})
target_link_libraries(raytracer_material_custom PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_EMISSIVE CONFIGURE_DEPENDS "src/plugins/materials/EmissiveMaterial.cpp")
add_library(raytracer_material_emissive SHARED ${SOURCES_EMISSIVE})
target_link_libraries(raytracer_material_emissive PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_FLAT CONFIGURE_DEPENDS "src/plugins/materials/FlatColorMaterial.cpp")
add_library(raytracer_material_flat SHARED ${SOURCES_FLAT})
target_link_libraries(raytracer_material_flat PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_METALLIC CONFIGURE_DEPENDS "src/plugins/materials/MetallicMaterial.cpp")
add_library(raytracer_material_metallic SHARED ${SOURCES_METALLIC})
target_link_libraries(raytracer_material_metallic PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_DIELECTRIC CONFIGURE_DEPENDS "src/plugins/materials/DielectricMaterial.cpp")
add_library(raytracer_material_dielectric SHARED ${SOURCES_DIELECTRIC})
target_link_libraries(raytracer_material_dielectric PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_DIFFUSE CONFIGURE_DEPENDS "src/plugins/materials/DiffuseMaterial.cpp")
add_library(raytracer_material_diffuse SHARED ${SOURCES_DIFFUSE})
target_link_libraries(raytracer_material_diffuse PRIVATE raytracer_core)

# Transformations compile the concrete primitives they manipulate, since
# primitives are plugins themselves.
file(GLOB_RECURSE SOURCES_TRANSLATION CONFIGURE_DEPENDS "src/plugins/transformations/Translation.cpp")
add_library(raytracer_transformation_translation SHARED
    ${SOURCES_TRANSLATION}
    src/plugins/primitives/Sphere.cpp
    src/plugins/primitives/Plane.cpp
    src/plugins/primitives/Cylinder.cpp
    src/plugins/primitives/Cone.cpp
)
target_link_libraries(raytracer_transformation_translation PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_ROTATION CONFIGURE_DEPENDS "src/plugins/transformations/Rotation.cpp")
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

set(RAYTRACER_GUI_TARGETS)
if (TARGET raytracer_gui)
    list(APPEND RAYTRACER_GUI_TARGETS raytracer_gui)
endif()
if (TARGET raytracer_gui_opengl)
    list(APPEND RAYTRACER_GUI_TARGETS raytracer_gui_opengl)
endif()
if (RAYTRACER_GUI_TARGETS)
    add_custom_target(gui DEPENDS ${RAYTRACER_GUI_TARGETS})
endif()

add_custom_target(renderers DEPENDS raytracer_renderer_cpu)
if (TARGET raytracer_renderer_cuda)
    add_dependencies(renderers raytracer_renderer_cuda)
endif()

add_custom_target(lights DEPENDS raytracer_light_ambient raytracer_light_directional raytracer_light_point)

add_custom_target(materials DEPENDS
    raytracer_material_custom
    raytracer_material_emissive
    raytracer_material_flat
    raytracer_material_metallic
    raytracer_material_dielectric
    raytracer_material_diffuse
)

add_custom_target(primitives DEPENDS raytracer_plane raytracer_sphere raytracer_cylinder raytracer_cone)

add_custom_target(transformations DEPENDS raytracer_transformation_translation raytracer_transformation_rotation)

# Set output directories for binaries and libraries
set_target_properties(raytracer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/.."
)

set_target_properties(
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
    raytracer_material_dielectric
    raytracer_material_diffuse
    raytracer_transformation_translation
    raytracer_transformation_rotation
    raytracer_renderer_cpu
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../plugins"
        PREFIX ""
)

# Basic smoke tests. Run them from the repository root because plugin paths are
# resolved relative to the working directory.
enable_testing()
add_test(
    NAME cli_cpu_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/showcase.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 1
            --output ${CMAKE_BINARY_DIR}/cli_cpu_smoke.ppm
)
add_test(NAME list_renderers COMMAND $<TARGET_FILE:raytracer> --list-renderers)
add_test(
    NAME deterministic_cpu_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_render.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME parser_rejects_negative_material
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_material.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_material.ppm
)
add_test(
    NAME post_process_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/showcase.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2
            --tonemap reinhard --exposure 0.75
            --output ${CMAKE_BINARY_DIR}/post_process_smoke.ppm
)
add_test(
    NAME roughness_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/roughness_study.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2
            --output ${CMAKE_BINARY_DIR}/roughness_smoke.ppm
)
add_test(
    NAME parser_rejects_invalid_roughness
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_roughness.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_roughness.ppm
)
set_tests_properties(cli_cpu_smoke list_renderers deterministic_cpu_render post_process_smoke
    roughness_smoke PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/..)
set_tests_properties(parser_rejects_invalid_roughness PROPERTIES
    WILL_FAIL TRUE
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/..
)
set_tests_properties(parser_rejects_negative_material PROPERTIES
    WILL_FAIL TRUE
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/..
)

# Install a self-contained bundle: executable and core in bin/, plugins in
# bin/plugins/ (or lib/raytracer/plugins when RAYTRACER_PLUGIN_DIR is used).
set(RAYTRACER_PLUGIN_TARGETS
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
    raytracer_material_dielectric
    raytracer_material_diffuse
    raytracer_transformation_translation
    raytracer_transformation_rotation
    raytracer_renderer_cpu
)
if (TARGET raytracer_gui)
    list(APPEND RAYTRACER_PLUGIN_TARGETS raytracer_gui)
endif()
if (TARGET raytracer_gui_opengl)
    list(APPEND RAYTRACER_PLUGIN_TARGETS raytracer_gui_opengl)
endif()
if (TARGET raytracer_renderer_cuda)
    list(APPEND RAYTRACER_PLUGIN_TARGETS raytracer_renderer_cuda)
endif()

install(TARGETS raytracer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS raytracer_core LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS ${RAYTRACER_PLUGIN_TARGETS} LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins)
install(DIRECTORY scenes/ DESTINATION ${CMAKE_INSTALL_DATADIR}/raytracer/scenes)
set_target_properties(raytracer raytracer_core PROPERTIES INSTALL_RPATH "$ORIGIN")
set_target_properties(${RAYTRACER_PLUGIN_TARGETS} PROPERTIES INSTALL_RPATH "$ORIGIN/..")
