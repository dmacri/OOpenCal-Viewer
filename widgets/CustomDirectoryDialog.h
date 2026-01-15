#pragma once

#include <QDialog>
#include <QFileSystemModel>
#include <QIcon>
#include <QSortFilterProxyModel>
#include <QMap>

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
    struct HeaderInfo
    {
        int numberNodeX = -1;
        int numberNodeY = -1;
        QString mode; // "binary" or "text"
        bool isValid = false;
    };

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
    HeaderInfo parseHeaderFile(const QString &headerPath) const;
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
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
        bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
        
        void setDirectoryIcon(const QString &path, const QIcon &icon);
        void setDirectoryEnabled(const QString &path, bool enabled);
        void setHeaderInfo(const QString &path, const HeaderInfo &info);
        
    private:
        QMap<QString, QIcon> m_customIcons;
        QMap<QString, bool> m_enabledState;
        QMap<QString, HeaderInfo> m_headerInfo;
    };

    CustomFileSystemModel *m_fileSystemModel;
    DirectorySortProxy *m_sortProxy;
    QString m_selectedDirectory;
    
    // Icons for different directory types
    QIcon m_greenFolderIcon;
    QIcon m_defaultFolderIcon;
    QIcon m_grayFolderIcon;    
};
