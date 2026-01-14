#pragma once

#include <QDialog>
#include <QFileSystemModel>
#include <QIcon>
#include <QSortFilterProxyModel>

class QLabel;
class QCheckBox;
class QPushButton;
class QTreeView;
class QTreeView;

namespace Ui {
class CustomDirectoryDialog;
}

class CustomDirectoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomDirectoryDialog(QWidget *parent = nullptr);
    ~CustomDirectoryDialog();

    QString getSelectedDirectory() const;
    void setStartDirectory(const QString &path);

private slots:
    void onTreeViewClicked(const QModelIndex &index);
    void onTreeViewDoubleClicked(const QModelIndex &index);
    void onHiddenDirectoriesToggled(bool checked);
    void onOkButtonClicked();
    void onCancelButtonClicked();

private:
    enum class DirectoryType
    {
        WithHeader,      // Contains Header.txt - green icon
        WithSubdirs,     // Contains subdirectories but no Header.txt - default icon
        Empty,           // No subdirectories and no Header.txt - gray icon (disabled)
        Unknown          // Not yet checked
    };

    class DirectorySortProxy : public QSortFilterProxyModel
    {
    public:
        explicit DirectorySortProxy(QObject *parent = nullptr);
        
    protected:
        bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    };

    void setupUI();
    void setupIcons();
    DirectoryType analyzeDirectory(const QString &path) const;
    bool directoryHasHeaderDirectly(const QString &path) const;
    void updateDirectoryAppearance(const QString &path);
    void updateVisibleDirectoriesAppearance();
    void updateDirectoriesRecursive(const QModelIndex &parentIndex);
    bool isDirectorySelectable(const QString &path) const;
    
    Ui::CustomDirectoryDialog *ui;

    class CustomFileSystemModel : public QFileSystemModel
    {
    public:
        explicit CustomFileSystemModel(QObject *parent = nullptr);
        
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        
        void setDirectoryIcon(const QString &path, const QIcon &icon);
        void setDirectoryEnabled(const QString &path, bool enabled);
        
    private:
        QMap<QString, QIcon> m_customIcons;
        QMap<QString, bool> m_enabledState;
    };

    CustomFileSystemModel *m_fileSystemModel;
    DirectorySortProxy *m_sortProxy;
    QString m_selectedDirectory;
    
    // Icons for different directory types
    QIcon m_greenFolderIcon;
    QIcon m_defaultFolderIcon;
    QIcon m_grayFolderIcon;    
};
