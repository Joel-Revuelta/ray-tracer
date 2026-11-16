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
    $<$<COMPILE_LANGUAGE:CXX>:-W>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Werror>
    $<$<COMPILE_LANGUAGE:CXX>:-g3>
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # The plugin ABI relies on one RTTI/typeinfo definition across DSOs on GCC.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-gnu-unique>)
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Plugin entry points intentionally use C linkage with shared_ptr returns;
    # the same ABI is used by GCC and is exercised by the loader. Keep this
    # after -Wall/-Wextra, which re-enable the diagnostic in Clang.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-return-type-c-linkage>)
endif()

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
include_directories(third_party/stb)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIG++ REQUIRED libconfig++)
include_directories(${LIBCONFIG++_INCLUDE_DIRS})
find_package(Threads REQUIRED)

option(RAYTRACER_ENABLE_GUI "Build the SFML GUI plugin when SFML 3 is available" ON)
option(RAYTRACER_ENABLE_OPENGL_GUI "Build the GLFW/OpenGL GUI plugin" ON)
option(RAYTRACER_IN_SOURCE_OUTPUT
    "Write the executable and plugins to the repository root instead of the build tree" ON)

# Keep the historical in-source layout by default so ./raytracer and ./plugins
# keep working. CI and multi-build workflows can switch to an isolated layout.
if (RAYTRACER_IN_SOURCE_OUTPUT)
    set(RAYTRACER_RUNTIME_DIR "${CMAKE_BINARY_DIR}/..")
    set(RAYTRACER_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/../plugins")
else()
    set(RAYTRACER_RUNTIME_DIR "${CMAKE_BINARY_DIR}")
    set(RAYTRACER_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins")
endif()
if (RAYTRACER_ENABLE_GUI)
    find_package(SFML 3.0 COMPONENTS Graphics Window System QUIET)
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

# Small C++ regression executable for invariants that are hard to assert from
# the CLI (plugin ABI coefficients, per-hit material overrides, texture decode).
add_executable(raytracer_regressions tests/renderer_regressions.cpp)
target_link_libraries(raytracer_regressions PRIVATE raytracer_core)

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
            LIBRARY_OUTPUT_DIRECTORY "${RAYTRACER_PLUGIN_OUTPUT_DIR}"
            PREFIX ""
        )
        message(STATUS "CUDA renderer enabled (CUDA ${CUDAToolkit_VERSION}, architectures ${RAYTRACER_CUDA_ARCH})")
    else()
        message(STATUS "CUDA renderer disabled: CUDA toolkit not found")
    endif()
endif()

# GUI plugin (optional)
if (RAYTRACER_ENABLE_GUI AND SFML_FOUND)
    file(GLOB_RECURSE SOURCES_GUI CONFIGURE_DEPENDS "src/plugins/GUI/SFML.cpp")
    add_library(raytracer_gui SHARED ${SOURCES_GUI})
    target_link_libraries(raytracer_gui PRIVATE raytracer_core SFML::Graphics SFML::Window SFML::System)
    set_target_properties(raytracer_gui PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${RAYTRACER_PLUGIN_OUTPUT_DIR}"
        PREFIX ""
    )
elseif (RAYTRACER_ENABLE_GUI)
    message(STATUS "SFML GUI disabled: install SFML 3.0.x to enable it")
endif()

# OpenGL GUI plugin (GLFW + GLEW)
if (RAYTRACER_ENABLE_OPENGL_GUI AND OpenGL_FOUND AND glfw3_FOUND AND GLEW_FOUND)
    add_library(raytracer_gui_opengl SHARED src/plugins/GUI/OpenGL.cpp)
    target_link_libraries(raytracer_gui_opengl PRIVATE raytracer_core glfw GLEW::GLEW OpenGL::GL)
    set_target_properties(raytracer_gui_opengl PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${RAYTRACER_PLUGIN_OUTPUT_DIR}"
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

file(GLOB_RECURSE SOURCES_SPHERE_LIGHT CONFIGURE_DEPENDS "src/plugins/lights/SphereLight.cpp")
add_library(raytracer_light_sphere SHARED ${SOURCES_SPHERE_LIGHT})
target_link_libraries(raytracer_light_sphere PRIVATE raytracer_core)

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

file(GLOB_RECURSE SOURCES_MESH CONFIGURE_DEPENDS "src/plugins/primitives/MeshPrimitive.cpp")
file(GLOB_RECURSE EXTERN_MESH CONFIGURE_DEPENDS "src/plugins/primitives/ExternMesh.cpp")
add_library(raytracer_mesh SHARED ${SOURCES_MESH} ${EXTERN_MESH})
target_link_libraries(raytracer_mesh PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_BOX CONFIGURE_DEPENDS "src/plugins/primitives/Box.cpp")
add_library(raytracer_box SHARED ${SOURCES_BOX})
target_link_libraries(raytracer_box PRIVATE raytracer_core)

file(GLOB_RECURSE SOURCES_DISK CONFIGURE_DEPENDS "src/plugins/primitives/Disk.cpp")
add_library(raytracer_disk SHARED ${SOURCES_DISK})
target_link_libraries(raytracer_disk PRIVATE raytracer_core)

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

file(GLOB_RECURSE SOURCES_PHONG CONFIGURE_DEPENDS "src/plugins/materials/PhongMaterial.cpp")
add_library(raytracer_material_phong SHARED ${SOURCES_PHONG})
target_link_libraries(raytracer_material_phong PRIVATE raytracer_core)

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

add_custom_target(lights DEPENDS
    raytracer_light_ambient
    raytracer_light_directional
    raytracer_light_point
    raytracer_light_sphere
)

add_custom_target(materials DEPENDS
    raytracer_material_phong
    raytracer_material_custom
    raytracer_material_emissive
    raytracer_material_flat
    raytracer_material_metallic
    raytracer_material_dielectric
    raytracer_material_diffuse
)

add_custom_target(primitives DEPENDS
    raytracer_plane
    raytracer_sphere
    raytracer_cylinder
    raytracer_cone
    raytracer_mesh
    raytracer_box
    raytracer_disk
)

add_custom_target(transformations DEPENDS raytracer_transformation_translation raytracer_transformation_rotation)

# Set output directories for binaries and libraries
set_target_properties(raytracer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${RAYTRACER_RUNTIME_DIR}"
)

set_target_properties(
    raytracer_light_ambient
    raytracer_light_directional
    raytracer_light_point
    raytracer_light_sphere
    raytracer_plane
    raytracer_sphere
    raytracer_cylinder
    raytracer_cone
    raytracer_mesh
    raytracer_box
    raytracer_disk
    raytracer_material_phong
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
        LIBRARY_OUTPUT_DIRECTORY "${RAYTRACER_PLUGIN_OUTPUT_DIR}"
        PREFIX ""
)

# Basic smoke tests. Run them from the repository root because plugin paths are
# resolved relative to the working directory.
enable_testing()
add_test(
    NAME renderer_regressions
    COMMAND $<TARGET_FILE:raytracer_regressions> ${CMAKE_SOURCE_DIR}/scenes/mtl_materials.cfg
            ${CMAKE_SOURCE_DIR}/scenes/textures/nebula_equirect.hdr ${CMAKE_SOURCE_DIR}/scenes/mtl_maps.cfg
)
set_tests_properties(renderer_regressions PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    ENVIRONMENT "RAYTRACER_PLUGIN_DIR=${RAYTRACER_PLUGIN_OUTPUT_DIR}"
)
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
add_test(
    NAME seed_deterministic_cpu_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_render.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} 12345
)
add_test(
    NAME threads_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/showcase.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2
            --threads 2 --seed 7 --output ${CMAKE_BINARY_DIR}/threads_smoke.ppm
)
add_test(NAME list_guis COMMAND $<TARGET_FILE:raytracer> --list-guis)
add_test(
    NAME bvh_matches_linear
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bvh_parity.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME transform_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/transforms.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/transform_smoke.ppm
)
add_test(
    NAME parser_rejects_invalid_transform
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_transform.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_transform.ppm
)
add_test(
    NAME mesh_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/meshes.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/mesh_smoke.ppm
)
add_test(
    NAME mesh_bvh_matches_linear
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/mesh_parity.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME mesh_smooth_normals
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/smooth_mesh.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2
            --output ${CMAKE_BINARY_DIR}/smooth_mesh.ppm
)
add_test(
    NAME mesh_flat_fallback
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/flat_mesh.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2
            --output ${CMAKE_BINARY_DIR}/flat_mesh.ppm
)
add_test(
    NAME parser_rejects_invalid_obj
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_obj.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_obj.ppm
)
add_test(
    NAME dof_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/depth_of_field.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/dof_smoke.ppm
)
add_test(
    NAME deterministic_dof_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_dof.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME dof_cli_override
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/depth_of_field.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2
            --lens-radius 0.2 --focus-distance 9.0
            --output ${CMAKE_BINARY_DIR}/dof_cli_override.ppm
)
add_test(
    NAME parser_rejects_invalid_lens
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_lens.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_lens.ppm
)
add_test(
    NAME parser_rejects_invalid_focus
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_focus.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_focus.ppm
)
add_test(
    NAME soft_shadow_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/soft_shadows.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/soft_shadow_smoke.ppm
)
add_test(
    NAME deterministic_soft_shadow_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_soft_shadow.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME parser_rejects_invalid_area_light
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_area_light.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_area_light.ppm
)
add_test(
    NAME texture_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/textures.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/texture_smoke.ppm
)
add_test(
    NAME deterministic_texture_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_texture.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME parser_rejects_invalid_texture
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_texture.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_texture.ppm
)
add_test(
    NAME obj_materials_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/obj_materials.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/obj_materials_smoke.ppm
)
add_test(
    NAME deterministic_obj_materials_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_obj_materials.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME parser_rejects_invalid_material_group
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_material_group.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_material_group.ppm
)
add_test(
    NAME environment_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/environment.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/environment_smoke.ppm
)
add_test(
    NAME deterministic_environment_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_environment.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME parser_rejects_invalid_environment
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_environment.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_environment.ppm
)
add_test(
    NAME parser_rejects_float_overflow
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_overflow.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_overflow.ppm
)
add_test(
    NAME parser_rejects_uv_overflow
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_uv.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_uv.ppm
)
add_test(
    NAME mtl_materials_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/mtl_materials.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/mtl_materials_smoke.ppm
)
add_test(
    NAME mtl_maps_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/mtl_maps.cfg
            --cli --renderer cpu --width 160 --height 90 --samples 8 --seed 5
            --output ${CMAKE_BINARY_DIR}/mtl_maps_smoke.ppm
)
add_test(
    NAME misty_shrine_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/misty_shrine.cfg
            --cli --renderer cpu --width 160 --height 90 --samples 8 --seed 5
            --output ${CMAKE_BINARY_DIR}/misty_shrine_smoke.ppm
)
add_test(
    NAME volumetric_cathedral_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/volumetric_cathedral.cfg
            --cli --renderer cpu --width 160 --height 90 --samples 8 --seed 5
            --output ${CMAKE_BINARY_DIR}/volumetric_cathedral_smoke.ppm
)
add_test(
    NAME lagoon_mist_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/lagoon_mist.cfg
            --cli --renderer cpu --width 160 --height 90 --samples 8 --seed 5
            --output ${CMAKE_BINARY_DIR}/lagoon_mist_smoke.ppm
)
add_test(
    NAME motion_blur_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/motion_blur.cfg
            --cli --renderer cpu --width 160 --height 90 --samples 8 --seed 5
            --animation --start-frame 24 --end-frame 24
            --output ${CMAKE_BINARY_DIR}/motion_blur_smoke-%02d.ppm
)
add_test(
    NAME deterministic_mtl_materials_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_mtl_materials.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME parser_rejects_invalid_atmosphere_density
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_atmosphere_density.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_atmosphere_density.ppm
)
add_test(
    NAME parser_rejects_invalid_atmosphere_anisotropy
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_atmosphere_anisotropy.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_atmosphere_anisotropy.ppm
)
add_test(
    NAME parser_rejects_invalid_atmosphere_type
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_atmosphere_type.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_atmosphere_type.ppm
)
add_test(
    NAME parser_rejects_invalid_height_fog
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_height_fog.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_height_fog.ppm
)
add_test(
    NAME parser_rejects_invalid_atmosphere_distance
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_atmosphere_distance.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_atmosphere_distance.ppm
)
add_test(
    NAME parser_rejects_invalid_mtl
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_mtl.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_mtl.ppm
)
add_test(
    NAME group_transform_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/group_transform.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/group_transform_smoke.ppm
)
add_test(
    NAME prefab_instances_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/prefabs.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/prefab_instances_smoke.ppm
)
add_test(
    NAME deterministic_prefabs_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_prefabs.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME parser_rejects_unknown_prefab
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_unknown_prefab.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_unknown_prefab.ppm
)
add_test(
    NAME parser_rejects_prefab_instances_in_prefabs
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_prefab_recursion.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_prefab_recursion.ppm
)
add_test(
    NAME sakura_shrine_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/sakura_shrine.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/sakura_shrine_smoke.ppm
)
add_test(
    NAME brass_orrery_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/brass_orrery_smoke.ppm
)
add_test(
    NAME tidal_bloom_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/tidal_bloom.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/tidal_bloom_smoke.ppm
)
add_test(
    NAME obsidian_aurora_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/obsidian_aurora.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/obsidian_aurora_smoke.ppm
)
add_test(
    NAME frozen_cathedral_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/frozen_cathedral.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --output ${CMAKE_BINARY_DIR}/frozen_cathedral_smoke.ppm
)
add_test(
    NAME deterministic_scene_pack_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_scene_pack.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME bloom_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/tidal_bloom.cfg
            --cli --renderer cpu --width 32 --height 18 --frames 2 --seed 5
            --tonemap aces --bloom --bloom-intensity 0.45
            --output ${CMAKE_BINARY_DIR}/bloom_smoke.ppm
)
add_test(
    NAME deterministic_bloom_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_bloom.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME animation_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 32 --height 18 --samples 1
            --animation --start-frame 0 --end-frame 1
            --output ${CMAKE_BINARY_DIR}/animation_smoke-%02d.ppm
)
add_test(
    NAME implicit_animation_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/sakura_shrine.cfg
            --cli --renderer cpu --width 32 --height 18 --samples 1
            --start-frame 0 --end-frame 1
            --output ${CMAKE_BINARY_DIR}/implicit_animation_smoke-%02d.ppm
)
add_test(
    NAME deterministic_animation_render
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_animation.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME png_output_smoke
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/png_output.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME parser_rejects_invalid_animation_order
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_animation_order.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_animation_order.ppm
)
add_test(
    NAME parser_rejects_invalid_animation_fov
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_animation_fov.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_animation_fov.ppm
)
add_test(
    NAME cli_rejects_missing_animation
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/showcase.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1 --animation
)
add_test(
    NAME cli_rejects_invalid_animation_range
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --animation --start-frame 0 --end-frame 999
)
add_test(
    NAME parser_rejects_invalid_animation_start
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_animation_start.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_animation_start.ppm
)
add_test(
    NAME parser_rejects_invalid_animation_single
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_animation_single.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_animation_single.ppm
)
add_test(
    NAME parser_rejects_invalid_animation_fps
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_animation_fps.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_animation_fps.ppm
)
add_test(
    NAME parser_rejects_invalid_animation_pose
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_animation_pose.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_animation_pose.ppm
)
add_test(
    NAME parser_rejects_invalid_animation_missing_camera
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_animation_missing_camera.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_animation_missing_camera.ppm
)
add_test(
    NAME cli_rejects_invalid_animation_fps
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --fps 0
)
add_test(
    NAME cli_rejects_animation_start_after_end
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --animation --start-frame 5 --end-frame 2
)
add_test(
    NAME cli_rejects_animation_bad_output_pattern
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --animation --end-frame 0
            --output ${CMAKE_BINARY_DIR}/invalid-animation-%s.png
)
add_test(
    NAME cli_rejects_animation_missing_output_dir
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --animation --end-frame 0
            --output /nonexistent/raytracer-animation-%02d.ppm
)
add_test(
    NAME cpu_cuda_parity
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/cpu_cuda_parity.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
set_tests_properties(cpu_cuda_parity PROPERTIES
    SKIP_RETURN_CODE 77
    LABELS "parity"
    WORKING_DIRECTORY ${RAYTRACER_RUNTIME_DIR}
)
add_test(
    NAME animation_shutter_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/animation_shutter.cfg
            --cli --renderer cpu --width 32 --height 18 --samples 1
            --animation --start-frame 12 --end-frame 12
            --output ${CMAKE_BINARY_DIR}/animation_shutter_smoke-%02d.ppm
)
add_test(
    NAME scene_animation_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/scene_animation.cfg
            --cli --renderer cpu --width 64 --height 36 --samples 2
            --animation --start-frame 0 --end-frame 1
            --output ${CMAKE_BINARY_DIR}/scene_animation_smoke-%02d.ppm
)
add_test(
    NAME deterministic_scene_animation
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_scene_animation.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME material_maps_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/material_maps.cfg
            --cli --renderer cpu --width 160 --height 90 --samples 8 --seed 5
            --output ${CMAKE_BINARY_DIR}/material_maps_smoke.png
)
add_test(
    NAME primitives_extra_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/primitives_extra.cfg
            --cli --renderer cpu --width 160 --height 90 --samples 8 --seed 5
            --output ${CMAKE_BINARY_DIR}/primitives_extra_smoke.png
)
add_test(
    NAME scatter_field_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/scatter_field.cfg
            --cli --renderer cpu --width 160 --height 90 --samples 8 --seed 5
            --output ${CMAKE_BINARY_DIR}/scatter_field_smoke.png
)
add_test(
    NAME parser_rejects_invalid_box
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_box.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_box.ppm
)
add_test(
    NAME parser_rejects_invalid_disk
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_disk.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_disk.ppm
)
add_test(
    NAME parser_rejects_invalid_scatter
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_scatter.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_scatter.ppm
)
add_test(
    NAME golden_images
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/golden_images.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
set_tests_properties(golden_images PROPERTIES LABELS "golden;scene")
add_test(
    NAME atmosphere_regressions
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/atmosphere_regressions.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
set_tests_properties(atmosphere_regressions PROPERTIES LABELS "scene;atmosphere")
add_test(
    NAME malformed_scene_corpus
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/malformed_scene_corpus.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
set_tests_properties(malformed_scene_corpus PROPERTIES LABELS "negative;parser")
add_test(
    NAME benchmark_report
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/benchmark_report.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
set_tests_properties(benchmark_report PROPERTIES LABELS "benchmark")
add_test(
    NAME parser_rejects_invalid_scene_animation_target
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_scene_animation_target.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_scene_animation_target.ppm
)
add_test(
    NAME parser_rejects_invalid_scene_animation_times
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_scene_animation_times.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_scene_animation_times.ppm
)
add_test(
    NAME parser_rejects_invalid_scene_animation_empty
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_scene_animation_empty.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_scene_animation_empty.ppm
)
add_test(
    NAME parser_rejects_invalid_scene_animation_duplicate
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_scene_animation_duplicate.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_scene_animation_duplicate.ppm
)
add_test(
    NAME motion_blur_bvh
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/motion_blur_bvh.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
set_tests_properties(motion_blur_bvh PROPERTIES LABELS "parity;scene")
add_test(
    NAME deterministic_motion_blur
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/deterministic_motion_blur.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
add_test(
    NAME parser_rejects_invalid_animation_shutter_angle
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_animation_shutter_angle.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_animation_shutter_angle.ppm
)
add_test(
    NAME parser_rejects_invalid_animation_shutter_samples
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/tests/invalid_animation_shutter_samples.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --output ${CMAKE_BINARY_DIR}/invalid_animation_shutter_samples.ppm
)
add_test(
    NAME cli_rejects_invalid_shutter_angle
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --shutter-angle 360
)
add_test(
    NAME cli_rejects_invalid_shutter_samples
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --shutter-samples 0
)
add_test(
    NAME validate_still_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/showcase.cfg
            --cli --renderer cpu --width 320 --height 180 --samples 8
            --output ${CMAKE_BINARY_DIR}/validate_still.png --validate
)
add_test(
    NAME validate_animation_smoke
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --renderer cpu --width 320 --height 180 --samples 8 --animation
            --shutter-angle 180 --shutter-samples 4
            --output ${CMAKE_BINARY_DIR}/validate_animation-%04d.png --validate
)
add_test(
    NAME cli_rejects_validate_bad_pattern
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --animation --output ${CMAKE_BINARY_DIR}/validate-%s.png --validate
)
add_test(
    NAME video_output_smoke
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/video_output.sh $<TARGET_FILE:raytracer>
            ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
)
set_tests_properties(video_output_smoke PROPERTIES
    SKIP_RETURN_CODE 77
    LABELS "video;output"
    WORKING_DIRECTORY ${RAYTRACER_RUNTIME_DIR}
)
add_test(
    NAME cli_rejects_resume_video
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --animation --video ${CMAKE_BINARY_DIR}/resume.mp4 --resume
)
add_test(
    NAME cli_rejects_output_video_extension
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --animation --output ${CMAKE_BINARY_DIR}/output.mp4
)
add_test(
    NAME cli_rejects_invalid_video_container
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --animation --video ${CMAKE_BINARY_DIR}/video.avi
)
add_test(
    NAME cli_rejects_invalid_video_codec
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --animation --video ${CMAKE_BINARY_DIR}/video.mp4 --video-codec vp8
)
add_test(
    NAME cli_rejects_invalid_video_crf
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --animation --video ${CMAKE_BINARY_DIR}/video.mp4 --video-crf 99
)
add_test(
    NAME cli_rejects_invalid_video_bitrate
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --animation --video ${CMAKE_BINARY_DIR}/video.mp4 --video-bitrate bananas
)
add_test(
    NAME cli_rejects_webm_h264
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --animation --video ${CMAKE_BINARY_DIR}/video.webm --video-codec h264
)
add_test(
    NAME cli_rejects_missing_ffmpeg
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/brass_orrery.cfg
            --cli --animation --end-frame 0 --video ${CMAKE_BINARY_DIR}/video.mp4 --validate
)
set_tests_properties(cli_rejects_missing_ffmpeg PROPERTIES
    ENVIRONMENT "RAYTRACER_FFMPEG=/nonexistent"
)
add_test(
    NAME cli_rejects_invalid_bloom_radius
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/showcase.cfg
            --cli --renderer cpu --width 16 --height 9 --frames 1
            --bloom --bloom-radius 17
)
add_test(
    NAME cli_rejects_invalid_threads
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/showcase.cfg
            --cli --renderer cpu --threads -1 --width 16 --height 9 --frames 1
)
add_test(
    NAME cli_rejects_invalid_seed
    COMMAND $<TARGET_FILE:raytracer> --path ${CMAKE_SOURCE_DIR}/scenes/showcase.cfg
            --cli --renderer cpu --seed -1 --width 16 --height 9 --frames 1
)
set_tests_properties(cli_cpu_smoke list_renderers deterministic_cpu_render post_process_smoke
    roughness_smoke seed_deterministic_cpu_render threads_smoke list_guis
    bvh_matches_linear transform_smoke mesh_smoke mesh_bvh_matches_linear
    mesh_smooth_normals mesh_flat_fallback
    dof_smoke deterministic_dof_render dof_cli_override
    soft_shadow_smoke deterministic_soft_shadow_render
    texture_smoke deterministic_texture_render
    obj_materials_smoke deterministic_obj_materials_render
    environment_smoke deterministic_environment_render
    mtl_materials_smoke deterministic_mtl_materials_render
    group_transform_smoke prefab_instances_smoke deterministic_prefabs_render
    sakura_shrine_smoke brass_orrery_smoke tidal_bloom_smoke obsidian_aurora_smoke
    frozen_cathedral_smoke deterministic_scene_pack_render
    bloom_smoke deterministic_bloom_render
    animation_smoke implicit_animation_smoke animation_shutter_smoke scene_animation_smoke
    deterministic_animation_render deterministic_motion_blur deterministic_scene_animation
    golden_images atmosphere_regressions malformed_scene_corpus benchmark_report
    material_maps_smoke mtl_maps_smoke primitives_extra_smoke scatter_field_smoke
    misty_shrine_smoke volumetric_cathedral_smoke lagoon_mist_smoke motion_blur_smoke
    validate_still_smoke validate_animation_smoke
    png_output_smoke
    PROPERTIES WORKING_DIRECTORY ${RAYTRACER_RUNTIME_DIR})
set_tests_properties(cli_rejects_invalid_threads cli_rejects_invalid_seed
    parser_rejects_invalid_transform parser_rejects_invalid_obj
    parser_rejects_invalid_lens parser_rejects_invalid_focus
    parser_rejects_invalid_area_light parser_rejects_invalid_texture
    parser_rejects_invalid_material_group parser_rejects_invalid_environment
    parser_rejects_invalid_mtl parser_rejects_unknown_prefab
    parser_rejects_prefab_instances_in_prefabs parser_rejects_float_overflow
    parser_rejects_uv_overflow cli_rejects_invalid_bloom_radius
    parser_rejects_invalid_animation_order parser_rejects_invalid_animation_fov
    cli_rejects_missing_animation cli_rejects_invalid_animation_range
    parser_rejects_invalid_animation_start parser_rejects_invalid_animation_single
    parser_rejects_invalid_animation_fps parser_rejects_invalid_animation_pose
    parser_rejects_invalid_animation_missing_camera
    cli_rejects_invalid_animation_fps cli_rejects_animation_start_after_end
    cli_rejects_animation_bad_output_pattern
    cli_rejects_animation_missing_output_dir
    parser_rejects_invalid_animation_shutter_angle
    parser_rejects_invalid_animation_shutter_samples
    cli_rejects_invalid_shutter_angle cli_rejects_invalid_shutter_samples
    cli_rejects_validate_bad_pattern
    cli_rejects_resume_video cli_rejects_output_video_extension
    cli_rejects_invalid_video_container cli_rejects_invalid_video_codec
    cli_rejects_invalid_video_crf cli_rejects_invalid_video_bitrate
    cli_rejects_webm_h264 cli_rejects_missing_ffmpeg
    parser_rejects_invalid_scene_animation_target
    parser_rejects_invalid_scene_animation_times
    parser_rejects_invalid_scene_animation_empty
    parser_rejects_invalid_scene_animation_duplicate
    parser_rejects_invalid_box parser_rejects_invalid_disk
    parser_rejects_invalid_scatter parser_rejects_invalid_atmosphere_density
    parser_rejects_invalid_atmosphere_anisotropy parser_rejects_invalid_atmosphere_type
    parser_rejects_invalid_height_fog parser_rejects_invalid_atmosphere_distance PROPERTIES
    WILL_FAIL TRUE
    WORKING_DIRECTORY ${RAYTRACER_RUNTIME_DIR}
)
set_tests_properties(parser_rejects_invalid_roughness PROPERTIES
    WILL_FAIL TRUE
    WORKING_DIRECTORY ${RAYTRACER_RUNTIME_DIR}
)
set_tests_properties(parser_rejects_negative_material PROPERTIES
    WILL_FAIL TRUE
    WORKING_DIRECTORY ${RAYTRACER_RUNTIME_DIR}
)

# Test labels keep local and CI runs selectable: ctest -L animation, -L parity,
# -L negative, and so on. A test may carry more than one label.
set_tests_properties(renderer_regressions PROPERTIES LABELS "unit")
set_tests_properties(
    cli_cpu_smoke list_renderers list_guis post_process_smoke roughness_smoke
    threads_smoke transform_smoke mesh_smoke mesh_smooth_normals mesh_flat_fallback
    dof_smoke dof_cli_override soft_shadow_smoke texture_smoke obj_materials_smoke
    environment_smoke mtl_materials_smoke group_transform_smoke prefab_instances_smoke
    sakura_shrine_smoke brass_orrery_smoke tidal_bloom_smoke obsidian_aurora_smoke
    frozen_cathedral_smoke bloom_smoke validate_still_smoke validate_animation_smoke
    material_maps_smoke mtl_maps_smoke primitives_extra_smoke scatter_field_smoke
    misty_shrine_smoke volumetric_cathedral_smoke lagoon_mist_smoke motion_blur_smoke
    PROPERTIES LABELS "smoke"
)
set_tests_properties(animation_smoke implicit_animation_smoke animation_shutter_smoke PROPERTIES
    LABELS "smoke;animation"
)
set_tests_properties(scene_animation_smoke PROPERTIES LABELS "smoke;animation;scene")
set_tests_properties(deterministic_motion_blur deterministic_scene_animation PROPERTIES
    LABELS "scene;animation"
)
set_tests_properties(png_output_smoke PROPERTIES LABELS "output;smoke")
set_tests_properties(
    deterministic_cpu_render seed_deterministic_cpu_render deterministic_dof_render
    deterministic_soft_shadow_render deterministic_texture_render
    deterministic_obj_materials_render deterministic_environment_render
    deterministic_mtl_materials_render deterministic_prefabs_render
    deterministic_scene_pack_render deterministic_bloom_render
    PROPERTIES LABELS "scene"
)
set_tests_properties(bvh_matches_linear mesh_bvh_matches_linear PROPERTIES
    LABELS "scene;parity"
)
set_tests_properties(deterministic_animation_render PROPERTIES LABELS "scene;animation")
set_tests_properties(
    parser_rejects_negative_material parser_rejects_invalid_roughness
    parser_rejects_invalid_transform parser_rejects_invalid_obj parser_rejects_invalid_lens
    parser_rejects_invalid_focus parser_rejects_invalid_area_light parser_rejects_invalid_texture
    parser_rejects_invalid_material_group parser_rejects_invalid_environment
    parser_rejects_float_overflow parser_rejects_uv_overflow parser_rejects_invalid_mtl
    parser_rejects_unknown_prefab parser_rejects_prefab_instances_in_prefabs
    cli_rejects_invalid_bloom_radius cli_rejects_invalid_threads cli_rejects_invalid_seed
    PROPERTIES LABELS "negative"
)
set_tests_properties(
    parser_rejects_invalid_animation_order parser_rejects_invalid_animation_fov
    cli_rejects_missing_animation cli_rejects_invalid_animation_range
    parser_rejects_invalid_animation_start parser_rejects_invalid_animation_single
    parser_rejects_invalid_animation_fps parser_rejects_invalid_animation_pose
    parser_rejects_invalid_animation_missing_camera cli_rejects_invalid_animation_fps
    cli_rejects_animation_start_after_end cli_rejects_animation_bad_output_pattern
    cli_rejects_animation_missing_output_dir
    parser_rejects_invalid_animation_shutter_angle
    parser_rejects_invalid_animation_shutter_samples
    cli_rejects_invalid_shutter_angle cli_rejects_invalid_shutter_samples
    cli_rejects_validate_bad_pattern
    parser_rejects_invalid_scene_animation_target
    parser_rejects_invalid_scene_animation_times
    parser_rejects_invalid_scene_animation_empty
    parser_rejects_invalid_scene_animation_duplicate
    PROPERTIES LABELS "negative;animation"
)
set_tests_properties(
    parser_rejects_invalid_box parser_rejects_invalid_disk parser_rejects_invalid_scatter
    parser_rejects_invalid_atmosphere_density parser_rejects_invalid_atmosphere_anisotropy
    parser_rejects_invalid_atmosphere_type parser_rejects_invalid_height_fog
    parser_rejects_invalid_atmosphere_distance
    PROPERTIES LABELS "negative;scene"
)
set_tests_properties(
    cli_rejects_resume_video cli_rejects_output_video_extension
    cli_rejects_invalid_video_container cli_rejects_invalid_video_codec
    cli_rejects_invalid_video_crf cli_rejects_invalid_video_bitrate
    cli_rejects_webm_h264 cli_rejects_missing_ffmpeg
    PROPERTIES LABELS "negative;video"
)

# Install a self-contained bundle: executable and core in bin/, plugins in
# bin/plugins/ (or lib/raytracer/plugins when RAYTRACER_PLUGIN_DIR is used).
set(RAYTRACER_PLUGIN_TARGETS
    raytracer_light_ambient
    raytracer_light_directional
    raytracer_light_point
    raytracer_light_sphere
    raytracer_plane
    raytracer_sphere
    raytracer_cylinder
    raytracer_cone
    raytracer_mesh
    raytracer_box
    raytracer_disk
    raytracer_material_phong
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
