/** @file ApplicationName.h
 * @brief Name of the application. It is also the key of QSettings, so the GUI and the modes without GUI have to use the same. */

#pragma once

/** Value for `QCoreApplication::setApplicationName()`. */
inline constexpr char APPLICATION_NAME[] = "OOpenCal-Visualiser";
