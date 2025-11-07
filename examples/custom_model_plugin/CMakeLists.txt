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
        REQUIRED
            GUISupportQt
            IOLegacy
    )
endif()

# Try to find Qt6 first, fall back to Qt5 (Qt5 is for GithubCI for compatibility with VTK)
find_package(Qt6 COMPONENTS Core Gui Widgets OpenGL QUIET)
if (NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL REQUIRED)
    set(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL)
else()
    set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL)
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
