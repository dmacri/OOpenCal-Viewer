#pragma once

#include <chrono>
#include <string>
#include <memory>

/** @brief RAII guard for showing wait cursor with optional status message.
 * 
 * This class automatically shows a wait cursor when constructed and restores
 * the normal cursor when destroyed. Optionally displays a status message under
 * the cursor. Tracks elapsed time since construction.
 * 
 * Usage:
 * @code
 * {
 *     WaitCursorGuard guard("Loading model...");
 *     // ... long operation ...
 *     auto elapsed = guard.elapsedMilliseconds();
 *     std::cout << "Operation took " << elapsed << " ms" << std::endl;
 * } // Cursor automatically restored here
 * @endcode
 */
class WaitCursorGuard
{
public:
    /// @brief This method is to change icon, it is not restoring icon automatically
    static void changeIcon(bool waitingIcon);

    /** @brief Construct and show wait cursor.
     * 
     * @param statusMessage Optional message to display under cursor.
     *                      If empty, no message is shown. */
    explicit WaitCursorGuard(const std::string& statusMessage = "", bool restoneOnDestroy=true);

    /** @brief Destructor - restores normal cursor and hides message. */
    ~WaitCursorGuard();

    // Prevent copying
    WaitCursorGuard(const WaitCursorGuard&) = delete;
    WaitCursorGuard& operator=(const WaitCursorGuard&) = delete;

    // Allow moving
    WaitCursorGuard(WaitCursorGuard&&) noexcept;
    WaitCursorGuard& operator=(WaitCursorGuard&&) noexcept;

    /** @brief Get elapsed time in milliseconds since construction.
     * 
     * @return Milliseconds elapsed since this guard was created */
    long long elapsedMilliseconds() const;

    /** @brief Get elapsed time in seconds since construction.
     * 
     * @return Seconds elapsed since this guard was created */
    double elapsedSeconds() const;

private:
    std::chrono::steady_clock::time_point startTime;
    bool isActive = false;

    bool restoneOnDestroy = true; // when operations are separated we don't want to restore default

    // Forward declaration to hide Qt dependency
    class StatusMessageImpl;
    std::unique_ptr<StatusMessageImpl> statusMessage;
};
