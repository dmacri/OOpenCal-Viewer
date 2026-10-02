cmake_minimum_required(VERSION 3.16)
project(CustomModelPlugin VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Plugin configuration options
# ============================================================================

# These options allow you to build different plugin variants from the same source.
# You can override them in CMake GUI, on the command line, or in presets:
#   cmake -DPLUGIN_MODEL_NAME="\"FireModel\"" -DPLUGIN_CELL_CLASS=FireCell ..

set(PLUGIN_MODEL_NAME "\"Custom Model\"" CACHE STRING "Model name visible in the viewer (string literal)")
set(PLUGIN_CELL_CLASS CustomCell CACHE STRING "C++ class implementing the cell model")

# ============================================================================
# Configuration - Adjust these paths for your system
# ============================================================================

set(OOPENCALVIEWER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Path to OOpenCal-Viewer source")
set(OOPENCAL_DIR "${OOPENCALVIEWER_DIR}/.." CACHE PATH "Path to OOpenCAL directory")

# ============================================================================
# Find required packages
# ============================================================================

if(NOT VTK_FOUND)
    find_package(VTK COMPONENTS
        CommonColor
        CommonCore
        CommonDataModel
        FiltersSources
        InteractionStyle
        InteractionWidgets
        RenderingAnnotation
        RenderingContextOpenGL2
        RenderingCore
        RenderingFreeType
        RenderingGL2PSOpenGL2
        RenderingOpenGL2
        IOXML
        IOOggTheora
        IOLegacy
        REQUIRED
    )
endif()

# ============================================================================
# Include directories
# ============================================================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OOPENCALVIEWER_DIR}
    ${OOPENCALVIEWER_DIR}/visualiserProxy
    ${OOPENCALVIEWER_DIR}/visualiser
    ${OOPENCALVIEWER_DIR}/widgets
    ${OOPENCAL_DIR}
    ${VTK_INCLUDE_DIRS}
)

# ============================================================================
# Optional extra include directories (passed from bash)
# ============================================================================
set(EXTRA_INCLUDE_DIR "" CACHE PATH "Optional include directory for cell dependencies")

if(EXTRA_INCLUDE_DIR)
    message(STATUS "Extra include directory: ${EXTRA_INCLUDE_DIR}")
    include_directories(${EXTRA_INCLUDE_DIR})
endif()

# ============================================================================
# Precompiled header shared by all plugins (optional, see doc/PRECOMPILED_HEADER.md)
# ============================================================================
# Parsing the Viewer headers is a big part of the time needed to build a plugin, and every plugin parses the same ones.
# scripts/build_plugin.sh therefore builds them once per Viewer: it configures THIS project with
# -DOOPENCAL_PLUGIN_PCH_ONLY=ON in a separate build directory (so the compiler flags are identical by construction) and passes
# that directory to the plugin builds as OOPENCAL_PRECOMPILED_HEADER_DIR. When it does not fit, the compiler just
# parses the headers as usual.
option(OOPENCAL_PLUGIN_PCH_ONLY "Only build the precompiled header which is shared by the plugins" OFF)
set(OOPENCAL_PRECOMPILED_HEADER_DIR "" CACHE PATH "Build directory of the shared precompiled header (see OOPENCAL_PLUGIN_PCH_ONLY)")

if(OOPENCAL_PLUGIN_PCH_ONLY)
    add_library(OOpenCalPluginPch SHARED PluginPrecompiledStub.cpp)
    target_precompile_headers(OOpenCalPluginPch PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/OOpenCalPluginPrecompiled.h")
    target_link_libraries(OOpenCalPluginPch PRIVATE ${VTK_LIBRARIES})
    # g++ accepts a precompiled header only when every macro defined while it was built is defined for the plugin too.
    # CMake would add -D<target>_EXPORTS, which the plugins do not have.
    set_target_properties(OOpenCalPluginPch PROPERTIES DEFINE_SYMBOL "")
    message(STATUS "Building only the precompiled header shared by the plugins")
    return()
endif()

# ============================================================================
# Build the plugin as a shared library
# ============================================================================
set(PLUGIN_LIB_NAME "${PLUGIN_CELL_CLASS}Plugin")

# Define source list safely
set(PLUGIN_SOURCES Plugin_FullTemplate.cpp)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    list(APPEND PLUGIN_SOURCES README.md)
endif()

add_library(${PLUGIN_LIB_NAME} SHARED ${PLUGIN_SOURCES})

target_compile_definitions(${PLUGIN_LIB_NAME}
    PRIVATE
        PLUGIN_MODEL_NAME="\"${PLUGIN_MODEL_NAME}\""
        PLUGIN_CELL_CLASS=${PLUGIN_CELL_CLASS}
)

target_link_libraries(${PLUGIN_LIB_NAME}
    PRIVATE
        ${VTK_LIBRARIES}
)

set_target_properties(${PLUGIN_LIB_NAME} PROPERTIES
    PREFIX "lib"
)

# Use the shared precompiled header when it has been built (g++: .gch next to cmake_pch.hxx, clang: .pch)
if(OOPENCAL_PRECOMPILED_HEADER_DIR)
    file(GLOB_RECURSE OOPENCAL_PCH_CANDIDATES "${OOPENCAL_PRECOMPILED_HEADER_DIR}/CMakeFiles/OOpenCalPluginPch.dir/cmake_pch.hxx")
    if(OOPENCAL_PCH_CANDIDATES)
        list(GET OOPENCAL_PCH_CANDIDATES 0 OOPENCAL_PCH_HXX)
    endif()
endif()

if(OOPENCAL_PCH_HXX AND EXISTS "${OOPENCAL_PCH_HXX}.gch")
    message(STATUS "Using the shared precompiled header: ${OOPENCAL_PCH_HXX}.gch")
    target_compile_options(${PLUGIN_LIB_NAME} PRIVATE "SHELL:-include ${OOPENCAL_PCH_HXX}" -Winvalid-pch)
elseif(OOPENCAL_PCH_HXX AND EXISTS "${OOPENCAL_PCH_HXX}.pch")
    message(STATUS "Using the shared precompiled header: ${OOPENCAL_PCH_HXX}.pch")
    target_compile_options(${PLUGIN_LIB_NAME} PRIVATE "SHELL:-Xclang -include-pch -Xclang ${OOPENCAL_PCH_HXX}.pch")
else()
    message(STATUS "Shared precompiled header: not used")
endif()

# ============================================================================
# Installation (optional)
# ============================================================================

install(TARGETS ${PLUGIN_LIB_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
)

# ============================================================================
# Build instructions message
# ============================================================================

# ------------------------------
# Safe defaults for plugin build
# ------------------------------
if(NOT DEFINED PLUGIN_MODEL_NAME OR PLUGIN_MODEL_NAME STREQUAL "")
    set(PLUGIN_MODEL_NAME "Custom Model")
endif()

if(NOT DEFINED PLUGIN_CELL_CLASS OR PLUGIN_CELL_CLASS STREQUAL "")
    set(PLUGIN_CELL_CLASS CustomCell)
endif()

message(STATUS "========================================")
message(STATUS "${PROJECT_NAME} Configuration")
message(STATUS "========================================")
message(STATUS "Build type:          ${CMAKE_BUILD_TYPE}")
message(STATUS "OOpenCal-Viewer dir: ${OOPENCALVIEWER_DIR}")
message(STATUS "OOpenCAL directory:  ${OOPENCAL_DIR}")
message(STATUS "VTK version:         ${VTK_VERSION}")
message(STATUS "----------------------------------------")
message(STATUS "Plugin model name:   ${PLUGIN_MODEL_NAME}")
message(STATUS "Plugin cell class:   ${PLUGIN_CELL_CLASS}")
message(STATUS "----------------------------------------")

get_directory_property(HAS_PARENT PARENT_DIRECTORY)
if(HAS_PARENT)
    message(STATUS "Building as subdirectory of main project")
else()
    message(STATUS "Building standalone")
    message(STATUS "")
    message(STATUS "To build:")
    message(STATUS "  mkdir build && cd build")
    message(STATUS "  cmake ..")
    message(STATUS "  make")
endif()

message(STATUS "")
message(STATUS "Output: lib${PLUGIN_CELL_CLASS}Plugin.so")
message(STATUS "========================================")
