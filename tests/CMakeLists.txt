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

# ============================================
# OOpenCAL include directory (optional)
# ============================================
set(OOPENCAL_DIR "${OOPENCAL_DIR}" CACHE PATH "Path to the base directory of OOpenCAL sources")

if(OOPENCAL_DIR)
    message(STATUS "[Tests] Using OOpenCAL directory: ${OOPENCAL_DIR}")
else()
    message(WARNING "[Tests] OOPENCAL_DIR not set! Includes like <OOpenCAL/...> may not work.")
endif()

# Add test executable
add_executable(ModelReaderTests
    ModelReaderTests.cpp
    ${CMAKE_SOURCE_DIR}/utilities/ModelReader.cpp
)

# Link against GTest
target_link_libraries(ModelReaderTests
    GTest::gtest_main
)

# Include directories for the project
target_include_directories(ModelReaderTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/utilities
    ${OOPENCAL_DIR}
    ${OOPENCAL_DIR}/OOpenCAL
)

# Define OOPENCAL_DIR for use in C++ code
if(OOPENCAL_DIR)
    target_compile_definitions(ModelReaderTests PRIVATE OOPENCAL_DIR="${OOPENCAL_DIR}")
endif()

# Register tests
add_test(NAME ModelReaderTests COMMAND ModelReaderTests)
