/** @file ActionParametersDialog.h
 * @brief Dialog built from the parameters (viz::actions::ParameterInfo) of an action plugin. */

#pragma once

#include <QDialog>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "plugins/ActionApi.h"

class QLineEdit;

namespace Ui
{
class ActionParametersDialog;
}

/** @class ActionParametersDialog
 *  @brief Asks the user for the values of the parameters of an action.
 *
 *  A field is made for every parameter: a check box, a combo box (for choices), or a text field with a "Browse..." button
 *  for paths. On OK the values are given to the action (IAction::setParameter), so its error messages are shown here and
 *  the dialog stays open until all values are accepted. The last values are remembered between runs (QSettings).
 *  The frame of the dialog is in ActionParametersDialog.ui; the rows are added in code, as they depend on the action. */
class ActionParametersDialog : public QDialog
{
    Q_OBJECT

public:
    /** @param action           the action (its parameters are set when OK is pressed)
     *  @param initialDirectory where the file dialogs start and where the default output file goes
     *  @param parent           parent widget */
    ActionParametersDialog(viz::actions::IAction& action, const QString& initialDirectory, QWidget* parent = nullptr);
    ~ActionParametersDialog() override;

    /** @return values typed by the user (parameter name -> text), valid after exec() returned Accepted */
    std::map<std::string, std::string> values() const;

public slots:
    /** @brief Checks the values with the action; closes the dialog only if all are accepted. */
    void accept() override;

private:
    /** @struct Field
     *  @brief A parameter together with the widget that edits it. */
    struct Field
    {
        viz::actions::ParameterInfo info;
        QWidget* editor = nullptr; ///< QComboBox, QCheckBox or QLineEdit, see valueOf()
    };

    void showDescription();
    void addParameterRow(const viz::actions::ParameterInfo& parameter);
    QString initialValue(const viz::actions::ParameterInfo& parameter) const;

    QWidget* createChoiceEditor(Field& field, const QString& value);
    QWidget* createBooleanEditor(Field& field, const QString& value);
    QWidget* createTextEditor(Field& field, const QString& value);
    QWidget* createPathEditor(Field& field, const QString& value);
    void browseForPath(QLineEdit* edit, const viz::actions::ParameterInfo& parameter);

    void customizeButtons();
    bool applyValuesToAction();
    void rememberValues() const;

    std::string valueOf(const Field& field) const;
    QString settingsKey(const viz::actions::ParameterInfo& parameter) const;

    std::unique_ptr<Ui::ActionParametersDialog> ui; ///< The frame of the dialog is in ActionParametersDialog.ui
    viz::actions::IAction& action;
    QString initialDirectory;
    QString actionId;
    std::vector<Field> fields;
};
