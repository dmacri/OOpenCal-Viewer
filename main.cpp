/** @file main.cpp
 * @brief Main entry point for the OOpenCal-Visualiser application.
 *
 * This file initializes the Qt application, sets up the main window,
 * handles command-line arguments for loading initial configurations,
 * and loads model plugins from the plugins directory.
 *
 * @mainpage OOpenCal-Visualiser
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
#include "core/CommandLineParser.h"
#include "plugins/PluginLoader.h"
#include "data/PerformanceMetrics.h"


void applyStyleSheet(MainWindow& mainWindow);


int main(int argc, char* argv[])
{
    // vtkObject::GlobalWarningDisplayOff();

    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication a(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setApplicationName("OOpenCal-Visualiser");

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

    // Configure performance metrics based on command-line flag
    PerformanceMetrics::setEnabled(cmdParser.areMetricsEnabled());

    // Set metrics reporting mode
    const std::string& mode = cmdParser.getMetricsMode();
    if (mode == "summary")
    {
        PerformanceMetrics::setReportingMode(PerformanceMetrics::ReportingMode::SummaryOnly);
    }
    else if (mode == "steps")
    {
        PerformanceMetrics::setReportingMode(PerformanceMetrics::ReportingMode::StepsOnly);
    }
    else if (mode == "all")
    {
        PerformanceMetrics::setReportingMode(PerformanceMetrics::ReportingMode::All);
    }
    // "none" is handled by disabling metrics above

    // Load custom model plugins if specified
    for (const auto& modelPath : cmdParser.getLoadModelPaths())
    {
        if (! pluginLoader.loadPlugin(modelPath))
        {
            std::cerr << "Warning: Failed to load plugin: " << modelPath << std::endl;
        }
    }

    MainWindow mainWindow;

    // Load configuration file or model directory if provided
    if (cmdParser.getConfigFile())
    {
        const auto& path = cmdParser.getConfigFile().value();
        if (std::filesystem::exists(path))
        {
            if (cmdParser.isModelDirectory())
            {
                // Load model from directory
                mainWindow.loadModelFromDirectory(QString::fromStdString(path));
            }
            else
            {
                // Load configuration from file
                mainWindow.openConfigurationFile(QString::fromStdString(path));
            }
        }
        else
        {
            std::cerr << "Path not found: '" << path << "'" << std::endl;
        }
    }

    applyStyleSheet(mainWindow);

    // Show window (unless in headless mode)
    if (! cmdParser.getGenerateMoviePath() && ! cmdParser.getGenerateImagePath())
    {
        mainWindow.show();
    }

    // Apply command-line options AFTER show() and before event loop
    // This ensures timers and event handlers are properly initialized
    mainWindow.applyCommandLineOptions(cmdParser);

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
