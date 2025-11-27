/** @file SubstateDisplayWidget.cpp
 * @brief Implementation of SubstateDisplayWidget. */

#include <cmath> //std::isnan
#include <limits>
#include <cctype>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QEvent>
#include <QPushButton>
#include <QColorDialog>
#include "SubstateDisplayWidget.h"
#include "ui_SubstateDisplayWidget.h"

namespace
{
constexpr double emptyValue = -1e9;

/// @brief Configure spinboxes with special value text for empty display
void setUpSpinBoxWithNoValue(QDoubleSpinBox* spinBox)
{
    spinBox->setMinimum(emptyValue);
    spinBox->setMaximum(1e9);
    spinBox->setSpecialValueText(" "); // space for empty display
    spinBox->setValue(emptyValue);
}
} // namespace


SubstateDisplayWidget::SubstateDisplayWidget(const std::string& fieldName, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::SubstateDisplayWidget)
{
    // Create UI from .ui file
    ui->setupUi(this);

    // Set field name in label
    ui->nameLabel->setText(QString::fromStdString(fieldName));
    
    setUpSpinBoxWithNoValue(ui->minSpinBox);
    setUpSpinBoxWithNoValue(ui->maxSpinBox);

    // Enable context menu for the widget
    setContextMenuPolicy(Qt::CustomContextMenu);

    // Install event filter on all child widgets to intercept right-click
    installEventFiltersOnChildren();

    // Connect signals
    connectSignals();
}

SubstateDisplayWidget::~SubstateDisplayWidget()
{
    delete ui;
}

void SubstateDisplayWidget::connectSignals()
{
    connect(ui->use3dButton, &QPushButton::clicked, this, [this]() {
        emit use3rdDimensionRequested(fieldName());
    });

    connect(ui->use2dButton, &QPushButton::clicked, this, &SubstateDisplayWidget::onUse2DClicked);

    // Connect color buttons
    connect(ui->minColorButton, &QPushButton::clicked, this, &SubstateDisplayWidget::onMinColorClicked);
    connect(ui->maxColorButton, &QPushButton::clicked, this, &SubstateDisplayWidget::onMaxColorClicked);
    connect(ui->clearColorsButton, &QPushButton::clicked, this, &SubstateDisplayWidget::onClearColorsClicked);

    // Connect spinbox value changes to update button state
    connect(ui->minSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SubstateDisplayWidget::updateButtonState);
    connect(ui->maxSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SubstateDisplayWidget::updateButtonState);

    // Install event filters on spinboxes to detect focus out
    ui->minSpinBox->installEventFilter(this);
    ui->maxSpinBox->installEventFilter(this);

    // Initial button state
    updateButtonState();
    updateColorButtonAppearance();
}

void SubstateDisplayWidget::setCellValue(const std::string& value)
{
    // If format is set, try to parse and reformat the value
    std::string format = getFormat();
    if (!format.empty())
    {
        try
        {
            // Try to parse value as double
            double numValue = std::stod(value);
            
            // Determine format type
            bool isDecimal = format.find('f') != std::string::npos || 
                           format.find('e') != std::string::npos ||
                           format.find('g') != std::string::npos;
            bool isInteger = format.find('d') != std::string::npos ||
                           format.find('i') != std::string::npos;
            
            if (isDecimal)
            {
                // Format as decimal - extract precision if specified
                int precision = 2;  // Default precision
                size_t dotPos = format.find('.');
                if (dotPos != std::string::npos && dotPos + 1 < format.length())
                {
                    char precChar = format[dotPos + 1];
                    if (std::isdigit(precChar))
                        precision = precChar - '0';
                }
                ui->valueLabel->setText(QString::number(numValue, 'f', precision));
            }
            else if (isInteger)
            {
                // Format as integer
                ui->valueLabel->setText(QString::number(static_cast<long long>(numValue)));
            }
            else
            {
                // Default: show as-is
                ui->valueLabel->setText(QString::fromStdString(value));
            }
        }
        catch (const std::exception&)
        {
            // If parsing fails, show value as-is
            ui->valueLabel->setText(QString::fromStdString(value));
        }
    }
    else
    {
        // No format specified, show value as-is
        ui->valueLabel->setText(QString::fromStdString(value));
    }
}

double SubstateDisplayWidget::getMinValue() const
{
    const double value = ui->minSpinBox->value();
    // Return NaN if value is the "empty" sentinel
    if (value == emptyValue)
        return std::numeric_limits<double>::quiet_NaN();
    return value;
}

void SubstateDisplayWidget::setMinValue(double value)
{
    if (std::isnan(value))
        ui->minSpinBox->setValue(emptyValue);
    else
        ui->minSpinBox->setValue(value);
}

double SubstateDisplayWidget::getMaxValue() const
{
    const double value = ui->maxSpinBox->value();
    // Return NaN if value is the "empty" sentinel
    if (value == emptyValue)
        return std::numeric_limits<double>::quiet_NaN();
    return value;
}

void SubstateDisplayWidget::setMaxValue(double value)
{
    if (std::isnan(value))
        ui->maxSpinBox->setValue(emptyValue);
    else
        ui->maxSpinBox->setValue(value);
}

std::string SubstateDisplayWidget::getFormat() const
{
    auto text = ui->formatLineEdit->text().toStdString();
    // Remove % prefix if present
    if (!text.empty() && text[0] == '%')
        return text.substr(1);
    return text;
}

void SubstateDisplayWidget::setFormat(const std::string& format)
{
    // Remove % prefix if present in input
    std::string cleanFormat = format;
    if (!cleanFormat.empty() && cleanFormat[0] == '%')
        cleanFormat = cleanFormat.substr(1);
    ui->formatLineEdit->setText(QString::fromStdString(cleanFormat));
}

bool SubstateDisplayWidget::hasMinValue() const
{
    return ui->minSpinBox->value() != ui->minSpinBox->minimum();
}

bool SubstateDisplayWidget::hasMaxValue() const
{
    return ui->maxSpinBox->value() != ui->maxSpinBox->minimum();
}

std::string SubstateDisplayWidget::fieldName() const
{
    return ui->nameLabel->text().toStdString();
}

void SubstateDisplayWidget::updateButtonState()
{
    // Buttons are enabled only if both min and max values are set
    const bool hasMin = hasMinValue();
    const bool hasMax = hasMaxValue();
    const bool isEnabled = hasMin && hasMax;
    
    // Update "Use as 3D" button state
    ui->use3dButton->setEnabled(isEnabled);
    if (isEnabled)
    {
        ui->use3dButton->setToolTip("Use this field as 3rd dimension in 3D visualization");
    }
    else
    {
        ui->use3dButton->setToolTip("Set both Min and Max values to enable 3D visualization");
    }

    // Update "Use as 2D" button state (same requirement as 3D)
    ui->use2dButton->setEnabled(isEnabled);
    if (isEnabled)
    {
        ui->use2dButton->setToolTip("Use this field as 2D visualization");
    }
    else
    {
        ui->use2dButton->setToolTip("Set both Min and Max values to enable 2D visualization");
    }

    // Emit signal with current min/max values so they can be stored in substateInfo
    emit minMaxValuesChanged(fieldName(), getMinValue(), getMaxValue());
}

void SubstateDisplayWidget::installEventFiltersOnChildren()
{
    // Install event filter on all child widgets to intercept right-click
    ui->minSpinBox->installEventFilter(this);
    ui->maxSpinBox->installEventFilter(this);
    ui->formatLineEdit->installEventFilter(this);
    ui->use3dButton->installEventFilter(this);
    ui->nameLabel->installEventFilter(this);
    ui->valueLabel->installEventFilter(this);
}

bool SubstateDisplayWidget::eventFilter(QObject* obj, QEvent* event)
{
    // Intercept right-click (context menu) on child widgets
    if (event->type() == QEvent::ContextMenu)
    {
        // Forward context menu event to this widget's contextMenuEvent
        auto contextEvent = static_cast<QContextMenuEvent*>(event);
        contextMenuEvent(contextEvent);
        return true;  // Event handled
    }
    
    // Intercept focus out on spinboxes to trigger visualization refresh
    if (event->type() == QEvent::FocusOut)
    {
        if (obj == ui->minSpinBox)
        {
            onMinSpinBoxFocusOut();
            return false;  // Let the event pass through
        }
        else if (obj == ui->maxSpinBox)
        {
            onMaxSpinBoxFocusOut();
            return false;  // Let the event pass through
        }
    }
    
    // Let other events pass through
    return QWidget::eventFilter(obj, event);
}

void SubstateDisplayWidget::contextMenuEvent(QContextMenuEvent* event)
{
    // Create context menu for the widget
    QMenu menu;
    
    // Add calculation actions with icons
    auto calcMinAction = menu.addAction(QIcon(":/icons/zoom_to.png"), "Calculate minimum");
    connect(calcMinAction, &QAction::triggered, this, &SubstateDisplayWidget::onCalculateMinimum);
    
    auto calcMinGtZeroAction = menu.addAction(QIcon(":/icons/zoom_to.png"), "Calculate minimum > 0");
    connect(calcMinGtZeroAction, &QAction::triggered, this, &SubstateDisplayWidget::onCalculateMinimumGreaterThanZero);
    
    menu.addSeparator();
    
    auto calcMaxAction = menu.addAction(QIcon(":/icons/zoom_to.png"), "Calculate maximum");
    connect(calcMaxAction, &QAction::triggered, this, &SubstateDisplayWidget::onCalculateMaximum);

    menu.addSeparator();

    auto calcMaxAndMinGtZeroAction = menu.addAction(QIcon(":/icons/zoom_to.png"), "Calculate maximum and minimum > 0");
    connect(calcMaxAndMinGtZeroAction, &QAction::triggered, this, &SubstateDisplayWidget::onCalculateMinimumGreaterThanZeroAndMaximum);
    
    // Show menu at cursor position
    menu.exec(event->globalPos());
}

void SubstateDisplayWidget::onCalculateMinimum()
{
    emit calculateMinimumRequested(fieldName());
    emit visualizationRefreshRequested();
}

void SubstateDisplayWidget::onCalculateMinimumGreaterThanZero()
{
    emit calculateMinimumGreaterThanZeroRequested(fieldName());
    emit visualizationRefreshRequested();
}

void SubstateDisplayWidget::onCalculateMaximum()
{
    emit calculateMaximumRequested(fieldName());
    emit visualizationRefreshRequested();
}

void SubstateDisplayWidget::onCalculateMinimumGreaterThanZeroAndMaximum()
{
    emit onCalculateMinimumGreaterThanZero();
    emit onCalculateMaximum();
    emit visualizationRefreshRequested();
}

void SubstateDisplayWidget::onUse2DClicked()
{
    emit use2DRequested(fieldName());
}

void SubstateDisplayWidget::setMinColor(const std::string& color)
{
    m_minColor = color;
    updateColorButtonAppearance();
    emit colorsChanged(fieldName(), m_minColor, m_maxColor);
    emit visualizationRefreshRequested();
}

void SubstateDisplayWidget::setMaxColor(const std::string& color)
{
    m_maxColor = color;
    updateColorButtonAppearance();
    emit colorsChanged(fieldName(), m_minColor, m_maxColor);
    emit visualizationRefreshRequested();
}

void SubstateDisplayWidget::onMinColorClicked()
{
    QColor currentColor = m_minColor.empty() ? QColor(Qt::white) : QColor(QString::fromStdString(m_minColor));
    QColor selectedColor = QColorDialog::getColor(currentColor, this, "Select minimum value color");
    
    if (selectedColor.isValid())
    {
        setMinColor(selectedColor.name().toStdString());
    }
}

void SubstateDisplayWidget::onMaxColorClicked()
{
    QColor currentColor = m_maxColor.empty() ? QColor(Qt::white) : QColor(QString::fromStdString(m_maxColor));
    QColor selectedColor = QColorDialog::getColor(currentColor, this, "Select maximum value color");
    
    if (selectedColor.isValid())
    {
        setMaxColor(selectedColor.name().toStdString());
    }
}

void SubstateDisplayWidget::onClearColorsClicked()
{
    setMinColor("");
    setMaxColor("");
}

void SubstateDisplayWidget::updateColorButtonAppearance()
{
    // Lambda to update a single color button
    auto updateColorButton = [](QPushButton* button, const std::string& color, const QString& label) {
        if (color.empty())
        {
            button->setStyleSheet("QPushButton { background-color: #cccccc; border: 1px solid #999999; }");
            button->setToolTip(QString("Click to set %1 value color (currently inactive)").arg(label.toLower()));
        }
        else
        {
            button->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #000000; }").arg(QString::fromStdString(color)));
            button->setToolTip(QString("%1 color: %2").arg(label, QString::fromStdString(color)));
        }
    };
    
    // Update both buttons
    updateColorButton(ui->minColorButton, m_minColor, "Min");
    updateColorButton(ui->maxColorButton, m_maxColor, "Max");
}

void SubstateDisplayWidget::onMinSpinBoxFocusOut()
{
    emit visualizationRefreshRequested();
}

void SubstateDisplayWidget::onMaxSpinBoxFocusOut()
{
    emit visualizationRefreshRequested();
}

void SubstateDisplayWidget::setActive(bool active)
{
    if (active)
    {
        // Highlight with light blue background without affecting children
        setStyleSheet("QWidget#SubstateDisplayWidget { background-color: #E3F2FD; border: 2px solid #2196F3; border-radius: 4px; padding: 2px; }");
        setAutoFillBackground(true);
    }
    else
    {
        // Remove highlight
        setStyleSheet("");
        setAutoFillBackground(false);
    }
}
