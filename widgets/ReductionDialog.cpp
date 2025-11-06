/** @file ReductionDialog.cpp
 * @brief Implementation of the ReductionDialog class.
 */

#include "ReductionDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QApplication>
#include <QScreen>

ReductionDialog::ReductionDialog(const std::map<QString, QString>& reductionData,
                                 int stepNumber,
                                 QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Reduction Values - Step %1").arg(stepNumber));
    setWindowModality(Qt::WindowModal);
    setMinimumWidth(400);
    setMinimumHeight(300);

    setupUI(reductionData, stepNumber);

    // Center the dialog on the parent window
    if (parent)
    {
        QRect parentGeometry = parent->geometry();
        QRect dialogGeometry = geometry();
        int x = parentGeometry.x() + (parentGeometry.width() - dialogGeometry.width()) / 2;
        int y = parentGeometry.y() + (parentGeometry.height() - dialogGeometry.height()) / 2;
        move(x, y);
    }
}

void ReductionDialog::setupUI(const std::map<QString, QString>& reductionData, int stepNumber)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Title label
    auto* titleLabel = new QLabel(QString("Reduction values for step %1:").arg(stepNumber));
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    mainLayout->addWidget(titleLabel);

    // Create table for reduction values
    auto* table = new QTableWidget();
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Reduction Type", "Value"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(true);

    // Populate table with reduction data
    int row = 0;
    for (const auto& [key, value] : reductionData)
    {
        table->insertRow(row);

        auto* keyItem = new QTableWidgetItem(key);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        keyItem->setFont(QFont("Courier", 10));
        table->setItem(row, 0, keyItem);

        auto* valueItem = new QTableWidgetItem(value);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setFont(QFont("Courier", 10));
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 1, valueItem);

        ++row;
    }

    // Resize columns to content
    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);

    mainLayout->addWidget(table);

    // Close button
    auto* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeButton);
}
