#include "ActionParametersDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>

#include "ui_ActionParametersDialog.h"

namespace
{
using Type = viz::actions::ParameterInfo::Type;

bool isPath(Type type)
{
    return type == Type::SaveFilePath || type == Type::OpenFilePath || type == Type::Directory;
}

QString toQString(const std::string& text)
{
    return QString::fromStdString(text);
}
} // namespace

ActionParametersDialog::ActionParametersDialog(viz::actions::IAction& action, const QString& initialDirectory, QWidget* parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::ActionParametersDialog>())
    , action(action)
    , initialDirectory(initialDirectory)
    , actionId(toQString(action.info().id))
{
    ui->setupUi(this);
    setWindowTitle(toQString(action.info().title).remove("..."));

    showDescription();
    for (const auto& parameter : action.parameters())
        addParameterRow(parameter);
    customizeButtons();
}

ActionParametersDialog::~ActionParametersDialog() = default;

void ActionParametersDialog::showDescription()
{
    const QString description = toQString(action.info().description);
    ui->descriptionLabel->setText(description);
    ui->descriptionLabel->setVisible(! description.isEmpty());
}

void ActionParametersDialog::addParameterRow(const viz::actions::ParameterInfo& parameter)
{
    Field field{parameter};
    const QString value = initialValue(parameter);

    QWidget* editor = nullptr;
    if (! parameter.choices.empty())
        editor = createChoiceEditor(field, value);
    else if (parameter.type == Type::Boolean)
        editor = createBooleanEditor(field, value);
    else if (isPath(parameter.type))
        editor = createPathEditor(field, value);
    else
        editor = createTextEditor(field, value);

    const QString label = toQString(parameter.label.empty() ? parameter.name : parameter.label);
    auto* labelWidget = new QLabel(parameter.required ? label + " *" : label, this);
    const QString toolTip = toQString(parameter.description);
    labelWidget->setToolTip(toolTip);
    editor->setToolTip(toolTip);

    ui->parametersLayout->addRow(labelWidget, editor);
    fields.push_back(field);
}

QString ActionParametersDialog::initialValue(const viz::actions::ParameterInfo& parameter) const
{
    const QString remembered = QSettings().value(settingsKey(parameter)).toString();
    if (! remembered.isNull())
        return remembered;

    if (parameter.defaultValue.empty() && parameter.type == Type::SaveFilePath)
        return QDir(initialDirectory).filePath(actionId + ".txt");

    return toQString(parameter.defaultValue);
}

QWidget* ActionParametersDialog::createChoiceEditor(Field& field, const QString& value)
{
    auto* combo = new QComboBox(this);
    for (const auto& choice : field.info.choices)
        combo->addItem(toQString(choice));
    combo->setCurrentText(value);
    field.editor = combo;
    return combo;
}

QWidget* ActionParametersDialog::createBooleanEditor(Field& field, const QString& value)
{
    auto* check = new QCheckBox(this);
    bool on = false;
    viz::actions::parseBoolean(value.toStdString(), on);
    check->setChecked(on);
    field.editor = check;
    return check;
}

QWidget* ActionParametersDialog::createTextEditor(Field& field, const QString& value)
{
    auto* edit = new QLineEdit(value, this);
    field.editor = edit;
    return edit;
}

QWidget* ActionParametersDialog::createPathEditor(Field& field, const QString& value)
{
    auto* row = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    auto* edit = new QLineEdit(value, row);
    auto* browse = new QPushButton(tr("Browse..."), row);
    rowLayout->addWidget(edit, 1);
    rowLayout->addWidget(browse);

    connect(browse, &QPushButton::clicked, this, [this, edit, parameter = field.info] { browseForPath(edit, parameter); });

    field.editor = edit;
    return row;
}

void ActionParametersDialog::browseForPath(QLineEdit* edit, const viz::actions::ParameterInfo& parameter)
{
    const QString start = edit->text().isEmpty() ? initialDirectory : edit->text();
    const QString filter = toQString(parameter.fileFilter);

    QString chosen;
    if (parameter.type == Type::SaveFilePath)
        chosen = QFileDialog::getSaveFileName(this, tr("Save as"), start, filter);
    else if (parameter.type == Type::OpenFilePath)
        chosen = QFileDialog::getOpenFileName(this, tr("Open"), start, filter);
    else
        chosen = QFileDialog::getExistingDirectory(this, tr("Select directory"), start);

    if (! chosen.isEmpty())
        edit->setText(chosen);
}

void ActionParametersDialog::customizeButtons()
{
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Run"));
}

std::string ActionParametersDialog::valueOf(const Field& field) const
{
    if (const auto* combo = qobject_cast<QComboBox*>(field.editor))
        return combo->currentText().toStdString();
    if (const auto* check = qobject_cast<QCheckBox*>(field.editor))
        return check->isChecked() ? "true" : "false";
    return qobject_cast<QLineEdit*>(field.editor)->text().toStdString();
}

std::map<std::string, std::string> ActionParametersDialog::values() const
{
    std::map<std::string, std::string> result;
    for (const auto& field : fields)
        result[field.info.name] = valueOf(field);
    return result;
}

QString ActionParametersDialog::settingsKey(const viz::actions::ParameterInfo& parameter) const
{
    return "actions/" + actionId + "/" + toQString(parameter.name);
}

void ActionParametersDialog::rememberValues() const
{
    QSettings settings;
    for (const auto& field : fields)
        settings.setValue(settingsKey(field.info), toQString(valueOf(field)));
}

bool ActionParametersDialog::applyValuesToAction()
{
    for (const auto& field : fields)
    {
        const std::string value = valueOf(field);
        const QString label = toQString(field.info.label);

        if (field.info.required && viz::actions::trimmed(value).empty())
        {
            QMessageBox::warning(this, windowTitle(), tr("'%1' is required.").arg(label));
            return false;
        }

        try
        {
            if (const auto status = action.setParameter(field.info.name, value); ! status)
            {
                QMessageBox::warning(this, windowTitle(), tr("%1: %2").arg(label, toQString(status.error())));
                return false;
            }
        }
        catch (const std::exception& e)
        {
            QMessageBox::warning(this, windowTitle(), QString::fromUtf8(e.what()));
            return false;
        }
    }
    return true;
}

void ActionParametersDialog::accept()
{
    // The action validates the values; its messages are shown here so the user can correct them
    if (! applyValuesToAction())
        return;

    rememberValues();
    QDialog::accept();
}
