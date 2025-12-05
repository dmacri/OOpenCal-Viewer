#include "ColorSettings.h"

#include <QSettings>


ColorSettings& ColorSettings::instance()
{
    static ColorSettings instance;
    return instance;
}

ColorSettings::ColorSettings(QObject* parent)
    : QObject(parent)
    , backgroundColor_(DEFAULT_BACKGROUND)
    , textColor_(DEFAULT_TEXT)
    , gridColor_(DEFAULT_GRID)
    , highlightColor_(DEFAULT_HIGHLIGHT)
    , flatSceneBackgroundColor_(DEFAULT_FLAT_SCENE_BACKGROUND)
{
    loadSettings();
}

void ColorSettings::setBackgroundColor(const QColor& color)
{
    if (backgroundColor_ != color)
    {
        backgroundColor_ = color;
        emit colorsChanged();
    }
}

void ColorSettings::setTextColor(const QColor& color)
{
    if (textColor_ != color)
    {
        textColor_ = color;
        emit colorsChanged();
    }
}

void ColorSettings::setGridColor(const QColor& color)
{
    if (gridColor_ != color)
    {
        gridColor_ = color;
        emit colorsChanged();
    }
}

void ColorSettings::setHighlightColor(const QColor& color)
{
    if (highlightColor_ != color)
    {
        highlightColor_ = color;
        emit colorsChanged();
    }
}

void ColorSettings::setFlatSceneBackgroundColor(const QColor& color)
{
    if (flatSceneBackgroundColor_ != color)
    {
        flatSceneBackgroundColor_ = color;
        emit colorsChanged();
    }
}

void ColorSettings::saveSettings()
{
    QSettings settings;
    settings.beginGroup("Colors");
    settings.setValue("background", backgroundColor_);
    settings.setValue("text", textColor_);
    settings.setValue("grid", gridColor_);
    settings.setValue("highlight", highlightColor_);
    settings.setValue("flatSceneBackground", flatSceneBackgroundColor_);
    settings.endGroup();
}

void ColorSettings::loadSettings()
{
    QSettings settings;
    settings.beginGroup("Colors");

    backgroundColor_ = settings.value("background", DEFAULT_BACKGROUND).value<QColor>();
    textColor_ = settings.value("text", DEFAULT_TEXT).value<QColor>();
    gridColor_ = settings.value("grid", DEFAULT_GRID).value<QColor>();
    highlightColor_ = settings.value("highlight", DEFAULT_HIGHLIGHT).value<QColor>();
    flatSceneBackgroundColor_ = settings.value("flatSceneBackground", DEFAULT_FLAT_SCENE_BACKGROUND).value<QColor>();

    settings.endGroup();

    emit colorsChanged();
}
