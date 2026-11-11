#include "ActionParametersDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace
{
using Type = viz::actions::ParameterInfo::Type;

bool isPath(Type type)
{
    return type == Type::SaveFilePath || type == Type::OpenFilePath || type == Type::Directory;
}
} // namespace

ActionParametersDialog::ActionParametersDialog(viz::actions::IAction& action, const QString& initialDirectory, QWidget* parent)
    : QDialog(parent)
    , action_(action)
{
    const auto info = action.info();
    actionId_ = QString::fromStdString(info.id);
    setWindowTitle(QString::fromStdString(info.title).remove("..."));
    setMinimumWidth(520);

    auto* layout = new QVBoxLayout(this);
    if (! info.description.empty())
    {
        auto* description = new QLabel(QString::fromStdString(info.description), this);
        description->setWordWrap(true);
        layout->addWidget(description);
    }

    auto* form = new QFormLayout;
    QSettings settings;
    for (const auto& parameter : action.parameters())
    {
        Field field;
        field.info = parameter;

        QString value = QString::fromStdString(parameter.defaultValue);
        const QString remembered = settings.value(settingsKey(parameter)).toString();
        if (! remembered.isNull())
            value = remembered;
        if (value.isEmpty() && parameter.type == Type::SaveFilePath)
            value = QDir(initialDirectory).filePath(actionId_ + ".txt");

        QWidget* editor = nullptr;
        if (! parameter.choices.empty())
        {
            auto* combo = new QComboBox(this);
            for (const auto& choice : parameter.choices)
                combo->addItem(QString::fromStdString(choice));
            combo->setCurrentText(value);
            editor = combo;
            field.widget = combo;
        }
        else if (parameter.type == Type::Boolean)
        {
            auto* check = new QCheckBox(this);
            bool on = false;
            viz::actions::parseBoolean(value.toStdString(), on);
            check->setChecked(on);
            editor = check;
            field.widget = check;
        }
        else
        {
            auto* edit = new QLineEdit(value, this);
            field.widget = edit;
            editor = edit;

            if (isPath(parameter.type))
            {
                auto* row = new QWidget(this);
                auto* rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(0, 0, 0, 0);
                auto* browse = new QPushButton(tr("Browse..."), row);
                rowLayout->addWidget(edit, 1);
                rowLayout->addWidget(browse);
                editor = row;

                connect(browse, &QPushButton::clicked, this,
                        [this, edit, type = parameter.type, filter = QString::fromStdString(parameter.fileFilter), initialDirectory]
                        {
                            const QString start = edit->text().isEmpty() ? initialDirectory : edit->text();
                            QString chosen;
                            if (type == Type::SaveFilePath)
                                chosen = QFileDialog::getSaveFileName(this, tr("Save as"), start, filter);
                            else if (type == Type::OpenFilePath)
                                chosen = QFileDialog::getOpenFileName(this, tr("Open"), start, filter);
                            else
                                chosen = QFileDialog::getExistingDirectory(this, tr("Select directory"), start);
                            if (! chosen.isEmpty())
                                edit->setText(chosen);
                        });
            }
        }

        const QString label = QString::fromStdString(parameter.label.empty() ? parameter.name : parameter.label);
        auto* labelWidget = new QLabel(parameter.required ? label + " *" : label, this);
        const QString toolTip = QString::fromStdString(parameter.description);
        labelWidget->setToolTip(toolTip);
        editor->setToolTip(toolTip);
        form->addRow(labelWidget, editor);

        fields_.push_back(field);
    }
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Run"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ActionParametersDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

std::string ActionParametersDialog::valueOf(const Field& field) const
{
    if (const auto* combo = qobject_cast<QComboBox*>(field.widget))
        return combo->currentText().toStdString();
    if (const auto* check = qobject_cast<QCheckBox*>(field.widget))
        return check->isChecked() ? "true" : "false";
    return qobject_cast<QLineEdit*>(field.widget)->text().toStdString();
}

std::map<std::string, std::string> ActionParametersDialog::values() const
{
    std::map<std::string, std::string> result;
    for (const auto& field : fields_)
        result[field.info.name] = valueOf(field);
    return result;
}

QString ActionParametersDialog::settingsKey(const viz::actions::ParameterInfo& parameter) const
{
    return "actions/" + actionId_ + "/" + QString::fromStdString(parameter.name);
}

void ActionParametersDialog::rememberValues() const
{
    QSettings settings;
    for (const auto& field : fields_)
        settings.setValue(settingsKey(field.info), QString::fromStdString(valueOf(field)));
}

void ActionParametersDialog::accept()
{
    // The action validates the values; its messages are shown here so the user can correct them
    for (const auto& field : fields_)
    {
        const std::string value = valueOf(field);
        if (field.info.required && viz::actions::trimmed(value).empty())
        {
            QMessageBox::warning(this, windowTitle(), tr("'%1' is required.").arg(QString::fromStdString(field.info.label)));
            return;
        }
        try
        {
            const auto result = action_.setParameter(field.info.name, value);
            if (! result)
            {
                QMessageBox::warning(this, windowTitle(),
                                     tr("%1: %2").arg(QString::fromStdString(field.info.label), QString::fromStdString(result.message)));
                return;
            }
        }
        catch (const std::exception& e)
        {
            QMessageBox::warning(this, windowTitle(), QString::fromUtf8(e.what()));
            return;
        }
    }
    rememberValues();
    QDialog::accept();
}
