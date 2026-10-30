@echo off
rem Launcher of the portable OOpenCal-Viewer (MinGW-w64, with the bundled C++ compiler) which keeps a console window open.
rem Normally use OOpenCal-Viewer.exe (no console window); use this file when the output of the program is of interest, e.g. the
rem compiler messages when a model is being compiled. Command line arguments are passed to the program, e.g.
rem     OOpenCal-Viewer-console.bat C:\path\to\model\directory

rem The program and the compiler print in UTF-8; the previous code page is put back at the end
set "OOPENCAL_OLD_CODEPAGE="
for /f "tokens=2 delims=:" %%c in ('chcp') do set "OOPENCAL_OLD_CODEPAGE=%%c"
chcp 65001 >nul

call "%~dp0compiler-env.bat"
if errorlevel 1 (
    pause
    if defined OOPENCAL_OLD_CODEPAGE chcp %OOPENCAL_OLD_CODEPAGE% >nul
    exit /b 1
)

"%OOPENCAL_PACKAGE_ROOT%\bin\OOpenCal-Viewer.exe" %*
set "EXITCODE=%ERRORLEVEL%"
if not "%EXITCODE%"=="0" (
    echo.
    echo OOpenCal-Viewer finished with exit code %EXITCODE%
    pause
)
if defined OOPENCAL_OLD_CODEPAGE chcp %OOPENCAL_OLD_CODEPAGE% >nul
exit /b %EXITCODE%
