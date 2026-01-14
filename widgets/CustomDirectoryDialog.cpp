#include <QStandardPaths>
#include <QCommonStyle>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <QDirIterator>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeView>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QDir>

#include "CustomDirectoryDialog.h"
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
    
    if (role == Qt::DecorationRole && fileInfo.isDir())
    {
        if (m_customIcons.contains(path))
        {
            return m_customIcons[path];
        }
    }
    
    if (role == Qt::ForegroundRole && fileInfo.isDir())
    {
        if (m_enabledState.contains(path) && !m_enabledState[path])
        {
            return QColor(150, 150, 150); // Gray color for disabled directories
        }
    }
    
    return QFileSystemModel::data(index, role);
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

CustomDirectoryDialog::CustomDirectoryDialog(QWidget *parent)
    : QDialog(parent)
    , m_treeView(new QTreeView(this))
    , m_fileSystemModel(new CustomFileSystemModel(this))
    , m_okButton(new QPushButton(tr("Open"), this))
    , m_cancelButton(new QPushButton(tr("Cancel"), this))
    , m_showHiddenCheckBox(new QCheckBox(tr("Show hidden directories"), this))
    , m_pathLabel(new QLabel(this))
{
    setupUI();
    setupIcons();
    
    // Set initial directory
    QString startPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (! startPath.isEmpty())
    {
        setStartDirectory(startPath);
    }
    
    // Initially disable OK button until a valid directory is selected
    m_okButton->setEnabled(false);
}

CustomDirectoryDialog::~CustomDirectoryDialog() = default;

void CustomDirectoryDialog::setupUI()
{
    setWindowTitle(tr("Load Model from Directory"));
    setModal(true);
    resize(600, 400);
    
    // Setup layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Path label
    m_pathLabel->setText(tr("Selected: None"));
    m_pathLabel->setStyleSheet("QLabel { font-weight: bold; color: #666; }");
    mainLayout->addWidget(m_pathLabel);
    
    // Tree view
    m_fileSystemModel->setRootPath("");
    m_fileSystemModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    
    m_treeView->setModel(m_fileSystemModel);
    m_treeView->setRootIndex(m_fileSystemModel->index(QDir::rootPath()));
    m_treeView->setColumnHidden(1, true);  // Hide size column
    m_treeView->setColumnHidden(2, true);  // Hide type column
    m_treeView->setColumnHidden(3, true);  // Hide date column
    m_treeView->setHeaderHidden(true);
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(0, Qt::AscendingOrder);
    
    // Expand the tree to show initial structure
    for (int i = 0; i < m_fileSystemModel->rowCount(m_treeView->rootIndex()); ++i)
    {
        QModelIndex child = m_fileSystemModel->index(i, 0, m_treeView->rootIndex());
        m_treeView->expand(child);
    }
    
    mainLayout->addWidget(m_treeView);
    
    // Checkbox for hidden directories
    mainLayout->addWidget(m_showHiddenCheckBox);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_treeView, &QTreeView::clicked, this, &CustomDirectoryDialog::onTreeViewClicked);
    connect(m_treeView, &QTreeView::doubleClicked, this, &CustomDirectoryDialog::onTreeViewDoubleClicked);
    connect(m_okButton, &QPushButton::clicked, this, &CustomDirectoryDialog::onOkButtonClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &CustomDirectoryDialog::onCancelButtonClicked);
    connect(m_showHiddenCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QDir::Filters filters = QDir::Dirs | QDir::NoDotAndDotDot;
        if (checked)
        {
            filters |= QDir::Hidden;
        }
        m_fileSystemModel->setFilter(filters);
    });
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
    if (QDir(path).exists()) {
        QModelIndex index = m_fileSystemModel->index(path);
        if (index.isValid()) {
            m_treeView->setCurrentIndex(index);
            m_treeView->scrollTo(index, QTreeView::EnsureVisible);
            onTreeViewClicked(index);
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
    
    bool hasHeader = dir.exists(DirectoryConstants::HEADER_FILE_NAME);
    bool hasSubdirs = false;
    
    // Check for subdirectories
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext())
    {
        it.next();
        hasSubdirs = true;
        break;
    }
    
    if (hasHeader)
    {
        return DirectoryType::WithHeader;
    }
    else if (hasSubdirs)
    {
        return DirectoryType::WithSubdirs;
    }
    else
    {
        return DirectoryType::Empty;
    }
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
}

bool CustomDirectoryDialog::isDirectorySelectable(const QString &path) const
{
    DirectoryType type = analyzeDirectory(path);
    return type != DirectoryType::Empty && type != DirectoryType::Unknown;
}

void CustomDirectoryDialog::onTreeViewClicked(const QModelIndex &index)
{
    if (!index.isValid())
    {
        return;
    }
    
    QString path = m_fileSystemModel->filePath(index);
    QFileInfo fileInfo(path);
    
    if (fileInfo.isDir())
    {
        updateDirectoryAppearance(path);
        
        bool selectable = isDirectorySelectable(path);
        m_okButton->setEnabled(selectable);
        
        if (selectable)
        {
            m_selectedDirectory = path;
            m_pathLabel->setText(tr("Selected: %1").arg(path));
        }
        else
        {
            m_selectedDirectory.clear();
            m_pathLabel->setText(tr("Selected: None (directory not selectable)"));
        }
    }
}

void CustomDirectoryDialog::onTreeViewDoubleClicked(const QModelIndex &index)
{
    if (! index.isValid())
    {
        return;
    }
    
    QString path = m_fileSystemModel->filePath(index);
    QFileInfo fileInfo(path);
    
    if (fileInfo.isDir() && isDirectorySelectable(path))
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
