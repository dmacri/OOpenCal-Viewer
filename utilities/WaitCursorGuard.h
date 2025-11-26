#pragma once

#include <chrono>
#include <string>

/** @brief RAII guard for showing wait cursor with optional status message.
 *
 * Shows a wait cursor on construction and restores the normal cursor on
 * destruction. Optionally displays a tooltip message. Tracks elapsed time.
 * Usage:
 * @code
 * {
 *     WaitCursorGuard guard("Loading model...");
 *     // ... long operation ...
 *     auto elapsed = guard.elapsedMilliseconds();
 *     std::cout << "Operation took " << elapsed << " ms" << std::endl;
 * } // Cursor automatically restored here
 * @endcode */
class WaitCursorGuard
{
public:
    /// @brief Manually change the cursor icon (does *not* automatically restore)
    static void changeIcon(bool waitingIcon);

    /** @brief Construct and show wait cursor and optional message. */
    explicit WaitCursorGuard(const std::string& statusMessage = "");

    /** @brief Destructor restores cursor and hides message. */
    ~WaitCursorGuard();

    // Non-copyable
    WaitCursorGuard(const WaitCursorGuard&) = delete;
    WaitCursorGuard& operator=(const WaitCursorGuard&) = delete;

    // Movable
    WaitCursorGuard(WaitCursorGuard&& other) noexcept;
    WaitCursorGuard& operator=(WaitCursorGuard&& other) noexcept;

    long long elapsedMilliseconds() const;
    double elapsedSeconds() const;

private:
    std::chrono::steady_clock::time_point startTime;
    bool isActive = false;

    // Qt-specific message is stored only as a string.
    std::string message;
};
