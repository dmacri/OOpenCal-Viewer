/** @file ProcessMemory.h
 *  @brief Qt/VTK-free query of the memory used by the running process (used by the --benchmark command-line mode). */

#pragma once

#include <cctype>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>

namespace ProcessMemory
{
/** @brief Reads the peak resident set size (the `VmHWM` line) from the text of `/proc/<pid>/status`.
 *  @param statusText Content of the status file, e.g. a line `VmHWM:\t  123456 kB`
 *  @return The peak size in megabytes or std::nullopt when there is no such line or its value is not a number */
inline std::optional<double> peakResidentMegabytesFromProcStatus(std::string_view statusText)
{
    constexpr std::string_view key = "VmHWM:";

    while (!statusText.empty())
    {
        const std::size_t lineEnd = statusText.find('\n');
        std::string_view line = statusText.substr(0, lineEnd);
        statusText.remove_prefix(lineEnd == std::string_view::npos ? statusText.size() : lineEnd + 1);

        if (!line.starts_with(key))
            continue;

        line.remove_prefix(key.size());
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front())))
            line.remove_prefix(1);

        unsigned long long kilobytes = 0; // the value in the status file is always in kB
        const auto [end, error] = std::from_chars(line.data(), line.data() + line.size(), kilobytes);
        if (error != std::errc{})
            return std::nullopt;
        return static_cast<double>(kilobytes) / 1024.0;
    }
    return std::nullopt;
}

/** @brief Peak size of the memory held in RAM by this process, in megabytes (resident set size, high water mark).
 *  @return The size, or std::nullopt when the system does not tell it (only Linux is supported at the moment) */
inline std::optional<double> peakResidentMegabytes()
{
#ifdef __linux__
    // The files of /proc report size 0, so they have to be read until the end instead of by the size of the file
    std::ifstream status("/proc/self/status");
    if (status)
    {
        std::ostringstream text;
        text << status.rdbuf();
        return peakResidentMegabytesFromProcStatus(text.str());
    }
#endif
    return std::nullopt;
}
} // namespace ProcessMemory
