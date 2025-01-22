cmake_minimum_required(VERSION 3.16)
project(raytracer)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Compiler options
add_compile_options(-fno-gnu-unique -W -Wall -Wextra -Werror -g3)

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

# To add shared src file to libraries:
# file(GLOB_RECURSE SOURCES_NAME "path")
# Then add the sources to the desired add_library
# ...                                             ↓↓↓↓↓↓↓↓↓↓↓↓↓↓
# add_library(raytacer_cli SHARED ${SOUCRES_CLI} ${SOURCES_NAME})

# Utils source
file(GLOB_RECURSE SOURCES_UTILS "src/utils/*.cpp")

# Parser source
file(GLOB_RECURSE SOURCES_PARSER "src/plugins/parser/*.cpp")

# Factory source
file(GLOB_RECURSE SOURCES_FACTORY "src/factory/*.cpp")

# Core raytracer executable
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIG++ REQUIRED libconfig++)
find_package(Threads REQUIRED)
file(GLOB_RECURSE SOURCES_CORE "src/*.cpp")
list(FILTER SOURCES_CORE EXCLUDE REGEX "src/plugins/.*")
add_executable(raytracer ${SOURCES_CORE} ${SOURCES_PARSER} ${SOURCES_UTILS})
target_link_libraries(raytracer PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} ${LIBCONFIG++_LIBRARIES} stdc++)

# Shared Libraries

# CLI
file(GLOB_RECURSE SOURCES_CLI "src/plugins/CLI/*.cpp")
add_library(raytracer_cli SHARED ${SOURCES_CLI})

# GUI
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
file(GLOB_RECURSE SOURCES_GUI "src/plugins/GUI/*.cpp")
add_library(raytracer_gui SHARED ${SOURCES_GUI} ${SOURCES_UTILS})
target_link_libraries(raytracer_gui sfml-graphics sfml-window sfml-system sfml-audio)

# Lights
# AmbientLight
file(GLOB_RECURSE SOURCES_AMBIENT "src/plugins/lights/AmbientLight.cpp")
add_library(raytracer_light_ambient SHARED ${SOURCES_AMBIENT} ${SOURCES_UTILS})

# DirectionalLight
file(GLOB_RECURSE SOURCES_DIRECTIONAL "src/plugins/lights/DirectionalLight.cpp")
add_library(raytracer_light_directional SHARED ${SOURCES_DIRECTIONAL} ${SOURCES_UTILS})

# PointLight
file(GLOB_RECURSE SOURCES_POINT "src/plugins/lights/PointLight.cpp")
add_library(raytracer_light_point SHARED ${SOURCES_POINT} ${SOURCES_UTILS})

# Primitives
# Plane
file(GLOB_RECURSE SOURCES_PLANE "src/plugins/primitives/Plane.cpp")
file (GLOB_RECURSE EXTERN_PLANE "src/plugins/primitives/ExternPlane.cpp")
add_library(raytracer_plane SHARED ${SOURCES_PLANE} ${SOURCES_UTILS} ${EXTERN_PLANE})

# Sphere
file(GLOB_RECURSE SOURCES_SPHERE "src/plugins/primitives/Sphere.cpp")
file (GLOB_RECURSE EXTERN_SPHERE "src/plugins/primitives/ExternSphere.cpp")
add_library(raytracer_sphere SHARED ${SOURCES_SPHERE} ${SOURCES_UTILS} ${EXTERN_SPHERE})

#CyLinder
file(GLOB_RECURSE SOURCES_CYLINDER "src/plugins/primitives/Cylinder.cpp")
file (GLOB_RECURSE EXTERN_CYLINDER "src/plugins/primitives/ExternCylinder.cpp")
add_library(raytracer_cylinder SHARED ${SOURCES_CYLINDER} ${SOURCES_UTILS} ${EXTERN_CYLINDER})

#Cone
file(GLOB_RECURSE SOURCES_CONE "src/plugins/primitives/Cone.cpp")
file (GLOB_RECURSE EXTERN_CONE "src/plugins/primitives/ExternCone.cpp")
add_library(raytracer_cone SHARED ${SOURCES_CONE} ${SOURCES_UTILS} ${EXTERN_CONE})

# Materials
# Custom Material
file(GLOB_RECURSE SOURCES_CUSTOM_MATERIAL "src/plugins/materials/CustomMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_custom SHARED ${SOURCES_CUSTOM_MATERIAL} ${SOURCES_UTILS})

# Emissive
file(GLOB_RECURSE SOURCES_EMISSIVE "src/plugins/materials/EmissiveMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_emissive SHARED ${SOURCES_EMISSIVE} ${SOURCES_UTILS})

# Flat
file(GLOB_RECURSE SOURCES_FLAT "src/plugins/materials/FlatColorMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_flat SHARED ${SOURCES_FLAT} ${SOURCES_UTILS})

# Metallic
file(GLOB_RECURSE SOURCES_METALLIC "src/plugins/materials/MetallicMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_metallic SHARED ${SOURCES_METALLIC} ${SOURCES_UTILS})

# Diffuse
file(GLOB_RECURSE SOURCES_DIFFUSE "src/plugins/materials/DiffuseMaterial.cpp" "src/plugins/materials/AMaterial.cpp")
add_library(raytracer_material_diffuse SHARED ${SOURCES_DIFFUSE} ${SOURCES_UTILS})

# Parser
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIG++ REQUIRED libconfig++)
add_library(raytracer_parser SHARED ${SOURCES_PARSER} ${SOURCES_FACTORY})
target_link_libraries(raytracer_parser ${LIBCONFIG++_LIBRARIES} stdc++)

# Transformations
# Translation
file(GLOB_RECURSE SOURCES_TRANSLATION "src/plugins/transformations/Translation.cpp")
add_library(raytracer_transformation_translation SHARED ${SOURCES_TRANSLATION} ${SOURCES_UTILS} ${SOURCES_SPHERE} ${SOURCES_PLANE} ${SOURCES_CYLINDER} ${SOURCES_CORE})

# Rotation
file(GLOB_RECURSE SOURCES_ROTATION "src/plugins/transformations/Rotation.cpp")
add_library(raytracer_transformation_rotation SHARED ${SOURCES_ROTATION} ${SOURCES_UTILS} ${SOURCES_SPHERE} ${SOURCES_PLANE} ${SOURCES_CYLINDER} ${SOURCES_CORE})

# Custom target for core
add_custom_target(core
    DEPENDS raytracer
)

# Custom target for cli library
add_custom_target(cli
    DEPENDS raytracer_cli
)

# Custom target for gui libraries
add_custom_target(gui
    DEPENDS raytracer_gui
)

# Custom target for lights libraries
add_custom_target(lights
    DEPENDS raytracer_light_ambient raytracer_light_directional
)

# Custom target for material libraries
add_custom_target(materials
    DEPENDS raytracer_material_flat
)

# Custom target for primitives libraries
add_custom_target(primitives
    DEPENDS raytracer_plane raytracer_sphere raytracer_cylinder raytracer_cone
)

# Custom target for transformation libraries
add_custom_target(transformations
    DEPENDS raytracer_transformation_translation raytracer_transformation_rotation
)

# Set output directories for binaries and libraries
set_target_properties(raytracer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/.."
)

set_target_properties(raytracer_parser raytracer_cli raytracer_gui raytracer_light_ambient raytracer_light_directional raytracer_light_point raytracer_plane raytracer_sphere raytracer_cylinder raytracer_cone raytracer_plane raytracer_material_custom raytracer_material_emissive raytracer_material_flat raytracer_material_metallic raytracer_material_diffuse raytracer_transformation_translation raytracer_transformation_rotation PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../plugins"
    PREFIX "" # This line removes the "lib" prefix from the generated .so files
)
