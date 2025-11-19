/** @file SubstateDisplayWidget.cpp
 * @brief Implementation of SubstateDisplayWidget. */

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QEvent>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <limits>
#include <cmath>
#include <cctype>
#include "SubstateDisplayWidget.h"


SubstateDisplayWidget::SubstateDisplayWidget(const std::string& fieldName, QWidget* parent)
    : QWidget(parent)
    , m_fieldName(fieldName)
    , m_nameLabel(new QLabel(QString::fromStdString(fieldName)))
    , m_valueLabel(new QLabel("-"))
    , m_minSpinBox(new QDoubleSpinBox())
    , m_maxSpinBox(new QDoubleSpinBox())
    , m_formatLineEdit(new QLineEdit())
    , m_use3dButton(new QPushButton("Use as 3rd dimension"))
{
    setupUI();
    connectSignals();
    
    // Enable context menu for the widget
    setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Install event filter on all child widgets to intercept right-click
    installEventFiltersOnChildren();
}

void SubstateDisplayWidget::setupUI()
{
    // Create main layout with border
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(4);

    // Set widget border
    setStyleSheet("SubstateDisplayWidget { border: 1px solid #cccccc; border-radius: 3px; background-color: #f9f9f9; }");

    // Title with field name (full width) - bold and larger
    auto titleLayout = new QHBoxLayout();
    auto titleLabel = new QLabel("Param:");
    titleLabel->setStyleSheet("QLabel { font-size: 8pt; }");
    titleLabel->setMaximumWidth(40);
    m_nameLabel->setStyleSheet("QLabel { font-size: 10pt; font-weight: bold; }");
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(m_nameLabel);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(titleLayout);

    // Value display (read-only) - full width
    auto valueLayout = new QHBoxLayout();
    auto valueTextLabel = new QLabel("Val:");
    valueTextLabel->setMaximumWidth(35);
    m_valueLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 1px; font-size: 9pt; }");
    m_valueLabel->setMaximumHeight(20);
    valueLayout->addWidget(valueTextLabel);
    valueLayout->addWidget(m_valueLabel);
    valueLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(valueLayout);

    // Two-column layout for Min and Max
    auto minMaxLayout = new QHBoxLayout();
    minMaxLayout->setSpacing(2);
    minMaxLayout->setContentsMargins(0, 0, 0, 0);

    // Minimum value (left column)
    auto minLayout = new QVBoxLayout();
    auto minTextLabel = new QLabel("Min:");
    minTextLabel->setStyleSheet("QLabel { font-size: 8pt; }");
    m_minSpinBox->setRange(-1e9, 1e9);
    m_minSpinBox->setDecimals(2);
    m_minSpinBox->setSpecialValueText(" ");  // Empty display for minimum value
    m_minSpinBox->setValue(m_minSpinBox->minimum());  // Start with "empty" state
    m_minSpinBox->setMaximumHeight(20);
    m_minSpinBox->setStyleSheet("QDoubleSpinBox { font-size: 8pt; }");
    minLayout->addWidget(minTextLabel);
    minLayout->addWidget(m_minSpinBox);
    minLayout->setContentsMargins(0, 0, 0, 0);
    minLayout->setSpacing(0);
    minMaxLayout->addLayout(minLayout);

    // Maximum value (right column)
    auto maxLayout = new QVBoxLayout();
    auto maxTextLabel = new QLabel("Max:");
    maxTextLabel->setStyleSheet("QLabel { font-size: 8pt; }");
    m_maxSpinBox->setRange(-1e9, 1e9);
    m_maxSpinBox->setDecimals(2);
    m_maxSpinBox->setSpecialValueText(" ");  // Empty display for minimum value
    m_maxSpinBox->setValue(m_maxSpinBox->minimum());  // Start with "empty" state
    m_maxSpinBox->setMaximumHeight(20);
    m_maxSpinBox->setStyleSheet("QDoubleSpinBox { font-size: 8pt; }");
    maxLayout->addWidget(maxTextLabel);
    maxLayout->addWidget(m_maxSpinBox);
    maxLayout->setContentsMargins(0, 0, 0, 0);
    maxLayout->setSpacing(0);
    minMaxLayout->addLayout(maxLayout);

    mainLayout->addLayout(minMaxLayout);

    // Format string (full width)
    auto formatLayout = new QHBoxLayout();
    auto formatTextLabel = new QLabel("Fmt:");
    formatTextLabel->setMaximumWidth(35);
    m_formatLineEdit->setMaximumHeight(20);
    m_formatLineEdit->setStyleSheet("QLineEdit { font-size: 8pt; }");
    formatLayout->addWidget(formatTextLabel);
    formatLayout->addWidget(m_formatLineEdit);
    formatLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(formatLayout);

    // Use as 3rd dimension button
    m_use3dButton->setMaximumHeight(22);
    m_use3dButton->setStyleSheet("QPushButton { font-size: 8pt; padding: 2px; }");
    mainLayout->addWidget(m_use3dButton);

    setLayout(mainLayout);
}

void SubstateDisplayWidget::connectSignals()
{
    connect(m_use3dButton, &QPushButton::clicked, this, [this]() {
        emit use3rdDimensionRequested(m_fieldName);
    });

    // Connect spinbox value changes to update button state
    connect(m_minSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SubstateDisplayWidget::updateButtonState);
    connect(m_maxSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SubstateDisplayWidget::updateButtonState);

    // Initial button state
    updateButtonState();
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
                m_valueLabel->setText(QString::number(numValue, 'f', precision));
            }
            else if (isInteger)
            {
                // Format as integer
                m_valueLabel->setText(QString::number(static_cast<long long>(numValue)));
            }
            else
            {
                // Default: show as-is
                m_valueLabel->setText(QString::fromStdString(value));
            }
        }
        catch (const std::exception&)
        {
            // If parsing fails, show value as-is
            m_valueLabel->setText(QString::fromStdString(value));
        }
    }
    else
    {
        // No format specified, show value as-is
        m_valueLabel->setText(QString::fromStdString(value));
    }
}

double SubstateDisplayWidget::getMinValue() const
{
    if (m_minSpinBox->value() == m_minSpinBox->minimum())
        return std::numeric_limits<double>::quiet_NaN();
    return m_minSpinBox->value();
}

void SubstateDisplayWidget::setMinValue(double value)
{
    if (std::isnan(value))
        m_minSpinBox->setValue(m_minSpinBox->minimum());  // Empty state
    else
        m_minSpinBox->setValue(value);
}

double SubstateDisplayWidget::getMaxValue() const
{
    if (m_maxSpinBox->value() == m_maxSpinBox->minimum())
        return std::numeric_limits<double>::quiet_NaN();
    return m_maxSpinBox->value();
}

void SubstateDisplayWidget::setMaxValue(double value)
{
    if (std::isnan(value))
        m_maxSpinBox->setValue(m_maxSpinBox->minimum());  // Empty state
    else
        m_maxSpinBox->setValue(value);
}

std::string SubstateDisplayWidget::getFormat() const
{
    auto text = m_formatLineEdit->text().toStdString();
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
    m_formatLineEdit->setText(QString::fromStdString(cleanFormat));
}

bool SubstateDisplayWidget::hasMinValue() const
{
    return m_minSpinBox->value() != m_minSpinBox->minimum();
}

bool SubstateDisplayWidget::hasMaxValue() const
{
    return m_maxSpinBox->value() != m_maxSpinBox->minimum();
}

void SubstateDisplayWidget::updateButtonState()
{
    // Button is enabled only if both min and max values are set
    const bool hasMin = hasMinValue();
    const bool hasMax = hasMaxValue();
    const bool isEnabled = hasMin && hasMax;
    
    m_use3dButton->setEnabled(isEnabled);
    
    // Update tooltip to explain why button might be disabled
    if (isEnabled)
    {
        m_use3dButton->setToolTip("Use this field as 3rd dimension in 3D visualization");
    }
    else
    {
        m_use3dButton->setToolTip("Set both Min and Max values to enable 3D visualization");
    }

    // Emit signal with current min/max values so they can be stored in substateInfo
    emit minMaxValuesChanged(m_fieldName, getMinValue(), getMaxValue());
}

void SubstateDisplayWidget::installEventFiltersOnChildren()
{
    // Install event filter on all child widgets to intercept right-click
    m_minSpinBox->installEventFilter(this);
    m_maxSpinBox->installEventFilter(this);
    m_formatLineEdit->installEventFilter(this);
    m_use3dButton->installEventFilter(this);
    m_nameLabel->installEventFilter(this);
    m_valueLabel->installEventFilter(this);
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
    emit calculateMinimumRequested(m_fieldName);
}

void SubstateDisplayWidget::onCalculateMinimumGreaterThanZero()
{
    emit calculateMinimumGreaterThanZeroRequested(m_fieldName);
}

void SubstateDisplayWidget::onCalculateMaximum()
{
    emit calculateMaximumRequested(m_fieldName);
}

void SubstateDisplayWidget::onCalculateMinimumGreaterThanZeroAndMaximum()
{
    emit onCalculateMinimumGreaterThanZero();
    emit onCalculateMaximum();
}
