#include "AddSubstateDialog.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "visualiser/SubstateString.h"

namespace
{
/// The check asks at most about this many cells, so it is quick also for a huge model (min and max look at all of them)
constexpr std::size_t maxCellsOfTheCheck = 200000;

QString toQString(const std::string& text)
{
    return QString::fromStdString(text);
}

/// Number typed by the user: "0.1" and, for a system with a decimal comma, "0,1". Empty text is "not set".
std::optional<double> parseTypedNumber(const QString& text, bool& ok)
{
    ok = true;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return std::nullopt;

    QString dotted = trimmed;
    dotted.replace(',', '.');
    const auto number = SubstateString::parseNumber(dotted.toStdString());
    if (! number || ! std::isfinite(*number))
    {
        ok = false;
        return std::nullopt;
    }
    return number;
}

/// RAII wait cursor for the scan of the cells
struct WaitCursor
{
    WaitCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
    ~WaitCursor() { QApplication::restoreOverrideCursor(); }
};
} // namespace

AddSubstateDialog::AddSubstateDialog(SubstateValueSource source, std::vector<SubstateInfo> existing, QWidget* parent)
    : QDialog(parent)
    , m_source(std::move(source))
    , m_existing(std::move(existing))
{
    setWindowTitle(tr("Add substate"));
    setModal(true);

    for (const auto& info : m_existing)
        m_existingNames.push_back(info.name);

    auto* mainLayout = new QVBoxLayout(this);

    // --- Name and the check
    auto* nameRow = new QHBoxLayout;
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("name of the substate, e.g. psi"));
    m_nameEdit->setToolTip(tr("The name which is given to Cell::stringEncoding(name); the model must answer with a number"));
    m_checkButton = new QPushButton(tr("Check"));
    m_checkButton->setToolTip(tr("Ask the model for the value of this name"));
    nameRow->addWidget(new QLabel(tr("Name:")));
    nameRow->addWidget(m_nameEdit, 1);
    nameRow->addWidget(m_checkButton);
    mainLayout->addLayout(nameRow);

    m_statusLabel = new QLabel;
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setMinimumWidth(420);
    mainLayout->addWidget(m_statusLabel);

    // --- Parameters
    auto* form = new QFormLayout;
    m_formatEdit = new QLineEdit(QStringLiteral("%f"));
    m_formatEdit->setToolTip(tr("printf format of the value shown in the panel"));
    form->addRow(tr("Format:"), m_formatEdit);

    m_minEdit = new QLineEdit;
    m_minEdit->setPlaceholderText(tr("not set"));
    m_maxEdit = new QLineEdit;
    m_maxEdit->setPlaceholderText(tr("not set"));
    m_calculateButton = new QPushButton(tr("Calculate min and max"));
    m_calculateButton->setToolTip(tr("Take the smallest and the largest value of the substate in the cells of the current step "
                                     "(without the no-value, if it is switched on)"));
    auto* minMaxRow = new QHBoxLayout;
    minMaxRow->addWidget(new QLabel(tr("Min:")));
    minMaxRow->addWidget(m_minEdit, 1);
    minMaxRow->addWidget(new QLabel(tr("Max:")));
    minMaxRow->addWidget(m_maxEdit, 1);
    minMaxRow->addWidget(m_calculateButton);
    form->addRow(minMaxRow);

    m_noValueCheckBox = new QCheckBox(tr("No value:"));
    m_noValueCheckBox->setToolTip(tr("Cells which have this value hold no data and are not drawn"));
    m_noValueEdit = new QLineEdit;
    m_noValueEdit->setPlaceholderText(tr("e.g. -9999"));
    m_noValueEdit->setEnabled(false);
    auto* noValueRow = new QHBoxLayout;
    noValueRow->addWidget(m_noValueCheckBox);
    noValueRow->addWidget(m_noValueEdit, 1);
    form->addRow(noValueRow);
    mainLayout->addLayout(form);

    // --- Colors (optional)
    m_colorsGroup = new QGroupBox(tr("Colors (optional)"));
    m_colorsGroup->setCheckable(true);
    m_colorsGroup->setChecked(false);
    auto* colorsLayout = new QHBoxLayout(m_colorsGroup);
    m_minColorButton = new QPushButton;
    m_maxColorButton = new QPushButton;
    setColorButton(m_minColorButton, m_minColor);
    setColorButton(m_maxColorButton, m_maxColor);
    colorsLayout->addWidget(new QLabel(tr("Min:")));
    colorsLayout->addWidget(m_minColorButton);
    colorsLayout->addWidget(new QLabel(tr("Max:")));
    colorsLayout->addWidget(m_maxColorButton);
    colorsLayout->addStretch(1);
    mainLayout->addWidget(m_colorsGroup);

    m_inputErrorLabel = new QLabel;
    m_inputErrorLabel->setWordWrap(true);
    m_inputErrorLabel->setStyleSheet(QStringLiteral("color: #b00020;"));
    mainLayout->addWidget(m_inputErrorLabel);

    // --- Text for Header.txt
    auto* generated = new QGroupBox(tr("For Header.txt"));
    auto* generatedLayout = new QGridLayout(generated);

    m_entryEdit = new QLineEdit;
    m_entryEdit->setReadOnly(true);
    auto* copyEntry = new QPushButton(tr("Copy"));
    generatedLayout->addWidget(new QLabel(tr("This substate:")), 0, 0);
    generatedLayout->addWidget(m_entryEdit, 0, 1);
    generatedLayout->addWidget(copyEntry, 0, 2);

    m_lineEdit = new QPlainTextEdit;
    m_lineEdit->setReadOnly(true);
    m_lineEdit->setMaximumHeight(70);
    m_lineEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    auto* copyLine = new QPushButton(tr("Copy"));
    generatedLayout->addWidget(new QLabel(tr("Whole line:")), 1, 0, Qt::AlignTop);
    generatedLayout->addWidget(m_lineEdit, 1, 1);
    generatedLayout->addWidget(copyLine, 1, 2, Qt::AlignTop);
    mainLayout->addWidget(generated);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(m_buttons);

    // Buttons inside the form must not react to Enter: Enter in the name field means "Check"
    for (auto* button : {m_checkButton, m_calculateButton, copyEntry, copyLine, m_minColorButton, m_maxColorButton})
        button->setAutoDefault(false);

    // --- Connections
    connect(m_nameEdit, &QLineEdit::textEdited, this, &AddSubstateDialog::onNameEdited);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &AddSubstateDialog::onCheckClicked);
    connect(m_checkButton, &QPushButton::clicked, this, &AddSubstateDialog::onCheckClicked);
    connect(m_calculateButton, &QPushButton::clicked, this, &AddSubstateDialog::onCalculateMinMaxClicked);
    connect(m_noValueCheckBox, &QCheckBox::toggled, m_noValueEdit, &QWidget::setEnabled);
    connect(m_noValueCheckBox, &QCheckBox::toggled, this, &AddSubstateDialog::refresh);
    connect(m_colorsGroup, &QGroupBox::toggled, this, &AddSubstateDialog::refresh);
    connect(m_minColorButton, &QPushButton::clicked, this, [this] { onColorButtonClicked(true); });
    connect(m_maxColorButton, &QPushButton::clicked, this, [this] { onColorButtonClicked(false); });
    for (auto* edit : {m_formatEdit, m_minEdit, m_maxEdit, m_noValueEdit})
        connect(edit, &QLineEdit::textChanged, this, &AddSubstateDialog::refresh);

    connect(copyEntry, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_entryEdit->text()); });
    connect(copyLine, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(headerLine()); });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Names, so that the dialog can be driven from a test
    m_nameEdit->setObjectName("nameEdit");
    m_checkButton->setObjectName("checkButton");
    m_statusLabel->setObjectName("statusLabel");
    m_formatEdit->setObjectName("formatEdit");
    m_minEdit->setObjectName("minEdit");
    m_maxEdit->setObjectName("maxEdit");
    m_calculateButton->setObjectName("calculateButton");
    m_noValueCheckBox->setObjectName("noValueCheckBox");
    m_noValueEdit->setObjectName("noValueEdit");
    m_colorsGroup->setObjectName("colorsGroup");
    m_inputErrorLabel->setObjectName("inputErrorLabel");
    m_entryEdit->setObjectName("entryEdit");
    m_lineEdit->setObjectName("lineEdit");
    m_buttons->setObjectName("buttons");

    setStatus(tr("Enter the name and click Check: the model must return a number for it."), true);
    m_statusLabel->setStyleSheet(QString());
    refresh();
    m_nameEdit->setFocus();
}

std::string AddSubstateDialog::trimmedName() const
{
    return m_nameEdit->text().trimmed().toStdString();
}

void AddSubstateDialog::setStatus(const QString& text, bool ok)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok ? QStringLiteral("color: #1b6e1b;") : QStringLiteral("color: #b00020;"));
}

void AddSubstateDialog::setColorButton(QPushButton* button, const QColor& color)
{
    button->setText(color.name());
    button->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: %2; padding: 3px 10px; }")
                              .arg(color.name(), color.lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black")));
}

void AddSubstateDialog::onNameEdited()
{
    // Whatever was checked is not what is typed now
    m_checkedName.clear();
    setStatus(tr("Click Check: the model must return a number for this name."), true);
    m_statusLabel->setStyleSheet(QString());
    refresh();
}

bool AddSubstateDialog::checkName()
{
    const std::string name = trimmedName();
    m_checkedName.clear();

    if (const std::string problem = SubstateString::validateName(name, m_existingNames); ! problem.empty())
    {
        setStatus(toQString(problem), false);
        refresh();
        return false;
    }

    if (! m_source.textOfCell || m_source.cellCount == 0)
    {
        setStatus(tr("There is no data loaded to check the name against."), false);
        refresh();
        return false;
    }

    const std::size_t stride = std::max<std::size_t>(1, m_source.cellCount / maxCellsOfTheCheck);
    SubstateString::ValueSummary summary;
    {
        const WaitCursor waitCursor;
        summary = SubstateString::summarizeValues(
            m_source.cellCount, stride, [&](std::size_t index) { return m_source.textOfCell(name, index); });
    }

    if (const std::string problem = SubstateString::describeProblem(summary, name); ! problem.empty())
    {
        setStatus(toQString(problem), false);
        refresh();
        return false;
    }

    m_checkedName = name;
    setStatus(tr("The model returns numbers for \"%1\" (%2 cells checked, e.g. %3).")
                  .arg(toQString(name))
                  .arg(summary.cellsChecked)
                  .arg(toQString(summary.firstNumericText)),
              true);
    refresh();
    return true;
}

void AddSubstateDialog::onCheckClicked()
{
    checkName();
}

void AddSubstateDialog::onCalculateMinMaxClicked()
{
    // The name has to be good first: with a wrong one the cells give no numbers to take min and max of
    if (m_checkedName != trimmedName() && ! checkName())
        return;

    std::optional<double> noValue;
    if (m_noValueCheckBox->isChecked())
    {
        bool ok = true;
        noValue = parseTypedNumber(m_noValueEdit->text(), ok);
        if (! ok || ! noValue)
        {
            m_inputErrorLabel->setText(tr("Enter the no-value (a number) or switch it off before calculating."));
            return;
        }
    }

    const std::string name = trimmedName();
    SubstateString::ValueSummary summary;
    {
        const WaitCursor waitCursor;
        summary = SubstateString::summarizeValues(
            m_source.cellCount, 1, [&](std::size_t index) { return m_source.textOfCell(name, index); }, noValue);
    }

    if (summary.usedForRange == 0)
    {
        setStatus(tr("No cell has a value to take min and max of (all of them are the no-value or not finite)."), false);
        return;
    }

    m_minEdit->setText(toQString(SubstateString::numberToText(summary.minValue)));
    m_maxEdit->setText(toQString(SubstateString::numberToText(summary.maxValue)));
    setStatus(tr("Min and max calculated from %1 cells.").arg(summary.usedForRange), true);
}

void AddSubstateDialog::onColorButtonClicked(bool isMinColor)
{
    const QColor current = isMinColor ? m_minColor : m_maxColor;
    const QColor chosen = QColorDialog::getColor(current, this, isMinColor ? tr("Color of the min value") : tr("Color of the max value"));
    if (! chosen.isValid())
        return;

    (isMinColor ? m_minColor : m_maxColor) = chosen;
    setColorButton(isMinColor ? m_minColorButton : m_maxColorButton, chosen);
    refresh();
}

SubstateInfo AddSubstateDialog::readFields(QString& error) const
{
    SubstateInfo info;
    info.name = trimmedName();
    info.format = m_formatEdit->text().trimmed().toStdString();
    if (info.format.empty())
        info.format = "%f";
    if (info.format.find_first_of(", ()") != std::string::npos)
        error = tr("The format cannot contain commas, spaces or parentheses.");

    bool ok = true;
    const auto minValue = parseTypedNumber(m_minEdit->text(), ok);
    if (! ok && error.isEmpty())
        error = tr("Min is not a number.");
    const auto maxValue = parseTypedNumber(m_maxEdit->text(), ok);
    if (! ok && error.isEmpty())
        error = tr("Max is not a number.");
    if (minValue)
        info.minValue = *minValue;
    if (maxValue)
        info.maxValue = *maxValue;
    if (minValue && maxValue && *minValue > *maxValue && error.isEmpty())
        error = tr("Min is larger than max.");

    if (m_noValueCheckBox->isChecked())
    {
        const auto noValue = parseTypedNumber(m_noValueEdit->text(), ok);
        if ((! ok || ! noValue) && error.isEmpty())
            error = tr("No value: enter a number or switch it off.");
        if (noValue)
        {
            info.noValue = *noValue;
            info.noValueEnabled = true;
        }
    }

    if (m_colorsGroup->isChecked())
    {
        info.minColor = m_minColor.name().toStdString();
        info.maxColor = m_maxColor.name().toStdString();
    }
    return info;
}

SubstateInfo AddSubstateDialog::substate() const
{
    QString ignored;
    return readFields(ignored);
}

QString AddSubstateDialog::headerLine() const
{
    QString ignored;
    std::vector<SubstateInfo> all = m_existing;
    all.push_back(readFields(ignored));
    return QStringLiteral("substates=") + toQString(SubstateString::formatEntries(all));
}

void AddSubstateDialog::refresh()
{
    QString error;
    const SubstateInfo info = readFields(error);
    m_inputErrorLabel->setText(error);

    // The text for Header.txt is given only for a name which the model answers with numbers
    const bool nameChecked = ! m_checkedName.empty() && m_checkedName == info.name;
    if (error.isEmpty() && nameChecked)
    {
        m_entryEdit->setText(toQString(SubstateString::formatEntry(info)));
        m_lineEdit->setPlainText(headerLine());
    }
    else
    {
        m_entryEdit->clear();
        m_lineEdit->clear();
    }

    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(nameChecked && error.isEmpty());
    m_buttons->button(QDialogButtonBox::Ok)->setToolTip(
        nameChecked ? QString() : tr("Click Check first: the model has to return a number for the name"));
}
