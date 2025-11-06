/** @file main.cpp
 * @brief Main entry point for the Qt-VTK Viewer application.
 *
 * This file initializes the Qt application, sets up the main window,
 * handles command-line arguments for loading initial configurations,
 * and loads model plugins from the plugins directory.
 *
 * @mainpage Qt-VTK Viewer
 * @tableofcontents
 *
 * @section intro_sec Introduction
 * A Qt-based application for visualizing VTK data with a user-friendly interface.
 *
 * @section features_sec Features
 * - Load and visualize VTK data files
 * - Interactive 3D visualization
 * - Support for multiple model types (runtime switchable)
 * - Plugin system for custom models (no recompilation needed)
 * - Video export functionality
 *
 * @include README.md */

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <filesystem>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>

#include "mainwindow.h"
#include "utilities/CommandLineParser.h"
#include "utilities/PluginLoader.h"


void applyStyleSheet(MainWindow& mainWindow);


int main(int argc, char* argv[])
{
    // vtkObject::GlobalWarningDisplayOff();

    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication a(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setApplicationName("Visualiser");

    // Load plugins from standard locations
    // This happens before MainWindow creation so models are available immediately
    PluginLoader& pluginLoader = PluginLoader::instance();
    pluginLoader.loadFromStandardDirectories({
        "./plugins",      // Current directory
        "../plugins",     // Parent directory
        "./build/plugins" // Build directory
    });

    // Parse command-line arguments
    CommandLineParser cmdParser;
    if (! cmdParser.parse(argc, argv))
    {
        return 1; // Parsing failed
    }

    // Load custom model plugins if specified
    for (const auto& modelPath : cmdParser.getLoadModelPaths())
    {
        if (! pluginLoader.loadPlugin(modelPath))
        {
            std::cerr << "Warning: Failed to load plugin: " << modelPath << std::endl;
        }
    }

    MainWindow mainWindow;
    mainWindow.setSilentMode(cmdParser.isSilentMode());

    // Load configuration file if provided
    if (cmdParser.getConfigFile())
    {
        const auto& configFile = cmdParser.getConfigFile().value();
        if (std::filesystem::exists(configFile))
        {
            mainWindow.openConfigurationFile(QString::fromStdString(configFile));
        }
        else
        {
            std::cerr << "Configuration file not found: '" << configFile << "'" << std::endl;
        }
    }

    applyStyleSheet(mainWindow);

    mainWindow.applyCommandLineOptions(cmdParser);

    // Show window (unless in headless mode)
    if (! cmdParser.getGenerateMoviePath() && ! cmdParser.getGenerateImagePath())
    {
        mainWindow.show();
    }

    return a.exec();
}

void applyStyleSheet(MainWindow& mainWindow)
{
    QFileInfo fi("style.qss");
    if (fi.isFile() && fi.isReadable() && ! fi.isSymLink()) // checking for security CWE-362
    {
        if (QFile styleFile("style.qss"); styleFile.open(QIODevice::ReadOnly))
        {
            mainWindow.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        }
    }
}
