@echo off
REM Setup script for Windows - OOpenCal-Viewer with vcpkg
REM This script automates the initial setup process for Visual Studio
REM Usage: bootstrap-vcpkg.bat [vcpkg_root_path]
REM Example: bootstrap-vcpkg.bat C:\vcpkg

setlocal enabledelayedexpansion

echo ========================================
echo OOpenCal-Viewer - vcpkg Setup
echo ========================================
echo.

REM Get vcpkg root path from argument or use default
if not "%~1"=="" (
    set VCPKG_ROOT=%~1
    echo Using vcpkg root: !VCPKG_ROOT!
) else (
    set VCPKG_ROOT=%cd%\.vcpkg
    echo Using default vcpkg root: !VCPKG_ROOT!
)

if not exist "!VCPKG_ROOT!" (
    echo.
    echo ERROR: vcpkg directory not found at: !VCPKG_ROOT!
    echo Please provide the correct vcpkg path as argument or install vcpkg first.
    echo Usage: %0 C:\vcpkg
    exit /b 1
)

echo vcpkg found at: !VCPKG_ROOT!
echo.

REM Check if build directory exists
if not exist "build" (
    echo Creating build directory...
    mkdir build
) else (
    echo Build directory already exists
)

cd build

REM Step 1: Initial CMake configuration
echo.
echo Step 1: Configuring CMake...
echo.
cmake -G "Visual Studio 17 2022" ..

if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    cd ..
    exit /b 1
)

REM Step 2: Reconfigure CMake with vcpkg toolchain and Qt6
echo.
echo Step 2: Reconfiguring CMake with vcpkg toolchain...
echo.
cmake -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=!VCPKG_ROOT!/scripts/buildsystems/vcpkg.cmake -DUSE_QT6=ON ..

if errorlevel 1 (
    echo ERROR: CMake configuration with vcpkg failed!
    cd ..
    exit /b 1
)

echo.
echo ========================================
echo vcpkg setup completed successfully!
echo ========================================
echo.
echo Next steps:
echo 1. Open the generated Visual Studio solution
echo 2. Build the OOpenCal-Viewer target
echo.
echo Or build from command line:
echo   cmake --build . --config Debug --target OOpenCal-Viewer
echo.

cd ..

if errorlevel 1 (
    echo ERROR: CMake reconfiguration with vcpkg toolchain failed!
    cd ..
    exit /b 1
)

REM Step 4: Build the project
echo.
echo Step 4: Building OOpenCal-Viewer...
echo.
cmake --build . --config Debug

if errorlevel 1 (
    echo ERROR: Build failed!
    cd ..
    exit /b 1
)

cd ..

echo.
echo ========================================
echo Setup completed successfully!
echo ========================================
echo.
echo Next time, you can just run:
echo   cd build
echo   cmake --build . --config Debug
echo.

endlocal
