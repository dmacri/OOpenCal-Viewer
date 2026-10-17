#include "widgets/CellHistoryDialog.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "widgets/TimeSeriesPlot.h"

CellHistoryDialog::CellHistoryDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Cell history"));
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(680, 480);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("QLabel { font-weight: bold; }");

    m_fieldComboBox = new QComboBox(this);
    m_fieldComboBox->setToolTip(tr("Substate to plot"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(m_titleLabel, 1);
    topRow->addWidget(new QLabel(tr("Substate:"), this));
    topRow->addWidget(m_fieldComboBox);

    m_plot = new TimeSeriesPlot(this);
    m_summaryLabel = new QLabel(this);

    auto* buttons = new QDialogButtonBox(this);
    auto* exportButton = buttons->addButton(tr("Export CSV..."), QDialogButtonBox::ActionRole);
    exportButton->setToolTip(tr("Save the values of all substates of this cell as a CSV file"));
    buttons->addButton(QDialogButtonBox::Close);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(m_plot, 1);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(buttons);

    connect(m_fieldComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CellHistoryDialog::onFieldChanged);
    connect(exportButton, &QPushButton::clicked, this, &CellHistoryDialog::onExportCsv);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
}

void CellHistoryDialog::setHistory(const CellHistory& history, const std::string& preferredField)
{
    // Keep the substate the user was looking at when another cell is shown
    const QString previous = m_fieldComboBox->currentText();

    m_history = history;

    {
        QSignalBlocker blocker(m_fieldComboBox);
        m_fieldComboBox->clear();
        for (const auto& field : m_history.fields)
            m_fieldComboBox->addItem(QString::fromStdString(field));

        int selected = m_fieldComboBox->findText(previous);
        if (selected < 0)
            selected = m_fieldComboBox->findText(QString::fromStdString(preferredField));
        m_fieldComboBox->setCurrentIndex(std::max(selected, 0));
    }

    QString title = tr("Cell (x=%1, y=%2").arg(m_history.column).arg(m_history.row);
    if (m_history.threeDimensional)
        title += tr(", z=%1").arg(m_history.slice);
    m_titleLabel->setText(title + ")");

    updatePlot();
}

void CellHistoryDialog::setCurrentStep(StepIndex step)
{
    m_currentStep = step;
    m_plot->setMarkerX(static_cast<double>(step));
}

void CellHistoryDialog::onFieldChanged()
{
    updatePlot();
}

void CellHistoryDialog::updatePlot()
{
    const int index = m_fieldComboBox->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= m_history.values.size())
    {
        m_plot->setSeries({}, {});
        m_summaryLabel->setText(tr("No data"));
        return;
    }

    const auto& values = m_history.values[static_cast<std::size_t>(index)];
    std::vector<double> x;
    x.reserve(m_history.steps.size());
    for (const auto step : m_history.steps)
        x.push_back(static_cast<double>(step));

    const QString field = m_fieldComboBox->currentText();
    m_plot->setAxisLabels(tr("Step"), field);
    m_plot->setSeries(std::move(x), values);
    m_plot->setMarkerX(m_currentStep ? std::optional<double>(static_cast<double>(*m_currentStep)) : std::nullopt);

    const std::size_t valid = m_history.validCount(static_cast<std::size_t>(index));
    if (valid == 0)
    {
        m_summaryLabel->setText(tr("'%1' has no numeric values for this cell").arg(field));
        return;
    }

    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (const double v : values)
    {
        if (std::isfinite(v))
        {
            minimum = std::min(minimum, v);
            maximum = std::max(maximum, v);
        }
    }

    QString summary = tr("%1 of %2 steps, min %3, max %4")
                          .arg(valid)
                          .arg(m_history.steps.size())
                          .arg(QString::number(minimum, 'g', 8))
                          .arg(QString::number(maximum, 'g', 8));
    if (m_history.cancelled)
        summary += tr(" (reading was cancelled, the plot is incomplete)");
    m_summaryLabel->setText(summary);
}

void CellHistoryDialog::onExportCsv()
{
    if (m_history.empty())
        return;

    const QString suggested = QString("cell_x%1_y%2%3.csv")
                                  .arg(m_history.column)
                                  .arg(m_history.row)
                                  .arg(m_history.threeDimensional ? QString("_z%1").arg(m_history.slice) : QString());
    const QString fileName = QFileDialog::getSaveFileName(this, tr("Export cell history"), suggested, tr("CSV files (*.csv)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (! file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Export cell history"), tr("Cannot write '%1': %2").arg(fileName, file.errorString()));
        return;
    }

    const std::string csv = cellHistoryToCsv(m_history);
    file.write(csv.data(), static_cast<qint64>(csv.size()));
}
