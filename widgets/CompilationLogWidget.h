/** @file CompilationLogWidget.h
 * @brief Widget for displaying compilation logs and errors.
 *
 * This widget displays compilation results including errors, warnings, and
 * other compiler output in a user-friendly format. */

#pragma once

#include <QDialog>
#include <QString>

namespace Ui
{
class CompilationLogWidget;
}

namespace viz::plugins
{
struct CompilationResult;
}

/** @class CompilationLogWidget
 * @brief Dialog widget for displaying C++ module compilation results.
 *
 * This widget shows:
 * - Compilation status (success/failure)
 * - Exit code
 * - Source file and output file paths
 * - Compilation command used
 * - Compiler stdout and stderr output
 * - Error highlighting and formatting */
class CompilationLogWidget : public QDialog
{
    Q_OBJECT

public:
    explicit CompilationLogWidget(QWidget* parent = nullptr);
    ~CompilationLogWidget();

    /** @brief Display compilation results
     * @param result The compilation result to display */
    void displayCompilationResult(const viz::plugins::CompilationResult& result);

    /** @brief Clear all displayed content */
    void clearLog();

private:
    Ui::CompilationLogWidget* ui;

    /** @brief Format the compilation result for display */
    QString formatResult(const viz::plugins::CompilationResult& result);

    /** @brief Format error output with highlighting */
    QString formatErrorOutput(const std::string& errorText);
};
