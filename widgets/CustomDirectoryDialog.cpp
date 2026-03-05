#include <iostream>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QDirIterator>
#include <QMessageBox>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <QStyle>
#include <QCommonStyle>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>
#include <QBuffer>
#include <QHBoxLayout>
#include <QTreeView>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QMap>
#include "CustomDirectoryDialog.h"
#include "ui_CustomDirectoryDialog.h"
#include "config/Config.h"
#include "config/ConfigCategory.h"
#include "config/ConfigParameter.h"
#include "config/ConfigConstants.h"
#include "core/directoryConstants.h"
#include "plugins/ModelLoader.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"
#include "plugins/ModelLoader.h"


namespace
{
// Column indices for the tree view
enum ColumnIndex
{
    ColumnDirectory = 0,
    ColumnX = 1,
    ColumnY = 2,
    ColumnZ = 3,
    ColumnM = 4,

    ColumnCount
};

bool is3DModel(Config& config)
{
    // Determine if this is a 3D model by checking number_of_slices from GENERAL category
    ConfigCategory* generalCat = config.getConfigCategory(ConfigConstants::CATEGORY_GENERAL, /*ignoreCase=*/true);
    if (generalCat)
    {
        const ConfigParameter* paramSlices = generalCat->getConfigParameter(ConfigConstants::PARAM_NUMBER_OF_SLICES);
        if (paramSlices)
        {
            try
            {
                int numberOfSlices = std::stoi(paramSlices->getDefaultValue());
                return (numberOfSlices > 1);
            }
            catch (...)
            {
                return false;
            }
        }
    }
    return false;
}

QString readFileModeFromConfig(Config& config)
{
    // Get mode from VISUALIZATION category
    ConfigCategory* visualizationCat = config.getConfigCategory(ConfigConstants::CATEGORY_VISUALIZATION, /*ignoreCase=*/true);
    if (visualizationCat)
    {
        const ConfigParameter* paramMode = visualizationCat->getConfigParameter(ConfigConstants::PARAM_MODE);
        if (paramMode)
        {
            QString mode = QString::fromStdString(paramMode->getDefaultValue()).toLower();
            if (mode == "binary")
                return "b";
            else if (mode == "text")
                return "t";
            else
                return mode.left(1); // Take first letter as fallback
        }
    }
    return QString();
}
} // namespace

namespace Icons
{
QIcon standardDirectoryIcon()
{
    return QCommonStyle{}.standardIcon(QStyle::SP_DirIcon);
}

QIcon greenIcon()
{
    return QIcon::fromTheme("folder-green");
}

QIcon grayIcon()
{
    // Create gray folder icon (modify default to be gray)
    QPixmap grayPixmap = standardDirectoryIcon().pixmap(32, 32);
    QPainter grayPainter(&grayPixmap);
    grayPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    grayPainter.fillRect(grayPixmap.rect(), QColor(150, 150, 150, 255));
    grayPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    return QIcon(grayPixmap);
}
} // namespace icons


struct CustomDirectoryDialog::HeaderInfo
{
    int numberNodeX = -1;
    int numberNodeY = -1;
    int numberNodeZ = -1;
    QString mode; // "binary" or "text"
    bool is3DModel = false;  // True if model has Z dimension > 1 or number_of_slices > 1
    bool isValid = false;
};


class CustomDirectoryDialog::CustomFileSystemModel : public QFileSystemModel
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
    QMap<QString, CustomDirectoryDialog::HeaderInfo> m_headerInfo;
};

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
    
    // Handle custom columns (columns X, Y, M)
    if (role == Qt::DisplayRole && index.column() >= ColumnX && index.column() <= ColumnM && fileInfo.isDir())
    {
        if (m_headerInfo.contains(path))
        {
            const HeaderInfo &info = m_headerInfo[path];
            if (info.isValid)
            {
                switch (index.column())
                {
                case ColumnX: // x column (number_node_x)
                    return info.numberNodeX > 0 ? QString::number(info.numberNodeX) : QString();
                case ColumnY: // y column (number_node_y)
                    return info.numberNodeY > 0 ? QString::number(info.numberNodeY) : QString();
                case ColumnZ: // z column (number_node_z) - only show for 3D models
                    return info.is3DModel ? QString::number(info.numberNodeZ) : QString();
                case ColumnM: // m column (mode)
                    return info.mode;
                }
            }
        }
        return QString(); // Empty string for directories without header info
    }
    
    // Right-align text for columns X, Y, M
    if (role == Qt::TextAlignmentRole && index.column() >= ColumnX && index.column() <= ColumnM)
    {
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);
    }

    // Only show icons in first column (column Directory)
    if (role == Qt::DecorationRole && fileInfo.isDir() && index.column() == ColumnDirectory)
    {
        if (m_customIcons.contains(path))
        {
            return m_customIcons[path];
        }
    }
    
    // Only show foreground color in first column
    if (role == Qt::ForegroundRole && fileInfo.isDir() && index.column() == ColumnDirectory)
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
    Q_UNUSED(parent)
    return ColumnCount;
}

QVariant CustomDirectoryDialog::CustomFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case ColumnDirectory: 
            return tr("Directory");
        case ColumnX: 
            return tr("X");
        case ColumnY: 
            return tr("Y");
        case ColumnZ:
            return tr("Z");
        case ColumnM: 
            return tr("M");
        }
    }
    
    if (orientation == Qt::Horizontal && role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case ColumnDirectory:
            {
                // Use static method to convert icon to base64 for embedding in HTML
                QPixmap greenPixmap = Icons::greenIcon().pixmap(16, 16);
                QPixmap defaultPixmap = Icons::standardDirectoryIcon().pixmap(16, 16);
                QPixmap grayPixmap = Icons::grayIcon().pixmap(16, 16);
                
                QByteArray greenIconData;
                QBuffer greenBuffer(&greenIconData);
                greenPixmap.save(&greenBuffer, "PNG", 100);
                QString greenIconBase64 = greenIconData.toBase64();
                
                QByteArray defaultIconData;
                QBuffer defaultBuffer(&defaultIconData);
                defaultPixmap.save(&defaultBuffer, "PNG", 100);
                QString defaultIconBase64 = defaultIconData.toBase64();
                
                QByteArray grayIconData;
                QBuffer grayBuffer(&grayIconData);
                grayPixmap.save(&grayBuffer, "PNG", 100);
                QString grayIconBase64 = grayIconData.toBase64();
                
                return tr("<html><b>Directory</b><br/>"
                          "Shows directory names with icons:<br/>"
                          "• <img src='data:image/png;base64,%1' width='16' height='16'/> Green folder - contains Header.txt file<br/>"
                          "• <img src='data:image/png;base64,%2' width='16' height='16'/> Default folder - contains subdirectories<br/>"
                          "• <img src='data:image/png;base64,%3' width='16' height='16'/> Gray folder - empty directory</html>")
                          .arg(greenIconBase64)
                          .arg(defaultIconBase64)
                          .arg(grayIconBase64);
            }
        case ColumnX:
            return tr("<html><b>X</b><br/>"
                      "Number of nodes in X direction<br/>"
                      "Extracted from Header.txt file<br/>"
                      "Only shown for directories with Header.txt</html>");
        case ColumnY:
            return tr("<html><b>Y</b><br/>"
                      "Number of nodes in Y direction<br/>"
                      "Extracted from Header.txt file<br/>"
                      "Only shown for directories with Header.txt</html>");
        case ColumnZ:
            return tr("<html><b>Z</b><br/>"
                      "Number of nodes in Z direction (3D)<br/>"
                      "Extracted from Header.txt file<br/>"
                      "Only shown for directories with Header.txt</html>");
        case ColumnM:
            return tr("<html><b>M</b><br/>"
                      "Processing mode<br/>"
                      "<b>b</b> - binary mode<br/>"
                      "<b>t</b> - text mode<br/>"
                      "Extracted from Header.txt file<br/>"
                      "Only shown for directories with Header.txt</html>");
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

bool CustomDirectoryDialog::CustomFileSystemModel::hasChildren(const QModelIndex &parent) const
{
    if (! parent.isValid())
        return QFileSystemModel::hasChildren(parent);

    QFileInfo info = fileInfo(parent);
    if (! info.isDir())
        return false;

    // check if the directory has subdirectories
    QDir dir(info.absoluteFilePath());
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    return dir.entryInfoList().size() > 0;
}

class CustomDirectoryDialog::DirectorySortProxy : public QSortFilterProxyModel
{
public:
    explicit DirectorySortProxy(QObject *parent = nullptr);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

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
    , m_hasCompiledLibrary(false)
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
    
    // Set column resize modes with fixed percentages
    QHeaderView* header = ui->m_treeView->header();
    header->setSectionResizeMode(ColumnDirectory, QHeaderView::Interactive);  // 70% for directory column
    header->setSectionResizeMode(ColumnX, QHeaderView::Fixed);  // 7.5% for X column
    header->setSectionResizeMode(ColumnY, QHeaderView::Fixed);  // 7.5% for Y column
    header->setSectionResizeMode(ColumnZ, QHeaderView::Fixed);  // 7.5% for Z column
    header->setSectionResizeMode(ColumnM, QHeaderView::Fixed);  // 7.5% for M column
    header->setStretchLastSection(false);
    
    // Set initial column widths (will be adjusted to percentages when widget is shown)
    ui->m_treeView->setColumnWidth(ColumnDirectory, 400);  // Initial width for directory column
    ui->m_treeView->setColumnWidth(ColumnX, 20);   // Initial width for X column
    ui->m_treeView->setColumnWidth(ColumnY, 20);   // Initial width for Y column
    ui->m_treeView->setColumnWidth(ColumnZ, 20);   // Initial width for Z column
    ui->m_treeView->setColumnWidth(ColumnM, 20);   // Initial width for M column
    
    // Apply percentage-based widths after widget is shown
    QTimer::singleShot(0, this, [this]() {
        QHeaderView* header = ui->m_treeView->header();
        int totalWidth = ui->m_treeView->header()->viewport()->width();
        
        header->resizeSection(ColumnDirectory, totalWidth * 0.7);   // 70% for directory
        header->resizeSection(ColumnX, totalWidth * 0.075);        // 7.5% for X
        header->resizeSection(ColumnY, totalWidth * 0.075);        // 7.5% for Y
        header->resizeSection(ColumnZ, totalWidth * 0.075);        // 7.5% for Z
        header->resizeSection(ColumnM, totalWidth * 0.075);        // 7.5% for M
    });
    
    ui->m_treeView->setSortingEnabled(true);
    ui->m_treeView->sortByColumn(ColumnDirectory, Qt::AscendingOrder);
    
    // Connect to directoryLoaded signal to update sorting when directories are loaded
    connect(m_fileSystemModel, &QFileSystemModel::directoryLoaded,
            this, [this](const QString &) {
                ui->m_treeView->sortByColumn(ColumnDirectory, Qt::AscendingOrder);
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
    
    // Connect compile module checkbox state change
    connect(ui->compileModuleCheckBox, &QCheckBox::toggled, this, &CustomDirectoryDialog::onCompileModuleToggled);
    
    // Connect tab widget and combo box signals
    connect(ui->loadingModuleOptionsTabWidget, &QTabWidget::currentChanged, this, &CustomDirectoryDialog::onLoadingModuleOptionsTabChanged);
    connect(ui->availableModulesComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CustomDirectoryDialog::onAvailableModulesChanged);
    
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
    m_defaultFolderIcon = Icons::standardDirectoryIcon();
    m_greenFolderIcon = Icons::greenIcon();
    m_grayFolderIcon = Icons::grayIcon();
}

QString CustomDirectoryDialog::getSelectedDirectory() const
{
    return m_selectedDirectory;
}

bool CustomDirectoryDialog::compilationRequested() const
{
    return ui->compileModuleCheckBox->isChecked();
}

QString CustomDirectoryDialog::getSelectedExistingModel() const
{
    return ui->availableModulesComboBox->currentText();
}

CustomDirectoryDialog::LoadingMode CustomDirectoryDialog::getLoadingMode() const
{
    int currentIndex = ui->loadingModuleOptionsTabWidget->currentIndex();
    return (currentIndex == 1) ? LoadingMode::UseExistingModel : LoadingMode::CompileModule;
}

void CustomDirectoryDialog::updateModuleInfo(const QString &directoryPath)
{
    // Find header file in the directory
    auto cppHeaderFile = QString::fromStdString(ModelLoader::findHeaderFile(directoryPath.toStdString()));
    
    if (cppHeaderFile.isEmpty())
    {
        // No header file found - clear all fields
        clearModuleInfo();
        
        // Disable available modules combo box for non-green directories
        ui->availableModulesComboBox->setEnabled(false);
        ui->availableModulesComboBox->clear();
        ui->availableModulesLabel->setEnabled(false);
        
        // Update OK button state
        updateOkButtonState();
        return;
    }
    
    // Enable available modules combo box for green directories
    ui->availableModulesComboBox->setEnabled(true);
    ui->availableModulesLabel->setEnabled(true);
    
    // Load available models
    loadAvailableModels();
    
    // Read expected model name from Header.txt and select matching model
    QString expectedModelName = readExpectedModelName(directoryPath);
    selectMatchingModel(expectedModelName);
    
    // Update source file information
    updateSourceFileInfo(cppHeaderFile);
    
    // Generate expected library name and update compiled module info
    auto libraryFile = QString::fromStdString(ModelLoader::generateModuleNameForSourceFile(cppHeaderFile.toStdString()));
    updateCompiledModuleInfo(directoryPath, libraryFile);
    
    // Update OK button state
    updateOkButtonState();
}

QDateTime CustomDirectoryDialog::getFileModificationDate(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists())
    {
        return fileInfo.lastModified();
    }
    return QDateTime();
}

void CustomDirectoryDialog::clearModuleInfo()
{
    ui->modelSourceLineEdit->clear();
    ui->compiledModuleLineEdit->clear();
    ui->compileModuleCheckBox->setChecked(false);
    ui->compileModuleCheckBox->setEnabled(false);
    ui->compileModuleCheckBox->setToolTip(tr("No module selected"));
    
    // Clear date labels and reset styles
    ui->modelSourceModificationTimeLineEdit->setText(tr("(not available)"));
    ui->compiledModuleTimeLineEdit->setText(tr("(not available)"));
    ui->modelSourceModificationTimeLineEdit->setStyleSheet("");
    ui->compiledModuleTimeLineEdit->setStyleSheet("");
    
    // Clear available modules combo box and disable it
    ui->availableModulesComboBox->clear();
    ui->availableModulesComboBox->setEnabled(false);
    ui->availableModulesLabel->setEnabled(false);
    
    // Clear tooltips
    ui->modelSourceLineEdit->setToolTip(tr("No source files found"));
    ui->compiledModuleLineEdit->setToolTip(tr("No compiled module found"));
}

void CustomDirectoryDialog::onCompileModuleToggled(bool checked)
{
    // Enable/disable OK button based on checkbox state and library existence
    // If we have a compiled library (m_hasCompiledLibrary = true):
    // - When checked (recompilation needed): enable OK button
    // - When unchecked (no recompilation needed): enable OK button
    // If we don't have a compiled library (m_hasCompiledLibrary = false):
    // - When checked (compilation needed): enable OK button
    // - When unchecked (no compilation): disable OK button
    if (m_hasCompiledLibrary)
    {
        // Library exists - always enable OK button regardless of checkbox state
        ui->m_okButton->setEnabled(true);
    }
    else
    {
        // No library - enable OK button only when checkbox is unchecked (no compilation)
        ui->m_okButton->setEnabled(checked);
    }
}

void CustomDirectoryDialog::onLoadingModuleOptionsTabChanged(int index)
{
    // Update OK button state based on current tab
    updateOkButtonState();
}

void CustomDirectoryDialog::onAvailableModulesChanged(int index)
{
    // Update OK button state based on available modules selection
    updateOkButtonState();
}

void CustomDirectoryDialog::updateOkButtonState()
{
    // Check if we have a valid directory selected
    if (m_selectedDirectory.isEmpty() || !isDirectorySelectable(m_selectedDirectory))
    {
        ui->m_okButton->setEnabled(false);
        return;
    }
    
    // Get current tab index
    int currentTab = ui->loadingModuleOptionsTabWidget->currentIndex();
    
    if (currentTab == 0) // compileModuleTab
    {
        // Use existing logic for compile module tab
        if (m_hasCompiledLibrary)
        {
            // Library exists - always enable OK button regardless of checkbox state
            ui->m_okButton->setEnabled(true);
        }
        else
        {
            // No library - enable OK button only when checkbox is checked (compilation needed)
            ui->m_okButton->setEnabled(ui->compileModuleCheckBox->isChecked());
        }
    }
    else if (currentTab == 1) // useExistingModuleTab
    {
        // Enable OK button only if we have available models selected
        bool hasModels = ui->availableModulesComboBox->count() > 0;
        bool hasSelection = ui->availableModulesComboBox->currentIndex() >= 0;
        ui->m_okButton->setEnabled(hasModels && hasSelection);
    }
}

void CustomDirectoryDialog::loadAvailableModels()
{
    // Clear current items
    ui->availableModulesComboBox->clear();
    
    // Get available models from factory
    const auto availableModels = SceneWidgetVisualizerFactory::getAvailableModels();
    
    if (availableModels.empty())
    {
        std::cerr << "Warning: No models available from factory!" << std::endl;
        return;
    }
    
    // Add models to combo box
    for (const auto& modelName : availableModels)
    {
        ui->availableModulesComboBox->addItem(QString::fromStdString(modelName));
    }
    
    // Select first model by default
    if (ui->availableModulesComboBox->count() > 0)
    {
        ui->availableModulesComboBox->setCurrentIndex(0);
    }
    
    std::cout << "[DEBUG] Loaded " << availableModels.size() << " available models" << std::endl;
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

        readNodeNumbersFromConfig(config, info);

        info.is3DModel = is3DModel(config);

        info.mode = readFileModeFromConfig(config);
        
        // Mark as valid if we got at least some data
        info.isValid = (info.numberNodeX > 0 || info.numberNodeY > 0 || info.numberNodeZ > 0 || ! info.mode.isEmpty());
    }
    catch (const std::exception& /*e*/)
    {
        // Return invalid info on exception
        info.isValid = false;
    }
    
    return info;
}
void CustomDirectoryDialog::readNodeNumbersFromConfig(Config& config, CustomDirectoryDialog::HeaderInfo& info) const
{
    // Get number_node_{x,y,z} from DISTRIBUTED category
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

        const ConfigParameter* paramZ = distributedCat->getConfigParameter(ConfigConstants::PARAM_NUMBER_NODE_Z);
        if (paramZ)
        {
            info.numberNodeZ = std::stoi(paramZ->getDefaultValue());
        }
    }
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
                updateModuleInfo(path);
            }
            else
            {
                ui->m_pathLabel->setText(tr("Selected: %1 (contains subdirectories)").arg(path));
                // Clear module info for non-green directories
                clearModuleInfo();
            }
        }
        else
        {
            m_selectedDirectory.clear();
            ui->m_pathLabel->setText(tr("Selected: None (directory not selectable)"));
            // Clear module info for non-selectable directories
            clearModuleInfo();
        }
    }
}

void CustomDirectoryDialog::updateSourceFileInfo(const QString &cppHeaderFile)
{
    // Extract only filename from full path
    QFileInfo sourceFileInfo(cppHeaderFile);
    QString sourceFileName = sourceFileInfo.fileName();
    
    // Update source file information
    ui->modelSourceLineEdit->setText(sourceFileName);
    QDateTime sourceModDate = getFileModificationDate(cppHeaderFile);

    // Update source date label (instead of QDateTimeEdit)
    if (sourceModDate.isValid())
    {
        ui->modelSourceModificationTimeLineEdit->setText(tr("Modified: %1")
                                                             .arg(sourceModDate.toString("yyyy-MM-dd hh:mm:ss")));
        ui->modelSourceModificationTimeLineEdit->setToolTip(tr("Source file: %1\nModified: %2")
                                                                .arg(cppHeaderFile)
                                                                .arg(sourceModDate.toString("yyyy-MM-dd hh:mm:ss")));
    }
    else
    {
        ui->modelSourceModificationTimeLineEdit->setText(tr("(file not available)"));
        ui->modelSourceModificationTimeLineEdit->setToolTip(tr("Source file: %1\nFile not available")
                                                                .arg(cppHeaderFile));
    }

    ui->modelSourceLineEdit->setToolTip(tr("Source file: %1\nModified: %2")
                                       .arg(cppHeaderFile)
                                       .arg(sourceModDate.isValid() ? sourceModDate.toString("yyyy-MM-dd hh:mm:ss") : tr("(file not available)")));
}

void CustomDirectoryDialog::updateCompiledModuleInfo(const QString &directoryPath, const QString &libraryFile)
{
    // Extract only filename from library name
    QFileInfo libraryFileInfo(libraryFile);
    QString libraryFileName = libraryFileInfo.fileName();
    
    ui->compiledModuleLineEdit->setText(libraryFileName);
    
    // Check if compiled library exists
    QString libraryPath = QDir(directoryPath).filePath(libraryFile);
    QFileInfo libraryInfo(libraryPath);
    
    m_hasCompiledLibrary = libraryInfo.exists();
    
    if (libraryInfo.exists())
    {
        QDateTime libraryModDate = getFileModificationDate(libraryPath);

        // Update compiled module date label (instead of QDateTimeEdit)
        QString compiledModuleText = tr("Modified: %1").arg(libraryModDate.toString("yyyy-MM-dd hh:mm:ss"));
        ui->compiledModuleTimeLineEdit->setText(compiledModuleText);
        ui->compiledModuleTimeLineEdit->setToolTip(tr("Compiled module: %1\nModified: %2")
                                                      .arg(libraryPath)
                                                      .arg(libraryModDate.toString("yyyy-MM-dd hh:mm:ss")));

        // Get source file modification date for comparison
        QString cppHeaderFile = QString::fromStdString(ModelLoader::findHeaderFile(directoryPath.toStdString()));
        QDateTime sourceModDate = getFileModificationDate(cppHeaderFile);

        // Compare dates to determine if recompilation is needed
        if (sourceModDate.isValid() && libraryModDate.isValid())
        {
            if (sourceModDate > libraryModDate)
            {
                // Source is newer than compiled module
                ui->compiledModuleTimeLineEdit->setStyleSheet("background-color: #ffcccc; color: black;");
                ui->compiledModuleTimeLineEdit->setToolTip(tr("Compiled module: %1\nModified: %2\n\n⚠️ Source file is newer - recompilation recommended!")
                                                              .arg(libraryPath)
                                                              .arg(libraryModDate.toString("yyyy-MM-dd hh:mm:ss")));
                
                // Check the checkbox by default when source is newer
                ui->compileModuleCheckBox->setChecked(true);
            }
            else
            {
                // Compiled module is up-to-date
                ui->compiledModuleTimeLineEdit->setStyleSheet("");
                ui->compiledModuleTimeLineEdit->setToolTip(tr("Compiled module: %1\nModified: %2\n✅ Up to date")
                                                              .arg(libraryPath)
                                                              .arg(libraryModDate.toString("yyyy-MM-dd hh:mm:ss")));
                
                // Uncheck the checkbox when library is up-to-date
                ui->compileModuleCheckBox->setChecked(false);
            }
        }
        else
        {
            ui->compiledModuleTimeLineEdit->setStyleSheet("");
        }
        
        // Enable checkbox when library exists
        ui->compileModuleCheckBox->setEnabled(true);
        ui->compileModuleCheckBox->setToolTip(tr("Module compiled successfully. Check to force recompilation."));
    }
    else
    {
        // No compiled library found
        ui->compiledModuleTimeLineEdit->setText(tr("(not compiled)"));
        ui->compiledModuleTimeLineEdit->setStyleSheet("background-color: #ffcccc; color: black;");
        ui->compiledModuleTimeLineEdit->setToolTip(tr("Compiled module not found. Compilation required."));
        
        // Check the checkbox by default when no library exists
        ui->compileModuleCheckBox->setChecked(true);
        
        // Enable checkbox when no library exists (user can uncheck to prevent compilation)
        ui->compileModuleCheckBox->setEnabled(true);
        ui->compileModuleCheckBox->setToolTip(tr("No compiled module found. Check to compile."));
    }
    
    // Update tooltip for compiled module line edit
    if (libraryInfo.exists())
    {
        QDateTime libraryModDate = getFileModificationDate(libraryPath);
        ui->compiledModuleLineEdit->setToolTip(tr("Compiled module: %1\nModified: %2")
                                                   .arg(libraryPath)
                                                   .arg(libraryModDate.toString("yyyy-MM-dd hh:mm:ss")));
    }
    else
    {
        ui->compiledModuleLineEdit->setToolTip(tr("Compiled module not found: %1\nCompilation required.")
                                                   .arg(libraryPath));
    }
}

QString CustomDirectoryDialog::readExpectedModelName(const QString &directoryPath) const
{
    try
    {
        QString headerPath = QDir(directoryPath).filePath(DirectoryConstants::HEADER_FILE_NAME);
        if (QFileInfo::exists(headerPath))
        {
            Config config(headerPath.toStdString(), /*printWarnings=*/false);
            return QString::fromStdString(ModelLoader::readOutputFileName(&config));
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Warning: Could not read output file name from Header.txt: " << e.what() << std::endl;
    }
    return QString();
}

void CustomDirectoryDialog::selectMatchingModel(const QString &expectedModelName)
{
    // Find matching model in available models
    int matchingModelIndex = -1;
    if (!expectedModelName.isEmpty())
    {
        for (int i = 0; i < ui->availableModulesComboBox->count(); ++i)
        {
            QString availableModel = ui->availableModulesComboBox->itemText(i);
            // Check for exact match or case-insensitive match
            if (availableModel.compare(expectedModelName, Qt::CaseInsensitive) == 0 ||
                availableModel.contains(expectedModelName, Qt::CaseInsensitive) ||
                expectedModelName.contains(availableModel, Qt::CaseInsensitive))
            {
                matchingModelIndex = i;
                break;
            }
        }
    }
    
    // Set the matching model if found, otherwise clear selection
    if (matchingModelIndex >= 0)
    {
        ui->availableModulesComboBox->setCurrentIndex(matchingModelIndex);
        std::cout << "[DEBUG] Found matching model: '" << ui->availableModulesComboBox->itemText(matchingModelIndex).toStdString() 
                  << "' for expected: '" << expectedModelName.toStdString() << "'" << std::endl;
    }
    else
    {
        // No matching model found - clear combo box and disable OK button
        ui->availableModulesComboBox->setCurrentIndex(-1);
        if (!expectedModelName.isEmpty())
        {
            std::cout << "[DEBUG] No matching model found for expected: '" << expectedModelName.toStdString() << "'" << std::endl;
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
        // Check if compilation is needed but not done
        if (! m_hasCompiledLibrary && ! ui->compileModuleCheckBox->isChecked())
        {
            QMessageBox::warning(this, tr("Compilation Required"), 
                                 tr("The module does not exist, please check \"Compile module\" before it can be used.\n"
                                    "Please check 'Compile module' and compile the module first."));
            return;
        }

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
