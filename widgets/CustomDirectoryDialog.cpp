#include <QStandardPaths>
#include <QCommonStyle>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <QDirIterator>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeView>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QDir>

#include "CustomDirectoryDialog.h"
#include "ui_CustomDirectoryDialog.h"
#include "config/Config.h"
#include "config/ConfigCategory.h"
#include "config/ConfigParameter.h"
#include "config/ConfigConstants.h"
#include "core/directoryConstants.h"


// CustomFileSystemModel implementation
CustomDirectoryDialog::CustomFileSystemModel::CustomFileSystemModel(QObject *parent)
    : QFileSystemModel(parent)
{
}

QVariant CustomDirectoryDialog::CustomFileSystemModel::data(const QModelIndex &index, int role) const
{
    if (! index.isValid())
        return QFileSystemModel::data(index, role);
    
    QString path = filePath(index);
    QFileInfo fileInfo(path);
    
    // Handle custom columns (columns 1, 2, 3 for x, y, m)
    if (role == Qt::DisplayRole && index.column() >= 1 && index.column() <= 3 && fileInfo.isDir())
    {
        if (m_headerInfo.contains(path))
        {
            const HeaderInfo &info = m_headerInfo[path];
            if (info.isValid)
            {
                switch (index.column())
                {
                case 1: // x column (number_node_x)
                    return info.numberNodeX > 0 ? QString::number(info.numberNodeX) : QString();
                case 2: // y column (number_node_y)
                    return info.numberNodeY > 0 ? QString::number(info.numberNodeY) : QString();
                case 3: // m column (mode)
                    return info.mode;
                }
            }
        }
        return QString(); // Empty string for directories without header info
    }
    
    // Right-align text for columns 1, 2, 3
    if (role == Qt::TextAlignmentRole && index.column() >= 1 && index.column() <= 3)
    {
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);
    }

    // Only show icons in the first column (column 0)
    if (role == Qt::DecorationRole && fileInfo.isDir() && index.column() == 0)
    {
        if (m_customIcons.contains(path))
        {
            return m_customIcons[path];
        }
    }
    
    // Only show foreground color in the first column
    if (role == Qt::ForegroundRole && fileInfo.isDir() && index.column() == 0)
    {
        if (m_enabledState.contains(path) && !m_enabledState[path])
        {
            return QColor(Qt::gray); // Gray color for disabled directories
        }
    }
    
    return QFileSystemModel::data(index, role);
}

int CustomDirectoryDialog::CustomFileSystemModel::columnCount(const QModelIndex &parent) const
{
    // Return 4 columns: Name, X, Y, M
    Q_UNUSED(parent)
    return 4;
}

QVariant CustomDirectoryDialog::CustomFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return tr("Directory");
        case 1: return tr("X");
        case 2: return tr("Y");
        case 3: return tr("M");
        }
    }
    return QFileSystemModel::headerData(section, orientation, role);
}

void CustomDirectoryDialog::CustomFileSystemModel::setDirectoryIcon(const QString &path, const QIcon &icon)
{
    m_customIcons[path] = icon;
    QModelIndex index = this->index(path);
    emit dataChanged(index, index, {Qt::DecorationRole});
}

void CustomDirectoryDialog::CustomFileSystemModel::setDirectoryEnabled(const QString &path, bool enabled)
{
    m_enabledState[path] = enabled;
    QModelIndex index = this->index(path);
    emit dataChanged(index, index, {Qt::ForegroundRole});
}

void CustomDirectoryDialog::CustomFileSystemModel::setHeaderInfo(const QString &path, const HeaderInfo &info)
{
    m_headerInfo[path] = info;
    QModelIndex index = this->index(path);
    emit dataChanged(index, index, {Qt::DisplayRole});
}

// DirectorySortProxy implementation
CustomDirectoryDialog::DirectorySortProxy::DirectorySortProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
}

bool CustomDirectoryDialog::DirectorySortProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // Get the file system model
    const QFileSystemModel *fsModel = qobject_cast<const QFileSystemModel*>(sourceModel());
    if (! fsModel)
        return QSortFilterProxyModel::lessThan(left, right);
    
    // Get file info for both indices
    QFileInfo leftInfo = fsModel->fileInfo(left);
    QFileInfo rightInfo = fsModel->fileInfo(right);
    
    // Only sort directories
    if (! leftInfo.isDir() || ! rightInfo.isDir())
        return QSortFilterProxyModel::lessThan(left, right);
    
    // Get directory paths
    QString leftPath = leftInfo.absoluteFilePath();
    QString rightPath = rightInfo.absoluteFilePath();
    
    // Analyze directory types
    auto analyzeType = [](const QString &path) -> int {
        QDir dir(path);
        if (! dir.exists())
            return 3; // Unknown - lowest priority
        
        // Check for Header.txt
        if (dir.exists(DirectoryConstants::HEADER_FILE_NAME))
            return 0; // WithHeader - highest priority
        
        // Check for subdirectories
        QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot);
        if (it.hasNext())
            return 1; // WithSubdirs - medium priority
        
        return 2; // Empty - lowest priority
    };
    
    int leftType = analyzeType(leftPath);
    int rightType = analyzeType(rightPath);
    
    // If types are different, sort by type (0=WithHeader, 1=WithSubdirs, 2=Empty)
    if (leftType != rightType)
        return leftType < rightType;
    
    // If types are the same, sort by name case-insensitively
    QString leftName = leftInfo.fileName();
    QString rightName = rightInfo.fileName();
    
    return leftName.compare(rightName, Qt::CaseInsensitive) < 0;
}

CustomDirectoryDialog::CustomDirectoryDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CustomDirectoryDialog)
    , m_fileSystemModel(new CustomFileSystemModel(this))
    , m_sortProxy(new DirectorySortProxy(this))
{
    ui->setupUi(this);

    // Setup icons
    setupIcons();

    // Setup UI elements
    m_fileSystemModel->setRootPath("");
    m_fileSystemModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    // Setup proxy model for sorting
    m_sortProxy->setSourceModel(m_fileSystemModel);
    m_sortProxy->setSortRole(QFileSystemModel::FileNameRole);

    ui->m_treeView->setModel(m_sortProxy);
    ui->m_treeView->setRootIndex(m_sortProxy->mapFromSource(m_fileSystemModel->index(QDir::rootPath())));
    
    // Show header and set custom column headers
    ui->m_treeView->setHeaderHidden(false);
    
    // Configure column widths
    ui->m_treeView->setColumnWidth(0, 400);  // First column (directory tree) - wide
    ui->m_treeView->setColumnWidth(1, 50);   // X column - small
    ui->m_treeView->setColumnWidth(2, 50);   // Y column - small
    ui->m_treeView->setColumnWidth(3, 40);   // M column - smallest
    
    // Set column resize modes
    QHeaderView* header = ui->m_treeView->header();
    header->setSectionResizeMode(0, QHeaderView::Stretch);  // First column stretches to fill
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);  // X column - resize to content
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);  // Y column - resize to content
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);  // M column - resize to content
    
    ui->m_treeView->setSortingEnabled(true);
    ui->m_treeView->sortByColumn(0, Qt::AscendingOrder);
    
    // Connect to directoryLoaded signal to update sorting when directories are loaded
    connect(m_fileSystemModel, &QFileSystemModel::directoryLoaded,
            this, [this](const QString &) {
                ui->m_treeView->sortByColumn(0, Qt::AscendingOrder);
            });
    
    // Expand the tree to show initial structure
    for (int i = 0; i < m_sortProxy->rowCount(ui->m_treeView->rootIndex()); ++i)
    {
        QModelIndex child = m_sortProxy->index(i, 0, ui->m_treeView->rootIndex());
        ui->m_treeView->expand(child);
    }
    
    // Connect signals
    connect(ui->m_treeView, &QTreeView::clicked, this, &CustomDirectoryDialog::onTreeViewClicked);
    connect(ui->m_treeView, &QTreeView::doubleClicked, this, &CustomDirectoryDialog::onTreeViewDoubleClicked);
    connect(ui->m_showHiddenCheckBox, &QCheckBox::toggled, this, &CustomDirectoryDialog::onHiddenDirectoriesToggled);
    connect(ui->m_okButton, &QPushButton::clicked, this, &CustomDirectoryDialog::onOkButtonClicked);
    connect(ui->m_cancelButton, &QPushButton::clicked, this, &CustomDirectoryDialog::onCancelButtonClicked);
    
    // Connect to expanded signal to update icons when directories are expanded
    connect(ui->m_treeView, &QTreeView::expanded, this, [this](const QModelIndex &index) {
        QString parentPath = m_fileSystemModel->filePath(m_sortProxy->mapToSource(index));
        
        // Scan only immediate subdirectories (not recursive)
        QDirIterator it(parentPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
        while (it.hasNext())
        {
            it.next();
            QString subDirPath = it.filePath();
            updateDirectoryAppearance(subDirPath);
        }
    });
    
    // Initially disable OK button
    ui->m_okButton->setEnabled(false);
    
    // Update visible directories appearance after UI is set up
    QTimer::singleShot(100, this, &CustomDirectoryDialog::updateVisibleDirectoriesAppearance);
}

CustomDirectoryDialog::~CustomDirectoryDialog()
{
    delete ui;
}

void CustomDirectoryDialog::setupIcons()
{
    QCommonStyle style;
    
    // Get default folder icon
    m_defaultFolderIcon = style.standardIcon(QStyle::SP_DirIcon);
    
    // Create green folder icon (modify default to be green)
    QPixmap greenPixmap = m_defaultFolderIcon.pixmap(32, 32);
    QPainter painter(&greenPixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(greenPixmap.rect(), QColor(0, 200, 0, 255));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    m_greenFolderIcon = QIcon(greenPixmap);
    
    // Create gray folder icon (modify default to be gray)
    QPixmap grayPixmap = m_defaultFolderIcon.pixmap(32, 32);
    QPainter grayPainter(&grayPixmap);
    grayPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    grayPainter.fillRect(grayPixmap.rect(), QColor(150, 150, 150, 255));
    grayPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    m_grayFolderIcon = QIcon(grayPixmap);
}

QString CustomDirectoryDialog::getSelectedDirectory() const
{
    return m_selectedDirectory;
}

void CustomDirectoryDialog::setStartDirectory(const QString &path)
{
    if (QDir(path).exists())
    {
        QModelIndex sourceIndex = m_fileSystemModel->index(path);
        QModelIndex proxyIndex = m_sortProxy->mapFromSource(sourceIndex);
        if (proxyIndex.isValid())
        {
            ui->m_treeView->setCurrentIndex(proxyIndex);
            ui->m_treeView->scrollTo(proxyIndex, QTreeView::EnsureVisible);
            onTreeViewClicked(proxyIndex);
        }
    }
}

bool CustomDirectoryDialog::directoryHasHeaderDirectly(const QString &path) const
{
    QDir dir(path);
    if (! dir.exists())
    {
        return false;
    }

    return dir.exists(DirectoryConstants::HEADER_FILE_NAME);
}

CustomDirectoryDialog::HeaderInfo CustomDirectoryDialog::parseHeaderFile(const QString &headerPath) const
{
    HeaderInfo info;
    
    QFileInfo fileInfo(headerPath);
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        return info; // Return invalid info
    }
    
    try
    {
        Config config(headerPath.toStdString(), /*printWarnings=*/false);
        
        // Get number_node_x from DISTRIBUTED category
        ConfigCategory* distributedCat = config.getConfigCategory(ConfigConstants::CATEGORY_DISTRIBUTED, /*ignoreCase=*/true);
        if (distributedCat)
        {
            const ConfigParameter* paramX = distributedCat->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_X);
            if (paramX)
            {
                info.numberNodeX = std::stoi(paramX->getDefaultValue());
            }
            
            const ConfigParameter* paramY = distributedCat->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_Y);
            if (paramY)
            {
                info.numberNodeY = std::stoi(paramY->getDefaultValue());
            }
        }
        
        // Get mode from VISUALIZATION category
        ConfigCategory* visualizationCat = config.getConfigCategory(ConfigConstants::CATEGORY_VISUALIZATION, /*ignoreCase=*/true);
        if (visualizationCat)
        {
            const ConfigParameter* paramMode = visualizationCat->getConfigParameter(ConfigConstants::PARAM_MODE);
            if (paramMode)
            {
                info.mode = QString::fromStdString(paramMode->getDefaultValue()).toLower();
                if (info.mode == "binary")
                    info.mode = "b";
                else if (info.mode == "text")
                    info.mode = "t";
                else
                    info.mode = info.mode.left(1); // Take first letter as fallback
            }
        }
        
        // Mark as valid if we got at least some data
        info.isValid = (info.numberNodeX > 0 || info.numberNodeY > 0 || !info.mode.isEmpty());
    }
    catch (const std::exception& e)
    {
        // Return invalid info on exception
        info.isValid = false;
    }
    
    return info;
}

CustomDirectoryDialog::DirectoryType CustomDirectoryDialog::analyzeDirectory(const QString &path) const
{
    QDir dir(path);
    if (! dir.exists())
    {
        return DirectoryType::Unknown;
    }
    
    if (directoryHasHeaderDirectly(path))
    {
        return DirectoryType::WithHeader;
    }
    
    // Check for subdirectories
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext())
    {
        it.next();
        return DirectoryType::WithSubdirs;
    }
    
    return DirectoryType::Empty;
}

void CustomDirectoryDialog::updateDirectoryAppearance(const QString &path)
{
    DirectoryType type = analyzeDirectory(path);
    
    QIcon icon;
    bool enabled = true;
    
    switch (type)
    {
        case DirectoryType::WithHeader:
            icon = m_greenFolderIcon;
            enabled = true;
            break;
        case DirectoryType::WithSubdirs:
            icon = m_defaultFolderIcon;
            enabled = true;
            break;
        case DirectoryType::Empty:
            icon = m_grayFolderIcon;
            enabled = false;
            break;
        case DirectoryType::Unknown:
        default:
            icon = m_defaultFolderIcon;
            enabled = true;
            break;
    }
    
    m_fileSystemModel->setDirectoryIcon(path, icon);
    m_fileSystemModel->setDirectoryEnabled(path, enabled);
    
    // Parse header file if directory has one
    if (type == DirectoryType::WithHeader)
    {
        QString headerPath = QDir(path).filePath(DirectoryConstants::HEADER_FILE_NAME);
        HeaderInfo info = parseHeaderFile(headerPath);
        m_fileSystemModel->setHeaderInfo(path, info);
    }
    else
    {
        // Clear header info for directories without header
        HeaderInfo emptyInfo;
        m_fileSystemModel->setHeaderInfo(path, emptyInfo);
    }
}

void CustomDirectoryDialog::updateVisibleDirectoriesAppearance()
{
    // Recursively update all visible directories in the tree
    QModelIndex rootIndex = ui->m_treeView->rootIndex();
    updateDirectoriesRecursive(rootIndex);
}

void CustomDirectoryDialog::updateDirectoriesRecursive(const QModelIndex &parentIndex)
{
    for (int row = 0; row < m_sortProxy->rowCount(parentIndex); ++row)
    {
        QModelIndex childIndex = m_sortProxy->index(row, 0, parentIndex);
        QModelIndex sourceIndex = m_sortProxy->mapToSource(childIndex);
        QString path = m_fileSystemModel->filePath(sourceIndex);
        QFileInfo fileInfo(path);
        
        if (fileInfo.isDir())
        {
            updateDirectoryAppearance(path);
            
            // Always update children recursively when this function is called
            // (not just when expanded)
            updateDirectoriesRecursive(childIndex);
        }
    }
}

bool CustomDirectoryDialog::isDirectorySelectable(const QString &path) const
{
    DirectoryType type = analyzeDirectory(path);
    return type != DirectoryType::Empty && type != DirectoryType::Unknown;
}

void CustomDirectoryDialog::onTreeViewClicked(const QModelIndex &index)
{
    if (! index.isValid())
    {
        return;
    }
    
    QString path = m_fileSystemModel->filePath(m_sortProxy->mapToSource(index));
    QFileInfo fileInfo(path);
    
    if (fileInfo.isDir())
    {
        updateDirectoryAppearance(path);
        
        const DirectoryType type = analyzeDirectory(path);
        const bool hasHeader = (type == DirectoryType::WithHeader);
        ui->m_okButton->setEnabled(hasHeader);
        
        // Enable OK button only if directory has Header.txt OR has subdirectories        
        if (isDirectorySelectable(path))
        {
            m_selectedDirectory = path;
            if (hasHeader)
            {
                ui->m_pathLabel->setText(tr("Selected: %1 (contains Header.txt)").arg(path));
            }
            else
            {
                ui->m_pathLabel->setText(tr("Selected: %1 (contains subdirectories)").arg(path));
            }
        }
        else
        {
            m_selectedDirectory.clear();
            ui->m_pathLabel->setText(tr("Selected: None (directory not selectable)"));
        }
    }
}

void CustomDirectoryDialog::onTreeViewDoubleClicked(const QModelIndex &index)
{
    if (! index.isValid())
    {
        return;
    }
    
    QString path = m_fileSystemModel->filePath(m_sortProxy->mapToSource(index));
    QFileInfo fileInfo(path);
    
    if (fileInfo.isDir() && directoryHasHeaderDirectly(path))
    {
        m_selectedDirectory = path;
        accept();
    }
}

void CustomDirectoryDialog::onOkButtonClicked()
{
    if (! m_selectedDirectory.isEmpty() && isDirectorySelectable(m_selectedDirectory))
    {
        accept();
    }
    else
    {
        QMessageBox::warning(this, tr("Invalid Selection"), 
                             tr("Please select a valid directory containing either a Header.txt file or subdirectories."));
    }
}

void CustomDirectoryDialog::onCancelButtonClicked()
{
    reject();
}

void CustomDirectoryDialog::onHiddenDirectoriesToggled(bool checked)
{
    QDir::Filters filters = QDir::Dirs | QDir::NoDotAndDotDot;
    if (checked)
    {
        filters |= QDir::Hidden;
    }
    m_fileSystemModel->setFilter(filters);
}
