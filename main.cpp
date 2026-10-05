/** @file main.cpp
 * @brief Main entry point for the OOpenCal-Visualiser application.
 *
 * This file initializes the Qt application, sets up the main window,
 * handles command-line arguments for loading initial simulation directories,
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
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <clocale>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>

#include "mainwindow.h"
#include "core/CommandLineParser.h"
#include "plugins/PluginLoader.h"
#include "data/PerformanceMetrics.h"
#include "plugins/CompilationConfig.h"
#include "plugins/CppModuleBuilder.h"
#include "plugins/ModelLoader.h"


void applyStyleSheet(MainWindow& mainWindow);


namespace
{
/// Looks for --buildPrecompiledHeader[=<dir>]. Returns the directory ("" if none was given) when the option is present.
std::optional<std::string> findBuildPrecompiledHeaderRequest(int argc, char* argv[])
{
    constexpr std::string_view option = CommandLineParser::ARG_BUILD_PRECOMPILED_HEADER;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == option)
            return std::string();
        if (arg.size() > option.size() && arg.substr(0, option.size()) == option && arg[option.size()] == '=')
            return std::string(arg.substr(option.size() + 1));
    }
    return std::nullopt;
}

/// Builds the precompiled header used to speed up compilation of models. Returns the exit code of the application.
int buildPrecompiledHeader(const std::string& requestedDirectory)
{
    const std::string directory = requestedDirectory.empty()
                                      ? viz::plugins::CompilationConfig::getInstance().getPrecompiledHeaderDir()
                                      : requestedDirectory;
    if (directory.empty())
    {
        std::cerr << "Error: no directory for the precompiled header. Pass "
                  << CommandLineParser::ARG_BUILD_PRECOMPILED_HEADER
                  << "=<directory> or set OOPENCAL_PRECOMPILED_HEADER_DIR." << std::endl;
        return 2;
    }

    viz::plugins::CppModuleBuilder builder;
    std::cout << "Building the precompiled header in '" << directory << "'" << std::endl;

    const auto result = builder.buildPrecompiledHeader(directory);
    if (! result.success)
    {
        std::cerr << "✗ Building the precompiled header failed (exit code " << result.exitCode << ")\n"
                  << result.stdErr << std::endl;
        return 1;
    }

    std::cout << "✓ Precompiled header is ready: " << result.outputFile << std::endl;
    return 0;
}
} // namespace

int main(int argc, char* argv[])
{
    // Helper mode without any GUI (and without a display): build the precompiled header used by model compilation
    if (const auto precompiledHeaderRequest = findBuildPrecompiledHeaderRequest(argc, argv))
    {
        QCoreApplication coreApplication(argc, argv);
        return buildPrecompiledHeader(*precompiledHeaderRequest);
    }

    // vtkObject::GlobalWarningDisplayOff();

    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication a(argc, argv);

    // On Unix Qt calls setlocale(LC_ALL, ""), so e.g. a Polish system would make the printf/strtod calls inside
    // the model (Cell) classes print and parse "1008,5" instead of "1008.5". The application is in English, so
    // keep numbers in the C format. Qt widgets format numbers through QLocale and are not affected.
    std::setlocale(LC_NUMERIC, "C");
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

    // Load model directory if provided
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
                std::cerr << "Only simulation directories are supported as input. "
                          << "Please pass the directory containing Header.txt instead of the Header.txt file itself: '"
                          << path << "'" << std::endl;
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
