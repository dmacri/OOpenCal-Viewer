/** @file ColorSettings.h
 * @brief Provides a singleton class for managing application-wide color settings.
 *
 * This file contains the ColorSettings class which implements the Singleton pattern
 * to provide centralized management of color settings throughout the application.
 * It handles loading, saving, and notifying about color changes for various UI elements.
 *
 * The class provides access to common color settings like background, text, grid,
 * and highlight colors, with built-in persistence using QSettings.
 *
 * @note This class follows the Singleton pattern to ensure consistent color management across the entire application. */

#pragma once

#include <QColor>
#include <QObject>

/** @class ColorSettings
 * @brief Singleton class managing application-wide color settings.
 *
 * This class provides a centralized way to manage and access color settings
 * throughout the application. It implements the Singleton pattern to ensure
 * consistent color management and provides signals to notify about color changes.
 *
 * The class handles:
 * - Persistent storage of color settings
 * - Default color definitions
 * - Change notifications
 * - Thread-safe access to color values
 *
 * @note All color changes are automatically saved to persistent storage (handled by Qt QSettings). */
class ColorSettings : public QObject
{
    Q_OBJECT

public:
    static ColorSettings& instance();

    // Getters
    QColor backgroundColor() const
    {
        return backgroundColor_;
    }
    QColor textColor() const
    {
        return textColor_;
    }
    QColor gridColor() const
    {
        return gridColor_;
    }
    QColor highlightColor() const
    {
        return highlightColor_;
    }
    QColor flatSceneBackgroundColor() const
    {
        return flatSceneBackgroundColor_;
    }

    // Setters
    void setBackgroundColor(const QColor& color);
    void setTextColor(const QColor& color);
    void setGridColor(const QColor& color);
    void setHighlightColor(const QColor& color);
    void setFlatSceneBackgroundColor(const QColor& color);

    // Save/load settings
    void saveSettings();
    void loadSettings();

    // Default colors
    static inline const QColor DEFAULT_BACKGROUND{ Qt::gray };
    static inline const QColor DEFAULT_TEXT{ Qt::black };
    static inline const QColor DEFAULT_GRID{ Qt::red };
    static inline const QColor DEFAULT_HIGHLIGHT{ Qt::yellow };
    static inline const QColor DEFAULT_FLAT_SCENE_BACKGROUND{ 204, 204, 204 };  // Light gray (0.8, 0.8, 0.8)

signals:
    void colorsChanged();

private:
    // Delete copy constructor and assignment operator
    ColorSettings(const ColorSettings&) = delete;
    ColorSettings& operator=(const ColorSettings&) = delete;

    ColorSettings(QObject* parent = nullptr);
    ~ColorSettings() = default;

    QColor backgroundColor_;
    QColor textColor_;
    QColor gridColor_;
    QColor highlightColor_;
    QColor flatSceneBackgroundColor_;
};
