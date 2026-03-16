#include "ConfigDetailsDialog.h"

#include <QApplication>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTableWidget>
#include <QVBoxLayout>

#include "config/Config.h"


ConfigDetailsDialog::ConfigDetailsDialog(const std::string& configFilePath, QWidget* parent)
    : QDialog(parent)
    , tableWidget(new QTableWidget(this))
    , mainLayout(new QVBoxLayout(this))
    , filePathLabel(new QLabel(this))
{
    setupUI();
    loadConfigData(configFilePath);

    filePathLabel->setText(QString::fromStdString(configFilePath));

    adjustSizeToContent();
}

ConfigDetailsDialog::~ConfigDetailsDialog() = default;

void ConfigDetailsDialog::setupUI()
{
    setWindowTitle(tr("Configuration Details"));
    setMinimumSize(600, 600);

    // Setup file path label
    QFont pathFont;
    pathFont.setBold(true);
    filePathLabel->setFont(pathFont);
    filePathLabel->setWordWrap(true);
    filePathLabel->setStyleSheet("QLabel { padding: 5px; background-color: #f0f0f0; border: 1px solid #ccc; border-radius: 3px; }");
    mainLayout->addWidget(filePathLabel);

    // Setup table
    tableWidget->setColumnCount(2);
    tableWidget->setHorizontalHeaderLabels({ tr("Parameter"), tr("Value") });
    tableWidget->horizontalHeader()->setStretchLastSection(true);
    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setAlternatingRowColors(true);

    // Add table to layout
    mainLayout->addWidget(tableWidget);

    // Add close button
    QPushButton* closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeButton);

    setLayout(mainLayout);
}

void ConfigDetailsDialog::loadConfigData(const std::string& configFilePath)
{
    try
    {
        Config config(configFilePath);

        int rowCount = 0;

        // Count total rows needed
        const auto categoryNames = config.categoryNames();

        for (const auto& categoryName : categoryNames)
        {
            ConfigCategory* category = config.getConfigCategory(categoryName);
            if (category && category->getSize() > 0)
            {
                const int categorySize = static_cast<int>(category->getSize());
                rowCount += 1 + categorySize; // 1 for category header + parameters
            }
        }

        tableWidget->setRowCount(rowCount);

        int currentRow = 0;

        // Populate table
        for (const auto& categoryName : categoryNames)
        {
            ConfigCategory* category = config.getConfigCategory(categoryName);
            if (! category || category->getSize() == 0)
                continue;

            // Add category header row
            QTableWidgetItem* categoryItem = new QTableWidgetItem(QString::fromStdString(categoryName + ":"));
            QFont boldFont;
            boldFont.setBold(true);
            categoryItem->setFont(boldFont);
            categoryItem->setBackground(QColor(220, 220, 220));

            tableWidget->setItem(currentRow, 0, categoryItem);
            tableWidget->setItem(currentRow, 1, new QTableWidgetItem(""));
            tableWidget->item(currentRow, 1)->setBackground(QColor(220, 220, 220));
            tableWidget->setSpan(currentRow, 0, 1, 2);
            currentRow++;

            // Add parameters
            for (const auto& param : category->getConfigParameters())
            {
                QTableWidgetItem* nameItem = new QTableWidgetItem(
                    QString("    ") + QString::fromStdString(param.getName())
                );
                QTableWidgetItem* valueItem = new QTableWidgetItem(
                    QString::fromStdString(param.getDefaultValue())
                );
                
                tableWidget->setItem(currentRow, 0, nameItem);
                tableWidget->setItem(currentRow, 1, valueItem);
                currentRow++;
            }
        }
    }
    catch (const std::exception& e)
    {
        // If config reading fails, show error in table
        tableWidget->setRowCount(1);
        tableWidget->setItem(0, 0, new QTableWidgetItem(tr("Error loading configuration")));
        tableWidget->setItem(0, 1, new QTableWidgetItem(QString::fromStdString(e.what())));
    }
}

void ConfigDetailsDialog::adjustSizeToContent()
{
    // Calculate required height for table content
    int totalHeight = 0;

    // Add height for all rows
    for (int row = 0; row < tableWidget->rowCount(); ++row)
    {
        totalHeight += tableWidget->rowHeight(row);
    }

    // Add height for horizontal header
    totalHeight += tableWidget->horizontalHeader()->height();

    // Add height for file path label
    totalHeight += filePathLabel->sizeHint().height();

    // Add height for close button (approximate)
    totalHeight += 50;

    // Add margins and spacing from layout
    totalHeight += mainLayout->contentsMargins().top() + mainLayout->contentsMargins().bottom();
    totalHeight += mainLayout->spacing() * 2; // spacing between widgets

    // Add some extra padding
    totalHeight += 40;

    // Get screen geometry to limit maximum height
    QScreen* screen = QApplication::primaryScreen();
    int maxHeight = screen->availableGeometry().height() - 100; // Leave some margin

    // Set height, but don't exceed screen height
    int finalHeight = qMin(totalHeight, maxHeight);

    // Keep minimum width at 600
    resize(600, finalHeight);
}
