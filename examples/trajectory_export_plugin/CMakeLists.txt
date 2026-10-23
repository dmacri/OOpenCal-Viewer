cmake_minimum_required(VERSION 3.16)
project(TrajectoryExportPlugin VERSION 1.0 LANGUAGES CXX)

# An action plugin needs nothing but the C++ standard library and plugins/ActionApi.h of the Viewer:
# no Qt, no VTK and no OOpenCAL. Build it with the same compiler (and standard library) as the Viewer.
#
#   cmake -S examples/trajectory_export_plugin -B build_trajectory_plugin && cmake --build build_trajectory_plugin
#   OOpenCal-Viewer --listActions --loadModel build_trajectory_plugin/libTrajectoryExport.so

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OOPENCALVIEWER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Path to OOpenCal-Viewer source")

add_library(TrajectoryExport SHARED TrajectoryExportPlugin.cpp JulichTrajectoryAction.h)
target_include_directories(TrajectoryExport PRIVATE "${OOPENCALVIEWER_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(TrajectoryExport PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # standalone build: nothing to do. As a part of the Viewer the library is put where the Viewer looks for plugins
else()
    set_target_properties(TrajectoryExport PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")
endif()

install(TARGETS TrajectoryExport LIBRARY DESTINATION plugins RUNTIME DESTINATION plugins)
