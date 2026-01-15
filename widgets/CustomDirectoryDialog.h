/** @file CustomDirectoryDialog.h
 * @brief Declaration of the CustomDirectoryDialog class.
 *
 * This file contains the declaration of CustomDirectoryDialog, a custom Qt dialog
 * used for browsing and selecting directories based on the presence of a Header.txt file.
 *
 * The dialog displays a directory tree with visual indicators:
 * - Green folders indicate directories containing a Header.txt file.
 * - Default folders indicate directories containing subdirectories.
 * - Gray folders indicate empty or non-selectable directories.
 *
 * Additional metadata extracted from Header.txt files (X, Y, M values)
 * is displayed in dedicated columns. */

#pragma once

#include <QDialog>
#include <QIcon>

class QLabel;
class QCheckBox;
class QPushButton;
class QTreeView;
class QTreeView;

namespace Ui {
class CustomDirectoryDialog;
}

/** @class CustomDirectoryDialog
 * @brief Dialog for selecting a directory based on custom content rules.
 *
 * CustomDirectoryDialog provides a tree-based directory browser that allows users
 * to select directories containing a specific configuration file (Header.txt)
 * or directories that contain further subdirectories.
 *
 * The dialog enhances usability by:
 * - Displaying custom icons depending on directory contents
 * - Disabling empty directories
 * - Showing metadata extracted from Header.txt files
 * - Sorting directories by relevance and name
 *
 * The selected directory can be retrieved after the dialog is accepted. */
class CustomDirectoryDialog : public QDialog
{
    Q_OBJECT

public:
    /// Constructs the directory selection dialog
    explicit CustomDirectoryDialog(QWidget *parent = nullptr);

    /// Destroys the dialog and releases all associated resources
    ~CustomDirectoryDialog();

    /// Returns the currently selected directory path
    QString getSelectedDirectory() const;

    /// Sets the initial directory displayed and selected in the tree view
    void setStartDirectory(const QString &path);

private slots:
    /// Handles single-click events on the directory tree view
    void onTreeViewClicked(const QModelIndex &index);

    /// Handles double-click events on the directory tree view
    void onTreeViewDoubleClicked(const QModelIndex &index);

    /// Toggles the visibility of hidden directories
    void onHiddenDirectoriesToggled(bool checked);

    /// Validates the selection and accepts the dialog
    void onOkButtonClicked();

    /// Cancels the dialog without making a selection
    void onCancelButtonClicked();

private:
    /// Proxy model used to apply custom directory sorting rules
    class DirectorySortProxy;
    /// Custom file system model providing icons, states, and metadata
    class CustomFileSystemModel;

    /// Structure holding information parsed from a Header.txt file
    struct HeaderInfo;

    /** @enum DirectoryType
     * @brief Describes the classification of a directory. */
    enum class DirectoryType
    {
        WithHeader,  ///< Directory contains a Header.txt file (green icon).
        WithSubdirs, ///< Directory contains subdirectories but no Header.txt file.
        Empty,       ///< Directory contains no subdirectories and no Header.txt file.
        Unknown      ///< Directory state has not yet been determined.
    };

    /// Initializes UI-related settings and connections
    void setupUI();

    /// Initializes folder icons used by the dialog
    void setupIcons();

    /// Analyzes a directory and determines its type
    DirectoryType analyzeDirectory(const QString &path) const;

    /// Checks whether a directory directly contains a Header.txt file
    bool directoryHasHeaderDirectly(const QString &path) const;

    /// Parses a Header.txt file and extracts relevant metadata
    HeaderInfo parseHeaderFile(const QString &headerPath) const;

    /// Updates the icon, enabled state, and metadata for a single directory
    void updateDirectoryAppearance(const QString &path);

    /// Updates appearance of all currently visible directories
    void updateVisibleDirectoriesAppearance();
    
    /// Updates UI widgets with information about source and compiled files
    void updateModuleInfo(const QString &directoryPath);
    
    /// Gets modification date of a file
    QDateTime getFileModificationDate(const QString &filePath) const;

    /// Clears all module information widgets
    void clearModuleInfo();

    /// Recursively updates directory appearance starting from a given model index
    void updateDirectoriesRecursive(const QModelIndex &parentIndex);

    /// Determines whether a directory can be selected by the user
    bool isDirectorySelectable(const QString &path) const;

private:
    /// Generated UI object
    Ui::CustomDirectoryDialog *ui;

    /// File system model providing directory data
    CustomFileSystemModel *m_fileSystemModel;

    /// Proxy model responsible for sorting directories
    DirectorySortProxy *m_sortProxy;

    /// Currently selected directory path
    QString m_selectedDirectory;

    /// Icon used for directories containing Header.txt
    QIcon m_greenFolderIcon;

    /// Icon used for standard directories
    QIcon m_defaultFolderIcon;

    /// Icon used for empty or disabled directories
    QIcon m_grayFolderIcon;
};
