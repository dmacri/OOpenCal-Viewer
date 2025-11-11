cmake_minimum_required(VERSION 3.10)

project(OOpenCal-Viewer)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libraries statically" FORCE)

# ============================================
# Compiler warnings (optional, full strict set)
# ============================================
# Enable full warning level by running CMake with:
#   -DENABLE_WARNINGS=ON
#
# Default = OFF for fast builds
option(ENABLE_WARNINGS "Enable full compiler warnings" OFF)

function(enable_project_warnings target)
    if (MSVC)
        # /W4: high warning level
        # /permissive-: enforce standard C++ conformance
        # /EHsc: standard C++ exception handling
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)

    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wdouble-promotion
            -Wnull-dereference
            -Wformat=2
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wold-style-cast
            -Wunused
            -Wuninitialized
            -Woverloaded-virtual
        )

    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wdouble-promotion
            -Wnull-dereference
            -Wformat=2
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wold-style-cast
            -Wunused
            -Wuninitialized
            -Woverloaded-virtual
        )
    endif()
endfunction()


# ============================================
# Sanitizers (Address + Undefined) - optional
# Enable with:  -DENABLE_SANITIZERS=ON
# Default = ON (good for development & testing)
# Automatically disabled on Windows
# ============================================
option(ENABLE_SANITIZERS "Enable runtime sanitizers (ASan + UBSan)" ON)

function(enable_project_sanitizers target)
    # Enable sanitizers on non-Windows with GCC/Clang
    if (NOT CMAKE_SYSTEM_NAME MATCHES "Windows" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        include(CheckLibraryExists)

        check_library_exists(asan __asan_init "" HAVE_LIBASAN)
        check_library_exists(ubsan __ubsan_handle_add_overflow "" HAVE_LIBUBSAN)

        if (HAVE_LIBASAN AND HAVE_LIBUBSAN)
            message(STATUS "Sanitizers available: enabling AddressSanitizer and UndefinedBehaviorSanitizer")
            set(SANITIZER_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
            set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
        else()
            message(WARNING "Sanitizers requested but libasan/libubsan not available on this system")
        endif()
    endif()
endfunction()


# ============================================
# Sources and resources
# ============================================
file(GLOB_RECURSE Headers
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/*.h"
    "${CMAKE_SOURCE_DIR}/*.hpp"
)
file(GLOB Sources
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/*.cpp"
)
list(APPEND Sources
    config/Config.cpp
    config/ConfigCategory.cpp
    visualiser/SettingParameter.cpp
    visualiser/VideoExporter.cpp
    visualiser/Visualiser.cpp
    visualiserProxy/SceneWidgetVisualizerFactory.cpp
    widgets/ClickableLabel.cpp
    widgets/ConfigDetailsDialog.cpp
    widgets/SceneWidget.cpp
    widgets/ColorSettingsDialog.cpp
    widgets/ColorSettings.cpp
    widgets/AboutDialog.cpp
    widgets/CompilationLogWidget.cpp
    widgets/ReductionDialog.cpp
    widgets/ReductionDisplayWidget.cpp
    widgets/SubstateDisplayWidget.cpp
    widgets/SubstatesDockWidget.cpp
    utilities/PluginLoader.cpp
    utilities/ReductionManager.cpp
    utilities/ModelReader.cpp
    utilities/CommandLineParser.cpp
    utilities/ModelLoader.cpp
    utilities/CppModuleBuilder.cpp
)
file(GLOB Resources
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/*.qrc"
)
file(GLOB Styles
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/*.qss"
)
file(GLOB UIs
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/*.ui"
)
file(GLOB_RECURSE Docs
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/*.md"
)
set(OTHER_FILES
    LICENSE
    .github/workflows/release--deb-package--ubuntu24.04.yml
    .github/workflows/release--deb-package-bundled--ubuntu24.04.yml
    .github/workflows/release--app-image--ubuntu24.04.yml
    .githooks/pre-commit
    .clang-format
)

# ============================================
# VTK dependencies
# ============================================
# NOTE:
# VTK must be found *before* Qt to avoid a known issue in Qt6 (6.8–6.9)
# where re-importing Qt targets (e.g., Qt6::Platform*) causes
# "Some (but not all) targets in this export set were already defined" errors.
# The 'GUISupportQt' module internally calls find_package(Qt6),
# so finding VTK first prevents duplicate Qt target definitions.
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

# ============================================
# Qt setup + dependencies
# ============================================
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC "YES")
set(CMAKE_AUTOUIC "YES")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
message(STATUS "Using Qt${QT_VERSION_MAJOR} (${Qt${QT_VERSION_MAJOR}_VERSION})")

# ============================================
# Executable target
# ============================================
add_executable(${PROJECT_NAME} MACOSX_BUNDLE
    ${Sources} ${Headers} ${Resources} ${UIs} ${Styles} ${Docs} ${OTHER_FILES}
)

if (ENABLE_WARNINGS)
    enable_project_warnings(${PROJECT_NAME})
endif()

if (ENABLE_SANITIZERS)
    enable_project_sanitizers(${PROJECT_NAME})
endif()

# ============================================
# OOpenCAL include directory (optional)
# ============================================
set(OOPENCAL_DIR "/home/dmacri80/Progetto-Visualizer/" CACHE PATH "Path to the base directory of OOpenCAL sources")

if(OOPENCAL_DIR)
    message(STATUS "Using OOpenCAL directory: ${OOPENCAL_DIR}")
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${OOPENCAL_DIR}
        ${OOPENCAL_DIR}/OOpenCAL
    )

    # Define OOPENCAL_DIR for use in C++ code
    target_compile_definitions(${PROJECT_NAME} PRIVATE OOPENCAL_DIR="${OOPENCAL_DIR}")
else()
    message(WARNING "OOPENCAL_DIR not set! Includes like <OOpenCAL/...> may not work.")
endif()


# ============================================
# FetchContent setup (offline-friendly)
# ============================================
# The dependencies are fetched once and then reused without attempting updates
# (useful when working without network access).
include(FetchContent)

set(SKIP_UPDATES ON CACHE BOOL "Skip updating FetchContent dependencies after initial download")

if(SKIP_UPDATES)
    set(_UPDATE_ARGS UPDATE_DISCONNECTED ON)
else()
    set(_UPDATE_ARGS)
endif()

# ============================================
# inih library (FetchContent)
# ============================================
FetchContent_Declare(
    inih
    GIT_REPOSITORY https://github.com/benhoyt/inih.git
    GIT_TAG        master
    ${_UPDATE_ARGS}
)

FetchContent_MakeAvailable(inih)

FetchContent_GetProperties(inih)
if(inih_SOURCE_DIR)
    target_sources(${PROJECT_NAME} PRIVATE
        ${inih_SOURCE_DIR}/ini.c
        ${inih_SOURCE_DIR}/cpp/INIReader.cpp
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${inih_SOURCE_DIR}
        ${inih_SOURCE_DIR}/cpp
    )
else()
    message(FATAL_ERROR "inih source directory not found. Ensure initial FetchContent download succeeded.")
endif()

# ============================================
# argparse library (FetchContent)
# ============================================
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG        v3.0
    ${_UPDATE_ARGS}
)

FetchContent_MakeAvailable(argparse)

FetchContent_GetProperties(argparse)
if(argparse_SOURCE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${argparse_SOURCE_DIR}/include
    )
else()
    message(FATAL_ERROR "argparse source directory not found. Ensure initial FetchContent download succeeded.")
endif()

# ============================================
# Tiny Process Library (FetchContent)
# ============================================
FetchContent_Declare(
    tiny_process_library
    GIT_REPOSITORY https://gitlab.com/eidheim/tiny-process-library.git
    GIT_TAG master
    ${_UPDATE_ARGS}
)

FetchContent_MakeAvailable(tiny_process_library)

FetchContent_GetProperties(tiny_process_library)
if(tiny_process_library_SOURCE_DIR)
    target_sources(${PROJECT_NAME} PRIVATE
        ${tiny_process_library_SOURCE_DIR}/process.cpp
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${tiny_process_library_SOURCE_DIR}
    )
else()
    message(FATAL_ERROR "tiny_process_library source directory not found. Ensure initial FetchContent download succeeded.")
endif()


# ============================================
# Linking
# ============================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    # Qt:
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    # VTK:
    ${VTK_LIBRARIES}
    # other:
    tiny-process-library
)

# Export symbols for plugins to use
# This allows plugins to call functions from the main executable
target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)

vtk_module_autoinit(
    TARGETS ${PROJECT_NAME}
    MODULES VTK::GUISupportQt VTK::IOLegacy
)

# ============================================
# Export VTK compile flags to C++ header (this is temporary as long as plugin needs VTK)
# ============================================

# Select the main VTK target (always present)
set(VTK_MAIN_TARGET VTK::CommonCore)

# Safely read target properties (may contain generator expressions)
get_target_property(_vtk_includes ${VTK_MAIN_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_vtk_defs     ${VTK_MAIN_TARGET} INTERFACE_COMPILE_DEFINITIONS)
get_target_property(_vtk_opts     ${VTK_MAIN_TARGET} INTERFACE_COMPILE_OPTIONS)

# Prepare resolved lists
set(VTK_INCLUDES_EXPANDED "")
set(VTK_DEFS_EXPANDED "")
set(VTK_OPTS_EXPANDED "")

# Function to strip generator expressions like $<BUILD_INTERFACE:...> or $<INSTALL_INTERFACE:...>
function(clean_genex output input_list)
    set(temp_list "")
    foreach(item IN LISTS input_list)
        # Remove $<BUILD_INTERFACE:...> and $<INSTALL_INTERFACE:...>
        string(REGEX REPLACE "\\$<BUILD_INTERFACE:([^>]+)>" "\\1" item "${item}")
        string(REGEX REPLACE "\\$<INSTALL_INTERFACE:([^>]+)>" "\\1" item "${item}")
        string(REGEX REPLACE "\\$<[^>]+>" "" item "${item}") # remove any remaining generator expressions
        string(STRIP "${item}" item)
        if (NOT "${item}" STREQUAL "")
            list(APPEND temp_list "${item}")
        endif()
    endforeach()
    set(${output} "${temp_list}" PARENT_SCOPE)
endfunction()

# Clean all lists
clean_genex(VTK_INCLUDES_EXPANDED "${_vtk_includes}")
clean_genex(VTK_DEFS_EXPANDED "${_vtk_defs}")
clean_genex(VTK_OPTS_EXPANDED "${_vtk_opts}")

# Build final compile flags string
set(VTK_COMPILE_FLAGS_STR "")
foreach(inc IN LISTS VTK_INCLUDES_EXPANDED)
    if (EXISTS "${inc}")
        string(APPEND VTK_COMPILE_FLAGS_STR "-I${inc} ")
    endif()
endforeach()

# Generate header file
set(VTK_FLAGS_HEADER "${CMAKE_BINARY_DIR}/generated/vtk_compile_flags.h")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

file(WRITE "${VTK_FLAGS_HEADER}" "
// Auto-generated file: DO NOT EDIT
#pragma once

namespace BuildInfo
{
    static constexpr const char* VTK_COMPILE_FLAGS = R\"(${VTK_COMPILE_FLAGS_STR})\";
}
")

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/generated")

# (Optional) Debug print to verify values
message(STATUS "VTK compile includes: ${VTK_INCLUDES_EXPANDED}")
message(STATUS "VTK compile defs: ${VTK_DEFS_EXPANDED}")
message(STATUS "VTK compile opts: ${VTK_OPTS_EXPANDED}")


# ============================================
# Doxygen Documentation (optional)
# ============================================
find_package(Doxygen)

if(DOXYGEN_FOUND)
    find_program(DOT_PROGRAM dot)           # graphviz (dot) - optional

    set(DOXYGEN_INPUT_DIR "${CMAKE_SOURCE_DIR}")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs")
    set(PROJECT_BRIEF "${PROJECT_NAME} documentation generated by Doxygen")

    set(DOXYFILE_DIR "${CMAKE_SOURCE_DIR}/doc")
    set(DOXYFILE_IN "${DOXYFILE_DIR}/Doxyfile.in")
    set(DOXYFILE_OUT "${CMAKE_BINARY_DIR}/Doxyfile")

if(EXISTS "${DOXYFILE_IN}")
    if(DOT_PROGRAM)
        set(HAVE_DOT "YES")
    else()
        set(HAVE_DOT "NO")
    endif()

    configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating Doxygen documentation (HTML only)..."
        VERBATIM
    )

    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")
    message(STATUS "Docs will be generated at: ${DOXYGEN_OUTPUT_DIR} (HTML)")
    if(DOT_PROGRAM)
        message(STATUS "Graphviz (dot) found: ${DOT_PROGRAM} (class diagrams enabled)")
    else()
        message(STATUS "Graphviz (dot) not found: class diagrams disabled")
    endif()
    else()
        message(WARNING "Doxyfile.in not found in ${DOXYFILE_DIR}. Skipping docs target.")
    endif()
else()
    message(STATUS "Doxygen not found. Skipping docs target.")
endif()

# ============================================
# Development tools:
# ============================================

# Export compile_commands.json for tooling (clang-tidy, cppcheck, IWYU, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# include-what-you-use (IWYU)
find_program(IWYU_PATH NAMES include-what-you-use iwyu)

if (IWYU_PATH)
    message(STATUS "Found include-what-you-use: ${IWYU_PATH}")
    set(IWYU_MAPPING_FILE "${CMAKE_SOURCE_DIR}/config/iwyu.imp")
    if (EXISTS "${IWYU_MAPPING_FILE}")
        list(APPEND CMAKE_CXX_INCLUDE_WHAT_YOU_USE
            ${IWYU_PATH}
            -Xiwyu
            --mapping_file=${IWYU_MAPPING_FILE}
        )
        message(STATUS "IWYU mapping file: ${IWYU_MAPPING_FILE}")
    else()
        message(WARNING "IWYU mapping file not found at ${IWYU_MAPPING_FILE}; running without mapping")
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH})
    endif()
else()
    message(WARNING "include-what-you-use not found. Consider installing it for header optimization.")
endif()

# ============================================
# Formatting source code: clang-format
# ============================================

# Collect all relevant source and header files into a single list.
# This makes it easy to maintain and avoids hardcoded directory paths.
set(FORMAT_SOURCES
    ${Sources}
    ${Headers}
)

# Find clang-format executable
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-14 clang-format-13 clang-format-12)

if(CLANG_FORMAT_EXE)
    message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}, adding format targets")

    # Create a command-safe list by quoting paths (handles spaces in filenames).
    foreach(file ${FORMAT_SOURCES})
        if(EXISTS "${file}")
            list(APPEND FORMAT_SOURCES_SAFE "${file}")
        endif()
    endforeach()

    # Only add the target if there are files to format
    if(FORMAT_SOURCES_SAFE)
        add_custom_target(clang-format
            COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${FORMAT_SOURCES_SAFE}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-format on project sources"
        )

        add_custom_target(clang-format-check
            COMMAND ${CLANG_FORMAT_EXE} -style=file -output-replacements-xml ${FORMAT_SOURCES_SAFE}
                | grep -q "replacement offset"
                && (echo \"Files need reformatting\" && exit 1) || echo \"Everything is properly formatted\"
            COMMENT "Checking formatting with clang-format (no changes applied)"
        )
    else()
        message(WARNING "No source files found to format.")
    endif()
else()
    message(STATUS "clang-format not found, skipping format targets")
endif()

# ============================================
# Development tools: Static analysis & linting
# ============================================

# ============================================
# Development tools: Static analysis & linting (final)
# - Project-full clang-tidy (system headers off)
# - Flawfinder on project files
# - Cppcheck using compile_commands.json with excludes
# Excludes: _deps, external, third-party, vendor, tests, Qt-generated, build artifacts
# ============================================

# Build a filtered list of project source/header files to analyze
set(FILTERED_FILES "")
foreach(f ${Sources} ${Headers})
    # skip non-existing
    if (NOT EXISTS "${f}")
        continue()
    endif()

    # Normalize path for pattern matching
    file(TO_CMAKE_PATH "${f}" _fpath)

    # Exclude FetchContent deps and vendor directories (strong ignore)
    if (_fpath MATCHES ".*/_deps/.*" OR
        _fpath MATCHES ".*/external/.*" OR
        _fpath MATCHES ".*/third[-_]party/.*" OR
        _fpath MATCHES ".*/vendor/.*" OR
        _fpath MATCHES ".*/3rd[-_]party/.*" OR
        _fpath MATCHES ".*/contrib/.*")
        continue()
    endif()

    # Exclude explicit libraries that sometimes appear outside _deps
    if (_fpath MATCHES ".*/argparse(/|$).*" OR _fpath MATCHES ".*/inih(/|$).*")
        continue()
    endif()

    # Exclude test folders (unit tests / testdata)
    if (_fpath MATCHES ".*/tests?/.*" OR _fpath MATCHES ".*/testdata?/.*")
        continue()
    endif()

    # Exclude build artifacts and intermediate autogen dirs
    if (_fpath MATCHES ".*/CMakeFiles/.*" OR
        _fpath MATCHES ".*/\\.autogen/.*" OR
        _fpath MATCHES ".*/\\.moc/.*" OR
        _fpath MATCHES ".*/build/.*")
        continue()
    endif()

    # Exclude Qt-generated files (moc, ui, qrc, qmlcache, etc.)
    if (_fpath MATCHES ".*/moc_.*(\\.cpp|\\.cc)?$" OR
        _fpath MATCHES ".*/ui_.*(\\.h|\\.hpp)?$" OR
        _fpath MATCHES ".*/qrc_.*(\\.cpp)?$" OR
        _fpath MATCHES ".*/qmlcache/.*")
        continue()
    endif()

    # Exclude IDE/editor temp files (safety)
    if (_fpath MATCHES ".*~$" OR _fpath MATCHES ".*\\.swp$")
        continue()
    endif()

    # If we reached this point, include the file for analysis
    list(APPEND FILTERED_FILES "${f}")
endforeach()

# Print summary
list(LENGTH FILTERED_FILES _num_filtered)
message(STATUS "Static analysis: ${_num_filtered} source/header files selected for analysis")

# --------------------------------------------
# clang-tidy (Project-full)
# - system headers disabled
# - header-filter set to project source dir so headers in the project are checked
# --------------------------------------------
find_program(CLANG_TIDY NAMES clang-tidy)
if (CLANG_TIDY)
    message(STATUS "Adding command: clang_tidy (Project-full, system headers disabled)")

    if (FILTERED_FILES)
        # Quote file paths to avoid issues with spaces or special characters
        set(FILTERED_FILES_QUOTED "")
        foreach(f ${FILTERED_FILES})
            list(APPEND FILTERED_FILES_QUOTED "\"${f}\"")
        endforeach()

        add_custom_target(clang_tidy
            DEPENDS ${PROJECT_NAME}
            COMMAND ${CLANG_TIDY}
                "-p=${CMAKE_BINARY_DIR}"
                "--system-headers=false"
                "--header-filter=^${CMAKE_SOURCE_DIR}"
                ${FILTERED_FILES_QUOTED}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy (Project-full) on project sources (system headers disabled)"
            USES_TERMINAL
        )
    else()
        message(WARNING "clang_tidy: no project files to analyze (FILTERED_FILES is empty)")
    endif()
else()
    message(STATUS "clang-tidy not found: skipping clang_tidy target")
endif()

# --------------------------------------------
# flawfinder
# - run only on project files selected above
# --------------------------------------------
find_program(FLAWFINDER_TOOL NAMES flawfinder)
if (FLAWFINDER_TOOL)
    message(STATUS "Adding command: flawfinder (project-only)")

    if (FILTERED_FILES)
        add_custom_target(flawfinder
            COMMAND ${FLAWFINDER_TOOL}
                --quiet --dataonly --nolink --columns --context --singleline --minlevel=0
                ${FILTERED_FILES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running flawfinder on project sources (excluding vendor/_deps/Qt-generated)"
            USES_TERMINAL
        )
    else()
        message(WARNING "flawfinder: no project files to analyze (FILTERED_FILES is empty)")
    endif()
else()
    message(STATUS "flawfinder not found: skipping flawfinder target")
endif()

# --------------------------------------------
# cppcheck
# - use compile_commands.json
# - exclude common vendor and build dirs
# --------------------------------------------
find_program(CPPCHECK NAMES cppcheck)
if (CPPCHECK)
    message(STATUS "Adding command: cppcheck")

    # Exclude patterns for cppcheck (directories to ignore)
    set(CPPCHECK_EXCLUDES
        "-i${CMAKE_BINARY_DIR}"
        "-i${CMAKE_SOURCE_DIR}/_deps"
        "-i${CMAKE_SOURCE_DIR}/external"
        "-i${CMAKE_SOURCE_DIR}/third-party"
        "-i${CMAKE_SOURCE_DIR}/third_party"
        "-i${CMAKE_SOURCE_DIR}/vendor"
        "-i${CMAKE_SOURCE_DIR}/3rd-party"
        "-i${CMAKE_SOURCE_DIR}/contrib"
        "-i${CMAKE_SOURCE_DIR}/tests"
        "-i${CMAKE_SOURCE_DIR}/test"
        # skip Qt generated (some may live in source tree)
        "-i${CMAKE_SOURCE_DIR}/moc_*"
        "-i${CMAKE_SOURCE_DIR}/ui_*"
        "-i${CMAKE_SOURCE_DIR}/qrc_*"
    )

    add_custom_target(cppcheck
        COMMAND ${CPPCHECK}
            --project=${CMAKE_BINARY_DIR}/compile_commands.json
            --force
            --enable=warning,performance,unusedFunction,style,information
            --template=gcc
            --inline-suppr
            --suppress=missingIncludeSystem
            ${CPPCHECK_EXCLUDES}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running cppcheck (project) with excludes for vendor/_deps/Qt-generated"
        USES_TERMINAL
    )
else()
    message(STATUS "cppcheck not found: skipping cppcheck target")
endif()


# --------------------------------------------
# valgrind: Memory leak & runtime error analysis
# --------------------------------------------
find_program(VALGRIND_TOOL NAMES valgrind)
if (VALGRIND_TOOL)
    message(STATUS "Adding command: valgrind (runtime memory analysis)")

    # Define path to the built binary automatically
    if (WIN32)
        message(WARNING "Valgrind is not supported on Windows: skipping valgrind target")
    else()
        # Binary output path depends on configuration type (Debug/Release)
        # Use generator expression to select correct one at runtime
        set(PROJECT_EXECUTABLE "$<TARGET_FILE:${PROJECT_NAME}>")

        # Allow passing custom arguments to valgrind run
        set(VALGRIND_ARGS "" CACHE STRING "Arguments passed to ${PROJECT_NAME} during valgrind run")

        # Add the target
        add_custom_target(valgrind
            DEPENDS ${PROJECT_NAME}
            COMMAND ${VALGRIND_TOOL}
                --leak-check=full
                --show-leak-kinds=all
                --track-origins=yes
                --num-callers=20
                --error-exitcode=1
                --suppressions=${CMAKE_SOURCE_DIR}/valgrind.supp
                ${PROJECT_EXECUTABLE} ${VALGRIND_ARGS}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running valgrind to check for memory leaks and invalid accesses"
            USES_TERMINAL
        )
    endif()
else()
    message(STATUS "valgrind not found: skipping valgrind target")
endif()


# ============================================
# Subproject:
# ============================================
add_subdirectory(examples/custom_model_plugin)
