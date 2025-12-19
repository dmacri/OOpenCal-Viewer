#include "WaitCursorGuard.h"

#include <QApplication>
#include <QToolTip>
#include <QCursor>
#include <QString>

void WaitCursorGuard::changeIcon(bool waitingIcon)
{
    if (waitingIcon)
        QApplication::setOverrideCursor(Qt::WaitCursor);
    else
        QApplication::restoreOverrideCursor();
}

WaitCursorGuard::WaitCursorGuard(const std::string& statusMessage)
    : startTime(std::chrono::steady_clock::now()),
    isActive(true),
    message(statusMessage)
{
    changeIcon(true);

    if (!message.empty())
        QToolTip::showText(QCursor::pos(), QString::fromStdString(message));
}

WaitCursorGuard::~WaitCursorGuard()
{
    if (isActive)
    {
        if (!message.empty())
            QToolTip::hideText();

        changeIcon(false);
        isActive = false;
    }
}

WaitCursorGuard::WaitCursorGuard(WaitCursorGuard&& other) noexcept
    : startTime(other.startTime),
    isActive(other.isActive),
    message(std::move(other.message))
{
    other.isActive = false;
}

WaitCursorGuard& WaitCursorGuard::operator=(WaitCursorGuard&& other) noexcept
{
    if (this != &other)
    {
        if (isActive)
        {
            if (!message.empty())
                QToolTip::hideText();
            changeIcon(false);
        }

        startTime = other.startTime;
        isActive = other.isActive;
        message = std::move(other.message);

        other.isActive = false;
    }
    return *this;
}

long long WaitCursorGuard::elapsedMilliseconds() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - startTime)
        .count();
}

double WaitCursorGuard::elapsedSeconds() const
{
    return elapsedMilliseconds() / 1000.0;
}
