/** @file SubstatesDockWidget.cpp
 * @brief Implementation of SubstatesDockWidget. */

#include <algorithm>
#include <chrono>
#include <cmath> // std::isnan
#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressDialog>
#include <QFrame>
#include <QPushButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMessageBox>
#include <QMimeData>
#include "SubstatesDockWidget.h"
#include "SubstateDisplayWidget.h"
#include "widgets/AddSubstateDialog.h"
#include "widgets/CellHistoryDialog.h"
#include "widgets/StepRangeDialog.h"
#include "visualiser/SettingParameter.h"
#include "visualiser/SubstateString.h"
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
    , m_deactivateButton(nullptr)
{
    // Initialize will be called after UI setup in MainWindow
    // Set initial size - narrower for two-column layout
    setMinimumWidth(285);
    
    // Enable drag and drop
    setAcceptDrops(true);
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

    m_plotHistoryButton = findChild<QPushButton*>("plotHistoryButton");
    if (m_plotHistoryButton)
        connect(m_plotHistoryButton, &QPushButton::clicked, this, &SubstatesDockWidget::onPlotHistoryClicked);

    // Once the panel is closed (menu, close button) it is an ordinary panel again
    connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible)
            {
                if (!visible)
                    m_keptOpen = false;
            });
}

void SubstatesDockWidget::setCurrentStep(StepIndex step)
{
    m_currentStep = step;
    if (m_historyDialog)
        m_historyDialog->setCurrentStep(step);
}

void SubstatesDockWidget::resetSelectedCell()
{
    m_hasSelectedCell = false;
    m_selectedVolumeCell.reset();
    if (auto headerLabel = findChild<QLabel*>("cellHeaderLabel"))
        headerLabel->setText(m_native3DVolumeMode ? "Cell: (x=-, y=-, z=-)" : "Cell: (x=-, y=-)");
    if (m_plotHistoryButton)
        m_plotHistoryButton->setEnabled(false);
    if (m_historyDialog)
        m_historyDialog->hide();
}

void SubstatesDockWidget::onPlotHistoryClicked()
{
    showHistoryOfSelectedCell(true);
}

void SubstatesDockWidget::showHistoryOfSelectedCell(bool showWindow)
{
    if (! m_hasSelectedCell || ! m_currentSettingParameter || ! m_currentVisualizer)
        return;
    if (! showWindow && (! m_historyDialog || ! m_historyDialog->isVisible()))
        return;

    const auto fields = m_currentSettingParameter->getSubstateFields();
    if (fields.empty())
        return;

    // Reading goes through the files of all steps: let the user see the progress and cancel it
    QProgressDialog progressDialog(tr("Reading the cell from all steps..."), tr("Cancel"), 0, 0, window());
    progressDialog.setWindowTitle(tr("Cell history"));
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(400); // quick readings do not flash a dialog

    const CellHistoryProgress progress = [&progressDialog](std::size_t done, std::size_t total)
    {
        progressDialog.setMaximum(static_cast<int>(total));
        progressDialog.setValue(static_cast<int>(done));
        return ! progressDialog.wasCanceled();
    };

    const CellHistory history = m_selectedVolumeCell
        ? m_currentVisualizer->cellHistoryOfVolumeCell(*m_currentSettingParameter, *m_selectedVolumeCell, fields, progress)
        : m_currentVisualizer->cellHistory(*m_currentSettingParameter, m_selectedRow, m_selectedCol, fields, progress);
    progressDialog.close();

    if (history.empty())
        return;

    if (! m_historyDialog)
    {
        // Child of the main window, not of this dock: the dock hides when the user clicks outside the grid
        m_historyDialog = new CellHistoryDialog(parentWidget() ? parentWidget() : window());
    }

    // Preselect the substate the user works with: the one highlighted in the dock
    std::string preferred = fields.front();
    if (const auto* active = getActiveSubstateWidget())
        preferred = active->fieldName();

    m_historyDialog->setHistory(history, preferred);
    if (m_currentStep)
        m_historyDialog->setCurrentStep(*m_currentStep);

    if (showWindow)
    {
        m_historyDialog->show();
        m_historyDialog->raise();
        m_historyDialog->activateWindow();
    }
}

void SubstatesDockWidget::updateSubstates(SettingParameter* settingParameter)
{
    if (!settingParameter)
        return;

    // Store reference to current setting parameter
    m_currentSettingParameter = settingParameter;

    // The data were (re)loaded: a previously clicked cell and its history are no longer valid
    resetSelectedCell();
    m_lastStepRange.reset();

    // Clear existing widgets
    clearWidgets();

    // Get substate fields
    auto fields = settingParameter->getSubstateFields();

    // Create new widgets for each field (they go before the footer, which is created once)
    for (size_t i = 0; i < fields.size(); ++i)
    {
        const auto& field = fields[i];

        auto widget = createSubstateWidget(field, *settingParameter);
        m_containerLayout->insertWidget(substateInsertIndex(), widget);
        m_substateWidgets[field] = widget;

        // Add separator between widgets (but not after the last one)
        if (i < fields.size() - 1)
        {
            m_containerLayout->insertWidget(substateInsertIndex(), createSeparator());
        }
    }

    ensureFooter();
}

SubstateDisplayWidget* SubstatesDockWidget::createSubstateWidget(const std::string& field, const SettingParameter& settingParameter)
{
    auto widget = new SubstateDisplayWidget(field, m_containerWidget);

    // Restore saved parameters if they exist
    auto it = settingParameter.substateInfo.find(field);
    if (it != settingParameter.substateInfo.end())
    {
        widget->setMinValue(it->second.minValue);
        widget->setMaxValue(it->second.maxValue);
        widget->setFormat(it->second.format);
        widget->setMinColor(it->second.minColor);
        widget->setMaxColor(it->second.maxColor);
        widget->setNoValue(it->second.noValue);
        widget->setAltitudeScale(it->second.altitudeScale);
        // Restore noValue enabled state from saved configuration
        widget->setNoValueEnabled(it->second.noValueEnabled);
    }

    widget->setNative3DVolumeMode(m_native3DVolumeMode);

    // Connect signals
    connect(widget, &SubstateDisplayWidget::use3dStateChanged, this, &SubstatesDockWidget::onUse3DStateChanged);
    connect(widget, &SubstateDisplayWidget::useSubstateColorringRequested, this, &SubstatesDockWidget::onUseSubstateColorringRequested);
    connect(widget, QOverload<const std::string&, double, double>::of(&SubstateDisplayWidget::minMaxValuesChanged), this, &SubstatesDockWidget::onMinMaxValuesChanged);
    connect(widget, &SubstateDisplayWidget::calculateMinimumRequested, this, &SubstatesDockWidget::onCalculateMinimumRequested);
    connect(widget, &SubstateDisplayWidget::calculateMinimumGreaterThanZeroRequested, this, &SubstatesDockWidget::onCalculateMinimumGreaterThanZeroRequested);
    connect(widget, &SubstateDisplayWidget::calculateMaximumRequested, this, &SubstatesDockWidget::onCalculateMaximumRequested);
    connect(widget, &SubstateDisplayWidget::calculateOverStepsRequested, this, &SubstatesDockWidget::onCalculateOverStepsRequested);
    connect(widget, &SubstateDisplayWidget::colorsChanged, this, &SubstatesDockWidget::onColorsChanged);
    connect(widget, QOverload<const std::string&, double, bool>::of(&SubstateDisplayWidget::noValueChanged), this, &SubstatesDockWidget::onNoValueChanged);
    connect(widget, &SubstateDisplayWidget::altitudeScaleChanged, this, &SubstatesDockWidget::onAltitudeScaleChanged);
    connect(widget, &SubstateDisplayWidget::visualizationRefreshRequested, this, &SubstatesDockWidget::visualizationRefreshRequested);
    return widget;
}

int SubstatesDockWidget::substateInsertIndex() const
{
    return m_footerCreated ? m_containerLayout->count() - kFooterItems : m_containerLayout->count();
}

void SubstatesDockWidget::ensureFooter()
{
    if (m_footerCreated)
        return;

    // Push widgets to the top
    m_containerLayout->addStretch();

    m_addSubstateButton = new QPushButton(tr("Add substate..."));
    m_addSubstateButton->setMaximumHeight(24);
    m_addSubstateButton->setStyleSheet("QPushButton { font-size: 9pt; padding: 3px; }");
    m_addSubstateButton->setToolTip(tr("Add a substate which is not listed in Header.txt (the model has to return a number for its name)"));
    connect(m_addSubstateButton, &QPushButton::clicked, this, &SubstatesDockWidget::onAddSubstateClicked);
    m_containerLayout->addWidget(m_addSubstateButton);

    if (! m_deactivateButton)
    {
        m_deactivateButton = new QPushButton("Deactivate Substate");
        m_deactivateButton->setMaximumHeight(24);
        m_deactivateButton->setStyleSheet("QPushButton { font-size: 9pt; padding: 3px; background-color: #f8e8e8; }");
        connect(m_deactivateButton, &QPushButton::clicked, this, &SubstatesDockWidget::onDeactivateClicked);
    }
    m_containerLayout->addWidget(m_deactivateButton);

    m_footerCreated = true;
}

void SubstatesDockWidget::setVisualizer(ISceneWidgetVisualizer* visualizer)
{
    m_currentVisualizer = visualizer;
}

SubstateValueSource SubstatesDockWidget::makeValueSource() const
{
    SubstateValueSource source;
    if (! m_currentSettingParameter || ! m_currentVisualizer)
        return source;

    const auto rows = static_cast<std::size_t>(std::max(0, m_currentSettingParameter->numberOfRowsY));
    const auto columns = static_cast<std::size_t>(std::max(0, m_currentSettingParameter->numberOfColumnX));
    const auto layers = m_native3DVolumeMode ? static_cast<std::size_t>(std::max(1, m_currentSettingParameter->numberOfSlicesZ)) : std::size_t{1};

    source.cellCount = rows * columns * layers;
    if (source.cellCount == 0)
        return source;

    ISceneWidgetVisualizer* visualizer = m_currentVisualizer;
    const bool volume = m_native3DVolumeMode;
    source.textOfCell = [visualizer, rows, columns, volume](const std::string& name, std::size_t index)
    {
        const std::size_t perLayer = rows * columns;
        const std::size_t layer = index / perLayer;
        const std::size_t inLayer = index % perLayer;
        const std::size_t row = inLayer / columns;
        const std::size_t column = inLayer % columns;
        if (volume)
            return visualizer->getVolumeCellStringEncoding(GridCellIndex{row, column, layer}, name.c_str());
        return visualizer->getCellStringEncoding(static_cast<int>(row), static_cast<int>(column), name.c_str());
    };
    return source;
}

void SubstatesDockWidget::onAddSubstateClicked()
{
    if (! m_currentSettingParameter || ! m_currentVisualizer)
    {
        QMessageBox::information(this, tr("Add substate"), tr("Load a simulation first: the name of a substate is checked against its cells."));
        return;
    }

    // What the user changed in the panel must be in the line which the dialog writes for Header.txt
    saveParametersToSettings(m_currentSettingParameter);

    std::vector<SubstateInfo> existing;
    for (const auto& field : m_currentSettingParameter->getSubstateFields())
    {
        auto it = m_currentSettingParameter->substateInfo.find(field);
        if (it != m_currentSettingParameter->substateInfo.end())
        {
            existing.push_back(it->second);
        }
        else
        {
            SubstateInfo info;
            info.name = field;
            existing.push_back(info);
        }
    }

    AddSubstateDialog dialog(makeValueSource(), std::move(existing), window());
    if (dialog.exec() != QDialog::Accepted)
        return;

    SubstateInfo info = dialog.substate();
    if (m_substateWidgets.count(info.name) != 0)
        return; // the dialog does not allow it; never replace a widget which is in use

    // The list of the panel is built from this line, so the new entry goes there too
    std::string& line = m_currentSettingParameter->substates;
    line += (line.empty() ? "" : ",") + SubstateString::formatEntry(info);
    m_currentSettingParameter->substateInfo[info.name] = info;

    auto* widget = createSubstateWidget(info.name, *m_currentSettingParameter);
    if (! m_substateWidgets.empty())
        m_containerLayout->insertWidget(substateInsertIndex(), createSeparator());
    m_containerLayout->insertWidget(substateInsertIndex(), widget);
    m_substateWidgets[info.name] = widget;
}

void SubstatesDockWidget::onUseSubstateColorringRequested(const std::string&)
{
    std::vector<std::string> fieldNames;
    for (auto& [name, widget] : m_substateWidgets)
    {
        if (widget->isUse2DChecked())
        {
            fieldNames.push_back(name);
        }
    }

    emit useSubstatesColorringRequested(fieldNames);
}

void SubstatesDockWidget::updateCellValues(SettingParameter* settingParameter, int row, int col, class ISceneWidgetVisualizer* visualizer)
{
    if (!settingParameter || !visualizer)
        return;

    // Store visualizer for later use in calculations
    m_currentVisualizer = visualizer;

    // Remember the cell for the plot of its history
    m_hasSelectedCell = true;
    m_selectedRow = row;
    m_selectedCol = col;
    m_selectedVolumeCell.reset();
    if (m_plotHistoryButton)
        m_plotHistoryButton->setEnabled(true);

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

    // When the history window is open it follows the clicked cell
    showHistoryOfSelectedCell(false);
}

void SubstatesDockWidget::updateVolumeCellValues(SettingParameter* settingParameter, const GridCellIndex& cell, class ISceneWidgetVisualizer* visualizer)
{
    if (!settingParameter || !visualizer)
        return;

    m_currentVisualizer = visualizer;

    // Remember the cell for the plot of its history
    m_hasSelectedCell = true;
    m_selectedRow = static_cast<int>(cell.row);
    m_selectedCol = static_cast<int>(cell.column);
    m_selectedVolumeCell = cell;
    if (m_plotHistoryButton)
        m_plotHistoryButton->setEnabled(true);

    // The whole 3D grid: all three coordinates identify the cell (x=col, y=row, z=layer)
    if (auto headerLabel = findChild<QLabel*>("cellHeaderLabel"))
    {
        headerLabel->setText(QString("Cell: (x=%1, y=%2, z=%3)").arg(cell.column).arg(cell.row).arg(cell.layer));
    }

    for (auto& pair : m_substateWidgets)
    {
        const std::string cellValue = visualizer->getVolumeCellStringEncoding(cell, pair.first.c_str());
        if (!cellValue.empty())
            pair.second->setCellValue(cellValue);
    }

    // When the history window is open it follows the clicked cell
    showHistoryOfSelectedCell(false);
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
            it->second.altitudeScale = widget->getAltitudeScale();
        }
    }
}

void SubstatesDockWidget::clearWidgets()
{
    // Remove all widgets from layout except:
    // - First 2 items: label + separator (header)
    // - Last kFooterItems items: stretch + buttons (footer), if it was created
    const int keep = 2 + (m_footerCreated ? kFooterItems : 0);
    while (m_containerLayout->count() > keep)
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

void SubstatesDockWidget::onMinMaxValuesChanged(const std::string& fieldName, double minValue, double maxValue)
{
    // Update the substateInfo in SettingParameter with the new values
    if (m_currentSettingParameter)
    {
        auto it = m_currentSettingParameter->substateInfo.find(fieldName);
        if (it != m_currentSettingParameter->substateInfo.end())
        {
            it->second.minValue = minValue;
            it->second.maxValue = maxValue;
        }
    }
}

void SubstatesDockWidget::onCalculateMinimumRequested(const std::string& fieldName)
{
    if (!m_currentSettingParameter || !m_currentVisualizer)
        return;

    double minValue = std::numeric_limits<double>::max();
    bool found = false;

    // Get noValue if enabled
    double noValue = std::numeric_limits<double>::quiet_NaN();
    bool noValueEnabled = false;
    auto substateIt = m_currentSettingParameter->substateInfo.find(fieldName);
    if (substateIt != m_currentSettingParameter->substateInfo.end())
    {
        noValue = substateIt->second.noValue;
        noValueEnabled = substateIt->second.noValueEnabled;
    }

    // Iterate through all cells in current step
    for (int row = 0; row < m_currentSettingParameter->numberOfRowsY; ++row)
    {
        for (int col = 0; col < m_currentSettingParameter->numberOfColumnX; ++col)
        {
            try
            {
                std::string cellValueStr = m_currentVisualizer->getCellStringEncoding(row, col, fieldName.c_str());
                double cellValue = std::stod(cellValueStr);
                
                // Skip noValue if it's enabled
                if (noValueEnabled && !std::isnan(noValue) && cellValue == noValue)
                    continue;
                
                minValue = std::min(minValue, cellValue);
                found = true;
            }
            catch (const std::exception&)
            {
                // Skip cells that can't be parsed
            }
        }
    }

    if (found)
    {
        // Update the widget
        auto it = m_substateWidgets.find(fieldName);
        if (it != m_substateWidgets.end())
        {
            it->second->setMinValue(minValue);
        }
    }
}

void SubstatesDockWidget::onCalculateMinimumGreaterThanZeroRequested(const std::string& fieldName)
{
    if (!m_currentSettingParameter || !m_currentVisualizer)
        return;

    double minValue = std::numeric_limits<double>::max();
    bool found = false;

    // Iterate through all cells in current step
    for (int row = 0; row < m_currentSettingParameter->numberOfRowsY; ++row)
    {
        for (int col = 0; col < m_currentSettingParameter->numberOfColumnX; ++col)
        {
            try
            {
                std::string cellValueStr = m_currentVisualizer->getCellStringEncoding(row, col, fieldName.c_str());
                double cellValue = std::stod(cellValueStr);
                if (cellValue > 0.0)
                {
                    minValue = std::min(minValue, cellValue);
                    found = true;
                }
            }
            catch (const std::exception&)
            {
                // Skip cells that can't be parsed
            }
        }
    }

    if (found)
    {
        // Update the widget
        auto it = m_substateWidgets.find(fieldName);
        if (it != m_substateWidgets.end())
        {
            it->second->setMinValue(minValue);
        }
    }
}

void SubstatesDockWidget::onCalculateMaximumRequested(const std::string& fieldName)
{
    if (!m_currentSettingParameter || !m_currentVisualizer)
        return;

    double maxValue = std::numeric_limits<double>::lowest();
    bool found = false;

    // Get noValue if enabled
    double noValue = std::numeric_limits<double>::quiet_NaN();
    bool noValueEnabled = false;
    auto substateIt = m_currentSettingParameter->substateInfo.find(fieldName);
    if (substateIt != m_currentSettingParameter->substateInfo.end())
    {
        noValue = substateIt->second.noValue;
        noValueEnabled = substateIt->second.noValueEnabled;
    }

    // Iterate through all cells in current step
    for (int row = 0; row < m_currentSettingParameter->numberOfRowsY; ++row)
    {
        for (int col = 0; col < m_currentSettingParameter->numberOfColumnX; ++col)
        {
            try
            {
                std::string cellValueStr = m_currentVisualizer->getCellStringEncoding(row, col, fieldName.c_str());
                double cellValue = std::stod(cellValueStr);
                
                // Skip noValue if it's enabled
                if (noValueEnabled && !std::isnan(noValue) && cellValue == noValue)
                    continue;
                
                maxValue = std::max(maxValue, cellValue);
                found = true;
            }
            catch (const std::exception&)
            {
                // Skip cells that can't be parsed
            }
        }
    }

    if (found)
    {
        // Update the widget
        auto it = m_substateWidgets.find(fieldName);
        if (it != m_substateWidgets.end())
        {
            it->second->setMaxValue(maxValue);
        }
    }
}

namespace
{
/** @brief Time as `42 s`, `3 min 05 s` or `1 h 02 min`, for the estimate of the time left. */
QString formatDuration(std::chrono::seconds time)
{
    const auto total = std::max<long long>(0, time.count());
    if (total < 60)
        return QObject::tr("%1 s").arg(total);
    if (total < 3600)
        return QObject::tr("%1 min %2 s").arg(total / 60).arg(total % 60, 2, 10, QChar('0'));
    return QObject::tr("%1 h %2 min").arg(total / 3600).arg((total % 3600) / 60, 2, 10, QChar('0'));
}

/** @brief Text of the progress window: the step being read and the estimate of the time left. */
QString progressText(const QString& fieldName, const SubstateRangeProgress& state, std::chrono::steady_clock::duration elapsed)
{
    QString text = QObject::tr("Looking for the extremes of '%1'\nStep %2 (%3 of %4)")
                       .arg(fieldName)
                       .arg(state.step)
                       .arg(state.stepNumber)
                       .arg(state.stepsTotal);

    // The estimate needs some work done, or it jumps wildly in the first moments
    if (state.fraction > 0.02 && elapsed > std::chrono::seconds(1))
    {
        const double secondsElapsed = std::chrono::duration<double>(elapsed).count();
        const auto left = std::chrono::seconds(static_cast<long long>(secondsElapsed * (1.0 - state.fraction) / state.fraction));
        text += QObject::tr("\nTime left: about %1").arg(formatDuration(left));
    }
    return text;
}
} // namespace

void SubstatesDockWidget::onCalculateOverStepsRequested(const std::string& fieldName)
{
    if (! m_currentSettingParameter || ! m_currentVisualizer)
        return;

    const std::vector<StepIndex> availableSteps = m_currentVisualizer->availableSteps();
    if (availableSteps.empty())
    {
        QMessageBox::information(this, tr("Minimum / maximum over steps"), tr("The simulation has no saved steps to look through."));
        return;
    }

    const StepRange everything = StepRanges::boundsOf(availableSteps).value_or(StepRange{});
    const StepRange initialRange = m_lastStepRange.value_or(everything);

    StepRangeDialog dialog(QString::fromStdString(fieldName), availableSteps, initialRange, true, true, window());
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_lastStepRange = dialog.selectedRange();
    const std::vector<StepIndex> steps = dialog.stepsToRead();
    const bool wantsMinimum = dialog.calculatesMinimum();
    const bool wantsMaximum = dialog.calculatesMaximum();

    // The value which means "no data" is not a measurement, like in the calculation for one step
    std::optional<double> noValue;
    const auto substateIt = m_currentSettingParameter->substateInfo.find(fieldName);
    if (substateIt != m_currentSettingParameter->substateInfo.end() &&
        substateIt->second.noValueEnabled &&
        ! std::isnan(substateIt->second.noValue))
    {
        noValue = substateIt->second.noValue;
    }

    // Reading goes through the files of every step: the user sees how far it is and can cancel
    constexpr int progressResolution = 1000;
    QProgressDialog progressDialog(tr("Preparing..."), tr("Cancel"), 0, progressResolution, window());
    progressDialog.setWindowTitle(tr("Minimum / maximum over steps"));
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(0);
    progressDialog.setMinimumWidth(380);
    progressDialog.setAutoClose(false);
    progressDialog.setAutoReset(false);
    progressDialog.setValue(0);

    const QString fieldText = QString::fromStdString(fieldName);
    const auto startedAt = std::chrono::steady_clock::now();
    int shownValue = 0;
    StepIndex shownStep = steps.front();
    auto lastTextUpdate = startedAt;

    const SubstateRangeProgressCallback progress = [&](const SubstateRangeProgress& state)
    {
        const auto now = std::chrono::steady_clock::now();
        const int value = static_cast<int>(state.fraction * progressResolution);
        const bool newStep = state.step != shownStep;

        // Painting is the expensive part: only when the bar moves, and the text about 5 times a second
        if (value != shownValue || newStep)
        {
            shownValue = value;
            progressDialog.setValue(value);
        }
        if (newStep || now - lastTextUpdate > std::chrono::milliseconds(200))
        {
            shownStep = state.step;
            lastTextUpdate = now;
            progressDialog.setLabelText(progressText(fieldText, state, now - startedAt));
        }

        // The window is modal, so only its own button (Cancel) takes input; this lets it be clicked
        QApplication::processEvents();
        return ! progressDialog.wasCanceled();
    };

    SubstateRange range;
    QString failure;
    try
    {
        range = m_currentVisualizer->substateRangeAcrossSteps(*m_currentSettingParameter, fieldName, steps, noValue, progress);
    }
    catch (const std::exception& error)
    {
        failure = QString::fromUtf8(error.what());
    }
    progressDialog.close();

    if (! failure.isEmpty())
    {
        QMessageBox::warning(this, tr("Minimum / maximum over steps"), tr("The steps could not be read: %1").arg(failure));
        return;
    }
    if (range.cancelled)
        return; // a partial result would be misleading

    if (! range.hasValues())
    {
        QMessageBox::information(this,
                                 tr("Minimum / maximum over steps"),
                                 tr("No numeric value of '%1' was found in the chosen steps.").arg(fieldText));
        return;
    }

    if (auto it = m_substateWidgets.find(fieldName); it != m_substateWidgets.end())
    {
        if (wantsMinimum)
            it->second->setMinValue(*range.minimum);
        if (wantsMaximum)
            it->second->setMaxValue(*range.maximum);
    }
    emit visualizationRefreshRequested();

    if (! range.failedSteps.empty())
    {
        constexpr std::size_t maxListed = 10;
        QStringList listed;
        for (std::size_t i = 0; i < range.failedSteps.size() && i < maxListed; ++i)
            listed << QString::number(range.failedSteps[i]);
        if (range.failedSteps.size() > maxListed)
            listed << QStringLiteral("...");

        QMessageBox::warning(this,
                             tr("Minimum / maximum over steps"),
                             tr("%1 of %2 steps could not be read and were left out: %3.")
                                 .arg(range.failedSteps.size())
                                 .arg(steps.size())
                                 .arg(listed.join(QStringLiteral(", "))));
    }
}

void SubstatesDockWidget::onDeactivateClicked()
{
    emit deactivateRequested();
}

void SubstatesDockWidget::onColorsChanged(const std::string& fieldName, const std::string& minColor, const std::string& maxColor)
{
    // Update the substateInfo in SettingParameter with the new colors
    if (m_currentSettingParameter)
    {
        auto it = m_currentSettingParameter->substateInfo.find(fieldName);
        if (it != m_currentSettingParameter->substateInfo.end())
        {
            it->second.minColor = minColor;
            it->second.maxColor = maxColor;
        }
    }
}

void SubstatesDockWidget::setActiveSubstate(const std::string& fieldName)
{
    if (fieldName.empty())
    {
        setActiveSubstates({});
    }
    else
    {
        setActiveSubstates({fieldName});
    }
}

void SubstatesDockWidget::setActiveSubstates(const std::vector<std::string>& fieldNames)
{
    // Deactivate all widgets first
    for (auto& [name, widget] : m_substateWidgets)
    {
        const bool active = std::find(fieldNames.begin(), fieldNames.end(), name) != fieldNames.end();
        widget->setActive(active);
    }
}

std::vector<std::string> SubstatesDockWidget::checked3DSubstatesInDisplayOrder() const
{
    std::vector<std::string> checkedFields;

    if (!m_containerLayout)
        return checkedFields;

    for (int i = 0; i < m_containerLayout->count(); ++i)
    {
        QLayoutItem* item = m_containerLayout->itemAt(i);
        QWidget* widget = item ? item->widget() : nullptr;
        if (!widget)
            continue;

        for (const auto& [name, substateWidget] : m_substateWidgets)
        {
            if (substateWidget == widget && substateWidget->isUse3DChecked())
            {
                checkedFields.push_back(name);
                break;
            }
        }
    }

    return checkedFields;
}

class SubstateDisplayWidget* SubstatesDockWidget::getActiveSubstateWidget() const
{
    // Find the active widget in the map
    for (const auto& pair : m_substateWidgets)
    {
        if (pair.second && pair.second->isActive())
        {
            return pair.second;
        }
    }
    return nullptr;
}

void SubstatesDockWidget::onNoValueChanged(const std::string& fieldName, double noValue, bool isEnabled)
{
    // Update substateInfo with new noValue and enabled state
    if (m_currentSettingParameter)
    {
        auto it = m_currentSettingParameter->substateInfo.find(fieldName);
        if (it != m_currentSettingParameter->substateInfo.end())
        {
            it->second.noValue = noValue;
            it->second.noValueEnabled = isEnabled;
        }
    }
}

void SubstatesDockWidget::onAltitudeScaleChanged(const std::string& fieldName, double scale)
{
    if (!m_currentSettingParameter)
        return;

    auto it = m_currentSettingParameter->substateInfo.find(fieldName);
    if (it != m_currentSettingParameter->substateInfo.end())
    {
        it->second.altitudeScale = scale;
    }
}

void SubstatesDockWidget::onUse2DCheckboxChanged(const std::string& fieldName)
{
    // If fieldName is empty, it means the checkbox was unchecked - do nothing
    if (fieldName.empty())
        return;
    
    // Uncheck all other checkboxes (exclusive behavior)
    for (auto& [name, widget] : m_substateWidgets)
    {
        if (name != fieldName)
        {
            widget->setUse2DChecked(false);
        }
    }
}

void SubstatesDockWidget::uncheckAllUse2DCheckboxes()
{
    // Uncheck all use2D checkboxes
    for (auto& [name, widget] : m_substateWidgets)
    {
        widget->setUse2DChecked(false);
    }
}

void SubstatesDockWidget::uncheckAllUse3DCheckboxes()
{
    for (auto& [name, widget] : m_substateWidgets)
    {
        QSignalBlocker blocker(widget);
        widget->setUse3DChecked(false);
    }
}

void SubstatesDockWidget::setNative3DVolumeMode(bool enabled)
{
    m_native3DVolumeMode = enabled;
    for (auto& [name, widget] : m_substateWidgets)
        widget->setNative3DVolumeMode(enabled);
}

void SubstatesDockWidget::dragEnterEvent(QDragEnterEvent* event)
{
    // Accept the drag if it contains substate field data
    if (event->mimeData()->hasFormat("application/x-substate-field"))
    {
        event->acceptProposedAction();
    }
}

void SubstatesDockWidget::dropEvent(QDropEvent* event)
{
    // Get the dragged field name
    QByteArray fieldData = event->mimeData()->data("application/x-substate-field");
    std::string draggedField = QString::fromUtf8(fieldData).toStdString();
    
    // Reorder the widgets
    reorderWidgets(draggedField, event->pos());
    
    event->acceptProposedAction();
}

void SubstatesDockWidget::reorderWidgets(const std::string& draggedField, const QPoint& dropPosition)
{
    // Find the dragged widget
    auto draggedIt = m_substateWidgets.find(draggedField);
    if (draggedIt == m_substateWidgets.end())
        return;
    
    SubstateDisplayWidget* draggedWidget = draggedIt->second;
    
    // Find the widget at the drop position
    QWidget* dropWidget = m_containerWidget->childAt(dropPosition);
    if (!dropWidget)
        return;
    
    // Find which substate widget was dropped on
    std::string targetField;
    for (const auto& [name, widget] : m_substateWidgets)
    {
        if (widget == dropWidget || widget->isAncestorOf(dropWidget))
        {
            targetField = name;
            break;
        }
    }
    
    if (targetField.empty() || targetField == draggedField)
        return;
    
    // Get current positions in layout
    int draggedIndex = m_containerLayout->indexOf(draggedWidget);
    int targetIndex = m_containerLayout->indexOf(m_substateWidgets[targetField]);
    
    if (draggedIndex == -1 || targetIndex == -1)
        return;
    
    // Remove the dragged widget from its current position
    m_containerLayout->removeWidget(draggedWidget);
    
    // Insert it at the new position
    m_containerLayout->insertWidget(targetIndex, draggedWidget);
    
    // Update separators by finding and removing old ones, then adding new ones
    // Remove all existing separators from the layout
    for (int i = m_containerLayout->count() - 1; i >= 0; --i)
    {
        QLayoutItem* item = m_containerLayout->itemAt(i);
        if (QFrame* frame = qobject_cast<QFrame*>(item->widget()))
        {
            if (frame->frameShape() == QFrame::HLine)
            {
                m_containerLayout->removeItem(item);
                delete frame;
            }
        }
    }
    
    // Add new separators between widgets
    std::vector<SubstateDisplayWidget*> widgetList;
    for (const auto& [name, widget] : m_substateWidgets)
    {
        widgetList.push_back(widget);
    }
    
    // Re-add separators in the new order
    for (size_t i = 0; i < widgetList.size(); ++i)
    {
        int widgetIndex = m_containerLayout->indexOf(widgetList[i]);
        if (widgetIndex != -1 && i < widgetList.size() - 1)
        {
            m_containerLayout->insertWidget(widgetIndex + 1, createSeparator());
        }
    }
    
    // Save the new order to SettingParameter
    saveFieldOrder();
    emit use3dSubstateOrderChanged();
}

void SubstatesDockWidget::saveFieldOrder()
{
    if (!m_currentSettingParameter)
        return;
    
    // Get the current order of widgets from the layout
    std::vector<std::string> orderedFields;
    
    // Iterate through the layout to find substate widgets in order
    for (int i = 0; i < m_containerLayout->count(); ++i)
    {
        QLayoutItem* item = m_containerLayout->itemAt(i);
        QWidget* widget = item->widget();
        
        // Check if this widget is a SubstateDisplayWidget
        for (const auto& [name, substateWidget] : m_substateWidgets)
        {
            if (substateWidget == widget)
            {
                orderedFields.push_back(name);
                break;
            }
        }
    }
    
    // Update the order in SettingParameter's substateInfo
    for (size_t i = 0; i < orderedFields.size(); ++i)
    {
        const std::string& fieldName = orderedFields[i];
        auto it = m_currentSettingParameter->substateInfo.find(fieldName);
        if (it != m_currentSettingParameter->substateInfo.end())
        {
            it->second.order = static_cast<int>(i);
        }
    }
}

void SubstatesDockWidget::onUse3DStateChanged(const std::string& fieldName, bool checked)
{
    // Forward the signal to parent (e.g., MainWindow)
    emit use3dStateChanged(fieldName, checked);
}
