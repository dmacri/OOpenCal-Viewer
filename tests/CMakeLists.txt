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

# Add test executable
add_executable(ModelReaderTests
    ModelReaderTests.cpp
)

# Link against GTest
target_link_libraries(ModelReaderTests
    GTest::gtest_main
)

# Include directories for the project
target_include_directories(ModelReaderTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/utilities
)

# Register tests
add_test(NAME ModelReaderTests COMMAND ModelReaderTests)
