/** @file SubstatesDockWidget.cpp
 * @brief Implementation of SubstatesDockWidget. */

#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include "SubstatesDockWidget.h"
#include "SubstateDisplayWidget.h"
#include "visualiser/SettingParameter.h"
#include "visualiserProxy/ISceneWidgetVisualizer.h"

// Helper function to create a separator line
static QFrame* createSeparator()
{
    auto separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setMaximumHeight(1);
    separator->setStyleSheet("QFrame { background-color: #e0e0e0; }");
    return separator;
}


SubstatesDockWidget::SubstatesDockWidget(QWidget* parent)
    : QDockWidget("Substate Parameters", parent)
    , m_scrollArea(nullptr)
    , m_containerWidget(nullptr)
    , m_containerLayout(nullptr)
{
    // Initialize will be called after UI setup in MainWindow
    // Set initial size - narrower for two-column layout
    setMinimumWidth(240);
}

void SubstatesDockWidget::initializeFromUI()
{
    // Find scroll area and container layout from UI
    m_scrollArea = findChild<QScrollArea*>("scrollArea");
    if (m_scrollArea)
    {
        m_containerWidget = m_scrollArea->widget();
        if (m_containerWidget)
        {
            m_containerLayout = qobject_cast<QVBoxLayout*>(m_containerWidget->layout());
        }
    }
}

void SubstatesDockWidget::updateSubstates(SettingParameter* settingParameter)
{
    if (!settingParameter)
        return;

    // Clear existing widgets
    clearWidgets();

    // Get substate fields
    auto fields = settingParameter->getSubstateFields();

    // Create new widgets for each field
    for (size_t i = 0; i < fields.size(); ++i)
    {
        const auto& field = fields[i];
        
        auto widget = new SubstateDisplayWidget(field, m_containerWidget);

        // Restore saved parameters if they exist
        auto it = settingParameter->substateInfo.find(field);
        if (it != settingParameter->substateInfo.end())
        {
            widget->setMinValue(it->second.minValue);
            widget->setMaxValue(it->second.maxValue);
            widget->setFormat(it->second.format);
        }

        // Connect signal
        connect(widget, &SubstateDisplayWidget::use3rdDimensionRequested,
                this, &SubstatesDockWidget::use3rdDimensionRequested);

        m_containerLayout->addWidget(widget);
        m_substateWidgets[field] = widget;
        
        // Add separator between widgets (but not after the last one)
        if (i < fields.size() - 1)
        {
            m_containerLayout->addWidget(createSeparator());
        }
    }

    // Add stretch at the end to push widgets to the top
    m_containerLayout->addStretch();
}

void SubstatesDockWidget::updateCellValues(SettingParameter* settingParameter, int row, int col, class ISceneWidgetVisualizer* visualizer)
{
    if (!settingParameter || !visualizer)
        return;

    // Update header with row/col information (x=col, y=row for clarity)
    auto headerLabel = findChild<QLabel*>("cellHeaderLabel");
    if (headerLabel)
    {
        headerLabel->setText(QString("Cell: (x=%1, y=%2)").arg(col).arg(row));
    }

    // Update each substate widget with the cell value
    for (auto& pair : m_substateWidgets)
    {
        const auto& fieldName = pair.first;
        auto& widget = pair.second;

        // Get the cell value for this field
        std::string cellValue = visualizer->getCellStringEncoding(row, col, fieldName.c_str());
        if (!cellValue.empty())
        {
            widget->setCellValue(cellValue);
        }
    }
}

void SubstatesDockWidget::saveParametersToSettings(SettingParameter* settingParameter)
{
    if (!settingParameter)
        return;

    // Save all widget values back to substateInfo
    for (const auto& pair : m_substateWidgets)
    {
        const auto& fieldName = pair.first;
        const auto& widget = pair.second;

        auto it = settingParameter->substateInfo.find(fieldName);
        if (it != settingParameter->substateInfo.end())
        {
            it->second.minValue = widget->getMinValue();
            it->second.maxValue = widget->getMaxValue();
            it->second.format = widget->getFormat();
        }
    }
}

void SubstatesDockWidget::clearWidgets()
{
    // Remove all widgets from layout except header (first 2 items: label + separator)
    // Start from index 2 to preserve header
    while (m_containerLayout->count() > 2)
    {
        QLayoutItem* item = m_containerLayout->takeAt(2);
        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }
        delete item;
    }
    m_substateWidgets.clear();
}
