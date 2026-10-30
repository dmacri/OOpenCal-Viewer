@echo off
rem Double-click launcher of the portable OOpenCal-Viewer (MinGW-w64, with the bundled C++ compiler).
rem No installation and no other software are needed. Command line arguments are passed to the program, e.g.
rem     OOpenCal-Viewer.bat C:\path\to\model\directory
rem The window stays open: it shows the output of the compiler when a model is being compiled.

call "%~dp0compiler-env.bat"
if errorlevel 1 (
    pause
    exit /b 1
)

"%OOPENCAL_PACKAGE_ROOT%\bin\OOpenCal-Viewer.exe" %*
set "EXITCODE=%ERRORLEVEL%"
if not "%EXITCODE%"=="0" (
    echo.
    echo OOpenCal-Viewer finished with exit code %EXITCODE%
    pause
)
exit /b %EXITCODE%
