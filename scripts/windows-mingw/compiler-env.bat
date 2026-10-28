@echo off
rem ---------------------------------------------------------------------------
rem Environment of the portable OOpenCal-Viewer package (MinGW-w64, bundled compiler).
rem Sets everything which OOpenCal-Viewer needs to compile models on the fly with the
rem compiler from the "compiler" directory of this package. It is the Windows counterpart
rem of the environment which AppRun sets in the Linux AppImage.
rem
rem It does not start anything, so it can be used from other scripts as well:
rem     call "<package>\compiler-env.bat"
rem It is "call"-able and leaves the environment variables set in the calling shell.
rem ---------------------------------------------------------------------------

rem Package directory without a trailing backslash (%~dp0 ends with one, which breaks quoted paths: "C:\dir\")
for %%I in ("%~dp0.") do set "OOPENCAL_PACKAGE_ROOT=%%~fI"

rem The same path with forward slashes: used in the flags which are passed to the compiler
set "OOPENCAL_PACKAGE_ROOT_FS=%OOPENCAL_PACKAGE_ROOT:\=/%"

if not exist "%OOPENCAL_PACKAGE_ROOT%\bin\OOpenCal-Viewer.exe" goto :broken
if not exist "%OOPENCAL_PACKAGE_ROOT%\compiler\bin\g++.exe" goto :broken

rem The Viewer reads include paths from the environment and splits them on spaces, so a path with a space would break them
if "%OOPENCAL_PACKAGE_ROOT%"=="%OOPENCAL_PACKAGE_ROOT: =%" goto :nospaces
echo WARNING: the package is in a directory whose path contains a space:
echo          %OOPENCAL_PACKAGE_ROOT%
echo          Compilation of models will fail. Move the package to a path without spaces, e.g. C:\OOpenCal-Viewer
:nospaces

rem The program, its DLLs (Qt, VTK) and the compiler (with the DLLs of the compiler itself)
set "PATH=%OOPENCAL_PACKAGE_ROOT%\bin;%OOPENCAL_PACKAGE_ROOT%\compiler\bin;%PATH%"

rem The compiler used to build models (OOpenCal-Viewer prefers it over anything found in PATH)
set "OOPENCAL_COMPILER=%OOPENCAL_PACKAGE_ROOT_FS%/compiler/bin/g++.exe"

rem Headers: OOPENCAL_DIR contains OOpenCAL/base, VTK_INCLUDES is the VTK include directory
set "OOPENCAL_DIR=%OOPENCAL_PACKAGE_ROOT_FS%/include"
set "OOPENCAL_VIEWER_ROOT=%OOPENCAL_PACKAGE_ROOT_FS%/include/oopencal-viewer"
set "VTK_INCLUDES=%OOPENCAL_PACKAGE_ROOT_FS%/include/vtk"

rem Libraries which are linked into every model (a Windows DLL cannot have unresolved symbols):
rem the import library of OOpenCal-Viewer.exe (models register themselves in it) and the import libraries of VTK (lib\plugin-libs.rsp)
set "OOPENCAL_LINK_FLAGS=-L"%OOPENCAL_PACKAGE_ROOT_FS%/lib" -lOOpenCal-Viewer @"%OOPENCAL_PACKAGE_ROOT_FS%/lib/plugin-libs.rsp""

exit /b 0

:broken
echo ERROR: this does not look like a complete OOpenCal-Viewer package:
echo        %OOPENCAL_PACKAGE_ROOT%
echo        Extract the whole ZIP archive (not only a part of it) and run the launcher from there.
exit /b 1
