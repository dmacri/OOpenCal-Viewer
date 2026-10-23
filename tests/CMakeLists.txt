# Include GTest
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Disable IWYU for GoogleTest targets to avoid warnings from system headers
set_target_properties(gtest PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "")
set_target_properties(gtest_main PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "")
set_target_properties(gmock PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "")
set_target_properties(gmock_main PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "")

# ============================================
# OOpenCAL include directory (optional)
# ============================================
set(OOPENCAL_DIR "${OOPENCAL_DIR}" CACHE PATH "Path to the base directory of OOpenCAL sources")

if(OOPENCAL_DIR)
    message(STATUS "[Tests] Using OOpenCAL directory: ${OOPENCAL_DIR}")
else()
    message(WARNING "[Tests] OOPENCAL_DIR not set! Includes like <OOpenCAL/...> may not work.")
endif()

# Add test executable for ModelReader
add_executable(ModelReaderTests
    ModelReaderTests.cpp
    ${CMAKE_SOURCE_DIR}/data/ModelReader.cpp
)

# Link against GTest
target_link_libraries(ModelReaderTests
    GTest::gtest_main
)

# Include directories for the project
target_include_directories(ModelReaderTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/visualiser
    ${OOPENCAL_DIR}
    ${OOPENCAL_DIR}/OOpenCAL
)

# Define OOPENCAL_DIR for use in C++ code
if(OOPENCAL_DIR)
    target_compile_definitions(ModelReaderTests PRIVATE OOPENCAL_DIR="${OOPENCAL_DIR}")
endif()

# Register ModelReaderTests
add_test(NAME ModelReaderTests COMMAND ModelReaderTests)

# ============================================
# Add test executable for SettingParameter
# ============================================
add_executable(SettingParameterTests
    SettingParameterTests.cpp
    ${CMAKE_SOURCE_DIR}/visualiser/SettingParameter.cpp
)

# Link against GTest
target_link_libraries(SettingParameterTests
    GTest::gtest_main
)

# Include directories for the project
target_include_directories(SettingParameterTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/visualiser
    ${CMAKE_SOURCE_DIR}/core
)

# Register SettingParameterTests
add_test(NAME SettingParameterTests COMMAND SettingParameterTests)

# ============================================
# Add test executable for PrecompiledHeader helpers
# ============================================
add_executable(PrecompiledHeaderTests
    PrecompiledHeaderTests.cpp
    ${CMAKE_SOURCE_DIR}/plugins/PrecompiledHeader.cpp
)

# Link against GTest
target_link_libraries(PrecompiledHeaderTests
    GTest::gtest_main
)

# Include directories for the project
target_include_directories(PrecompiledHeaderTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/plugins
)

# Register PrecompiledHeaderTests
add_test(NAME PrecompiledHeaderTests COMMAND PrecompiledHeaderTests)

# ============================================
# Add test executable for GridWorldMapping (header-only, no Qt/VTK)
# ============================================
add_executable(GridWorldMappingTests
    GridWorldMappingTests.cpp
)

target_link_libraries(GridWorldMappingTests
    GTest::gtest_main
)

target_include_directories(GridWorldMappingTests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME GridWorldMappingTests COMMAND GridWorldMappingTests)

# ============================================
# Add test executable for VolumeZSpacing (header-only, no Qt/VTK)
# ============================================
add_executable(VolumeZSpacingTests
    VolumeZSpacingTests.cpp
)

target_link_libraries(VolumeZSpacingTests
    GTest::gtest_main
)

target_include_directories(VolumeZSpacingTests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME VolumeZSpacingTests COMMAND VolumeZSpacingTests)

# ============================================
# Add test executable for the history of a cell (reading one cell across steps)
# ============================================
add_executable(CellHistoryTests
    CellHistoryTests.cpp
    ${CMAKE_SOURCE_DIR}/data/ModelReader.cpp
)

target_link_libraries(CellHistoryTests
    GTest::gtest_main
)

target_include_directories(CellHistoryTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/visualiser
    ${OOPENCAL_DIR}
    ${OOPENCAL_DIR}/OOpenCAL
)

if(OOPENCAL_DIR)
    target_compile_definitions(CellHistoryTests PRIVATE OOPENCAL_DIR="${OOPENCAL_DIR}")
endif()

add_test(NAME CellHistoryTests COMMAND CellHistoryTests)

# ============================================
# Add test executable for NumberFormatting (header-only, no Qt/VTK)
# ============================================
add_executable(NumberFormattingTests
    NumberFormattingTests.cpp
)

target_link_libraries(NumberFormattingTests
    GTest::gtest_main
)

target_include_directories(NumberFormattingTests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME NumberFormattingTests COMMAND NumberFormattingTests)

# ============================================
# Add test executable for RulerLabelFit (header-only, no Qt/VTK)
# ============================================
add_executable(RulerLabelFitTests
    RulerLabelFitTests.cpp
)

target_link_libraries(RulerLabelFitTests
    GTest::gtest_main
)

target_include_directories(RulerLabelFitTests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME RulerLabelFitTests COMMAND RulerLabelFitTests)

# ============================================
# Add test executable for CameraPivotMath (header-only, no Qt/VTK)
# ============================================
add_executable(CameraPivotMathTests
    CameraPivotMathTests.cpp
)

target_link_libraries(CameraPivotMathTests
    GTest::gtest_main
)

target_include_directories(CameraPivotMathTests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME CameraPivotMathTests COMMAND CameraPivotMathTests)

# ============================================
# Add test executable for AppImagePaths (header-only, Qt Core only)
# ============================================
add_executable(AppImagePathsTests
    AppImagePathsTests.cpp
)

target_link_libraries(AppImagePathsTests
    GTest::gtest_main
    ${QT_TARGET_PREFIX}::Core
)

target_include_directories(AppImagePathsTests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME AppImagePathsTests COMMAND AppImagePathsTests)

# ============================================
# Add test executable for the action plugin mechanism (std only: no Qt, no VTK)
# ============================================
add_executable(ActionApiTests
    ActionApiTests.cpp
    ${CMAKE_SOURCE_DIR}/plugins/ActionRegistry.cpp
)

target_link_libraries(ActionApiTests
    GTest::gtest_main
)

target_include_directories(ActionApiTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/examples/trajectory_export_plugin
)

add_test(NAME ActionApiTests COMMAND ActionApiTests)
