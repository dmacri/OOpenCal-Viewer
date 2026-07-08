/** @file ConfigDetailsDialog.h
 * @brief Declaration of the ConfigDetailsDialog class for displaying configuration details. */

#pragma once

#include <QDialog>
#include <QString>
#include <string>

class QTableWidget;
class QVBoxLayout;
class QLabel;
class QTextBrowser;
class QTabWidget;

/** @class ConfigDetailsDialog
 * @brief A dialog that displays detailed configuration parameters in a table format.
 * 
 * This dialog shows configuration parameters loaded from a file in a user-friendly table view. */
class ConfigDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    /** @brief Constructs a ConfigDetailsDialog with the specified Header.txt and directory details.
     *  @param configFilePath Path to Header.txt to display
     *  @param simulationDetailsHtml Rich HTML with simulation directory details
     * @param parent The parent widget */
    explicit ConfigDetailsDialog(const std::string& configFilePath, const QString& simulationDetailsHtml = QString(), QWidget* parent = nullptr);

    /// @brief Destroys the ConfigDetailsDialog.
    ~ConfigDetailsDialog();

private:
    /// @brief Sets up the user interface components.
    void setupUI();

    /// @brief Loads configuration data from the specified file.
    /// @param configFilePath Path to Header.txt
    void loadConfigData(const std::string& configFilePath);
    void loadSimulationDetails(const QString& simulationDetailsHtml);

    /// @brief Adjusts the dialog size to fit its content (to make sure that all rows are visible)
    void adjustSizeToContent();

    QTableWidget* tableWidget; ///< Table widget for displaying configuration parameters
    QVBoxLayout* mainLayout;   ///< Main layout of the dialog
    QLabel* filePathLabel;     ///< Label displaying the configuration file path
    QTextBrowser* detailsBrowser; ///< Rich text widget for displaying simulation directory details
    QTabWidget* tabWidget;     ///< Tabs for directory details and Header.txt parameters
};
