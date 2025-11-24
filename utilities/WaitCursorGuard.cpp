#include "WaitCursorGuard.h"
#include <QApplication>
#include <QToolTip>

/** @brief Implementation detail for status message display.
 * 
 * This inner class handles Qt-specific functionality to keep Qt dependencies
 * out of the public header. */
class WaitCursorGuard::StatusMessageImpl
{
public:
    explicit StatusMessageImpl(const std::string& message)
        : message(message)
    {
        if (!message.empty())
        {
            showMessage();
        }
    }

    ~StatusMessageImpl()
    {
        if (!message.empty())
        {
            QToolTip::hideText();
        }
    }

    void showMessage()
    {
        if (!message.empty())
        {
            QToolTip::showText(QCursor::pos(), QString::fromStdString(message));
        }
    }

private:
    std::string message;
};

void WaitCursorGuard::changeIcon(bool waitingIcon)
{
    if (waitingIcon)
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    }
    else
    {
        QApplication::restoreOverrideCursor();
    }
}

WaitCursorGuard::WaitCursorGuard(const std::string& statusMessageText, bool restoneOnDestroy)
    : startTime(std::chrono::steady_clock::now()), isActive(true), restoneOnDestroy(restoneOnDestroy)
{
    WaitCursorGuard::changeIcon(/*waitingIcon=*/true);
    
    if (! statusMessageText.empty())
    {
        statusMessage = std::make_unique<StatusMessageImpl>(statusMessageText);
    }
}

WaitCursorGuard::~WaitCursorGuard()
{
    if (isActive && restoneOnDestroy)
    {
        statusMessage.reset();  // Destroy status message first
        WaitCursorGuard::changeIcon(/*waitingIcon=*/false);
        isActive = false;
    }
}

WaitCursorGuard::WaitCursorGuard(WaitCursorGuard&& other) noexcept
    : startTime(other.startTime), isActive(other.isActive), statusMessage(std::move(other.statusMessage))
{
    other.isActive = false;  // Prevent double cleanup
}

WaitCursorGuard& WaitCursorGuard::operator=(WaitCursorGuard&& other) noexcept
{
    if (this != &other)
    {
        // Clean up current state
        if (isActive)
        {
            statusMessage.reset();
            WaitCursorGuard::changeIcon(/*waitingIcon=*/false);
        }

        // Move from other
        startTime = other.startTime;
        isActive = other.isActive;
        statusMessage = std::move(other.statusMessage);
        other.isActive = false;
    }
    return *this;
}

long long WaitCursorGuard::elapsedMilliseconds() const
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
}

double WaitCursorGuard::elapsedSeconds() const
{
    return elapsedMilliseconds() / 1000.0;
}
