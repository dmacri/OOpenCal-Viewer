/** @file CompilationSettingsWidget.h
 * @brief Widget for displaying and managing C++ module compilation settings.
 *
 * This file contains the CompilationSettingsWidget class which provides a user interface
 * for viewing and potentially modifying C++ compilation options. The widget displays:
 * - C++ standard settings
 * - Compiler configuration
 * - Compilation flags
 * - Include paths (OOpenCAL and project paths)
 * - Environment variables and their values
 *
 * The widget integrates with CppModuleBuilder to access current compilation
 * configuration and provides a read-only view of settings (with future extension
 * capabilities for editing). */

#pragma once

#include <QWidget>
#include <memory>

namespace Ui
{
class CompilationSettingsWidget;
}

namespace viz::plugins
{
class CppModuleBuilder;
}

/** @class CompilationSettingsWidget
 * @brief Widget for displaying C++ module compilation settings.
 *
 * This class provides a comprehensive interface for viewing compilation settings
 * used by the CppModuleBuilder. It displays current compiler configuration,
 * compilation flags, include paths, and environment variables in an organized
 * and user-friendly manner.
 *
 * The widget is designed to be extendable for future editing capabilities
 * while currently providing a read-only view of the compilation configuration.
 *
 * Key features:
 * - Display current C++ standard
 * - Show compiler path and availability
 * - List compilation flags (-shared, -fPIC, -std=)
 * - Show OOpenCAL include paths
 * - Display project include paths
 * - Show environment variables and their values
 * - Refresh settings on demand
 *
 * @note The widget uses Qt's model-view framework for displaying environment
 * variables and provides automatic refresh capabilities. */
class CompilationSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CompilationSettingsWidget(QWidget* parent = nullptr);
    ~CompilationSettingsWidget();

    /** @brief Set the CppModuleBuilder instance to get settings from
     * @param builder Shared pointer to the module builder */
    void setModuleBuilder(std::shared_ptr<viz::plugins::CppModuleBuilder> builder);

    /** @brief Refresh all displayed settings from the current builder configuration */
    void refreshSettings();

private slots:
    /** @brief Handle refresh button click */
    void onRefreshClicked();
    
    /** @brief Handle environment variable selection change */
    void onEnvironmentVariableSelectionChanged();

private:
    /** @brief Initialize UI components and connections */
    void setupConnections();
    
    /** @brief Load and display compilation settings */
    void loadCompilationSettings();
    
    /** @brief Load and display environment variables */
    void loadEnvironmentVariables();
    
    /** @brief Update compiler status display */
    void updateCompilerStatus();
    
    /** @brief Detect and display C++ standard */
    void detectCppStandard();
    
    /** @brief Generate example compilation command from current settings */
    QString generateExampleCommand();

private:
    Ui::CompilationSettingsWidget* ui;
    std::shared_ptr<viz::plugins::CppModuleBuilder> m_moduleBuilder;
    QString m_currentCompiler;
    QString m_currentStandard;
};
