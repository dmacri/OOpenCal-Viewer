/** @file PrecompiledHeaderCommand.h
 * @brief The `--buildPrecompiledHeader[=<dir>]` helper mode of the application.
 *
 * It builds the precompiled header which speeds up compilation of models (see doc/PRECOMPILED_HEADER.md) and exits.
 * It is run by the release workflow on a machine without a display, so it must not start the GUI:
 * QApplication needs a display, therefore the mode has to be recognized before QApplication is created. */

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace viz::cli
{
/// The command line option which switches the application to this mode
inline constexpr char BUILD_PRECOMPILED_HEADER_OPTION[] = "--buildPrecompiledHeader";

/// Looks for `--buildPrecompiledHeader` or `--buildPrecompiledHeader=<dir>` among the arguments.
/// Returns the directory ("" if none was given) when the option is present.
inline std::optional<std::string> findBuildPrecompiledHeaderRequest(int argc, const char* const argv[])
{
    constexpr std::string_view option = BUILD_PRECOMPILED_HEADER_OPTION;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == option)
            return std::string();
        if (arg.size() > option.size() && arg.substr(0, option.size()) == option && arg[option.size()] == '=')
            return std::string(arg.substr(option.size() + 1));
    }
    return std::nullopt;
}

/// If the command line asks for the helper mode, runs it (without any GUI) and returns the exit code of the application:
/// 0 on success, 1 if building failed, 2 if no directory is known. Otherwise returns nothing and the application starts normally.
std::optional<int> runBuildPrecompiledHeaderCommand(int& argc, char* argv[]);
} // namespace viz::cli
