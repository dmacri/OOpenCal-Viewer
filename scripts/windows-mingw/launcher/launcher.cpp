/** @file launcher.cpp
 * @brief Double-click launcher of the portable OOpenCal-Viewer (MinGW-w64 package with the bundled compiler).
 *
 * It is the counterpart of OOpenCal-Viewer-console.bat without the console window: a program of the GUI subsystem (so Windows opens no
 * console for it), with the icon of the application. What it does:
 *
 *   1. takes the environment from compiler-env.bat (PATH, OOPENCAL_COMPILER, OOPENCAL_DIR, ...), which stays the only place that knows
 *      it: the batch file is run by a hidden cmd.exe and the variables it sets are copied,
 *   2. starts bin\OOpenCal-Viewer.exe with the arguments of the launcher, without a console window; the compilers which it starts
 *      for models do not get a window either,
 *   3. writes everything that the program prints to a log file (the console is not there to show it) and reports a program that
 *      ended with an error.
 *
 * Log file: %LOCALAPPDATA%\OOpenCal-Viewer\OOpenCal-Viewer.log, or the file in the variable OOPENCAL_LAUNCHER_LOG (used by the
 * smoke test).
 *
 * Standalone on purpose: no Qt, no DLLs (it is linked statically), it only has to work when everything else in the package is broken. */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>

#include <cwchar>
#include <string>

namespace
{
const wchar_t* const kTitle = L"OOpenCal-Viewer";

void showMessage(const std::wstring& text, UINT icon)
{
    MessageBoxW(nullptr, text.c_str(), kTitle, MB_OK | icon | MB_SETFOREGROUND);
}

std::wstring errorText(DWORD error)
{
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0,
                                        reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring text = L"error " + std::to_wstring(error);
    if (length && buffer)
        text += L": " + std::wstring(buffer, length);
    if (buffer)
        LocalFree(buffer);
    return text;
}

bool fileExists(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && ! (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

/** Directory of this executable, i.e. the root of the package. Empty on failure. */
std::wstring moduleDirectory()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return L"";
        if (length < buffer.size())
        {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    const auto slash = buffer.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"" : buffer.substr(0, slash);
}

/** The command line without the name of the program (empty, or starting with a space). */
std::wstring argumentsAfterProgramName()
{
    const wchar_t* c = GetCommandLineW();
    if (*c == L'"')
    {
        ++c;
        while (*c && *c != L'"')
            ++c;
        if (*c)
            ++c;
    }
    else
    {
        while (*c && *c != L' ' && *c != L'\t')
            ++c;
    }
    return c;
}

std::wstring fromUtf8(const std::string& text)
{
    if (text.empty())
        return L"";
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

HANDLE openNul(DWORD access)
{
    SECURITY_ATTRIBUTES inheritable{ sizeof(inheritable), nullptr, TRUE };
    return CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

/** Runs a command line without a window and collects what it prints (stdout and stderr together). */
bool runHidden(std::wstring commandLine, std::wstring& output, DWORD& exitCode)
{
    SECURITY_ATTRIBUTES inheritable{ sizeof(inheritable), nullptr, TRUE };
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (! CreatePipe(&readEnd, &writeEnd, &inheritable, 0))
        return false;
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0); // the child must not hold the read end

    HANDLE input = openNul(GENERIC_READ);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = writeEnd;
    startup.hStdError = writeEnd;

    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(writeEnd); // so that ReadFile ends when the child exits
    if (input != INVALID_HANDLE_VALUE)
        CloseHandle(input);
    if (! started)
    {
        CloseHandle(readEnd);
        return false;
    }

    std::string bytes;
    char chunk[4096];
    DWORD got = 0;
    while (ReadFile(readEnd, chunk, sizeof(chunk), &got, nullptr) && got > 0)
        bytes.append(chunk, got);
    CloseHandle(readEnd);

    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

    output = fromUtf8(bytes);
    return true;
}

bool isEnvironmentOfThePackage(const std::wstring& name)
{
    return _wcsicmp(name.c_str(), L"PATH") == 0 || _wcsicmp(name.c_str(), L"VTK_INCLUDES") == 0 || _wcsnicmp(name.c_str(), L"OOPENCAL_", 9) == 0;
}

/** Copies the variables which compiler-env.bat sets into the environment of this process (and so of the program started later).
 *  The error text is filled when the script reports a problem. */
bool loadPackageEnvironment(const std::wstring& root, std::wstring& error)
{
    const std::wstring script = root + L"\\compiler-env.bat";
    if (! fileExists(script))
    {
        error = L"compiler-env.bat is missing:\n" + script;
        return false;
    }

    // chcp 65001: the script and `set` print in UTF-8, so paths with national characters survive the pipe
    const std::wstring command = L"cmd.exe /d /s /c \"chcp 65001 >nul & call \"" + script + L"\" && set\"";
    std::wstring output;
    DWORD exitCode = 1;
    if (! runHidden(command, output, exitCode))
    {
        error = L"cannot start cmd.exe: " + errorText(GetLastError());
        return false;
    }
    if (exitCode != 0)
    {
        error = L"compiler-env.bat failed:\n\n" + output;
        return false;
    }

    std::size_t begin = 0;
    while (begin < output.size())
    {
        std::size_t end = output.find(L'\n', begin);
        if (end == std::wstring::npos)
            end = output.size();
        std::wstring line = output.substr(begin, end - begin);
        begin = end + 1;
        if (! line.empty() && line.back() == L'\r')
            line.pop_back();

        const auto equals = line.find(L'=');
        if (equals == std::wstring::npos || equals == 0) // "=C:=C:\dir" entries of cmd.exe have an empty name
            continue;
        const std::wstring name = line.substr(0, equals);
        if (isEnvironmentOfThePackage(name))
            SetEnvironmentVariableW(name.c_str(), line.c_str() + equals + 1);
    }
    return true;
}

std::wstring environmentValue(const wchar_t* name)
{
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0)
        return L"";
    std::wstring value(size, L'\0');
    value.resize(GetEnvironmentVariableW(name, value.data(), size));
    return value;
}

/** Log file for the output of the program: where it is and a handle to write to (inheritable). */
HANDLE openLog(std::wstring& path)
{
    path = environmentValue(L"OOPENCAL_LAUNCHER_LOG");
    if (path.empty())
    {
        const std::wstring base = environmentValue(L"LOCALAPPDATA");
        if (! base.empty())
        {
            const std::wstring directory = base + L"\\OOpenCal-Viewer";
            CreateDirectoryW(directory.c_str(), nullptr);
            path = directory + L"\\OOpenCal-Viewer.log";
        }
    }

    SECURITY_ATTRIBUTES inheritable{ sizeof(inheritable), nullptr, TRUE };
    if (! path.empty())
    {
        HANDLE log = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log != INVALID_HANDLE_VALUE)
            return log;
    }
    path.clear();
    return openNul(GENERIC_WRITE);
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const std::wstring root = moduleDirectory();
    const std::wstring program = root + L"\\bin\\OOpenCal-Viewer.exe";
    if (root.empty() || ! fileExists(program) || ! fileExists(root + L"\\compiler\\bin\\g++.exe"))
    {
        showMessage(L"This does not look like a complete OOpenCal-Viewer package:\n" + root +
                        L"\n\nExtract the whole ZIP archive (not only a part of it) and run OOpenCal-Viewer.exe from there.",
                    MB_ICONERROR);
        return 1;
    }

    if (root.find(L' ') != std::wstring::npos)
        showMessage(L"The package is in a directory whose path contains a space:\n" + root +
                        L"\n\nThe program will start, but compilation of models will fail. Move the package to a path without spaces, "
                        L"e.g. C:\\OOpenCal-Viewer.",
                    MB_ICONWARNING);

    std::wstring error;
    if (! loadPackageEnvironment(root, error))
    {
        showMessage(error, MB_ICONERROR);
        return 1;
    }

    std::wstring logPath;
    HANDLE log = openLog(logPath);
    HANDLE input = openNul(GENERIC_READ);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = log;
    startup.hStdError = log;

    // CREATE_NO_WINDOW gives the program a console which is not shown; the compilers which it starts use it too, so none of them
    // opens a window of its own
    std::wstring commandLine = L"\"" + program + L"\"" + argumentsAfterProgramName();
    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    const DWORD startError = GetLastError();
    if (input != INVALID_HANDLE_VALUE)
        CloseHandle(input);
    if (log != INVALID_HANDLE_VALUE)
        CloseHandle(log);
    if (! started)
    {
        showMessage(L"Cannot start\n" + program + L"\n\n" + errorText(startError), MB_ICONERROR);
        return 1;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

    if (exitCode != 0)
    {
        wchar_t code[32];
        swprintf(code, sizeof(code) / sizeof(code[0]), L"0x%08lX", static_cast<unsigned long>(exitCode));
        std::wstring text = L"OOpenCal-Viewer ended with an error (exit code " + std::wstring(code) + L").";
        if (exitCode == 0xC0000135UL)
            text += L"\nA DLL is missing: extract the whole ZIP archive and do not move the files out of the package.";
        if (! logPath.empty())
            text += L"\n\nWhat the program printed is in:\n" + logPath;
        showMessage(text, MB_ICONERROR);
    }
    return static_cast<int>(exitCode);
}
