/** @file ActionParametersDialog.h
 * @brief Dialog built from the parameters (viz::actions::ParameterInfo) of an action plugin. */

#pragma once

#include <QDialog>
#include <map>
#include <string>
#include <vector>

#include "plugins/ActionApi.h"

class QWidget;

/** @brief Asks the user for the values of the parameters of an action.
 *
 * A field is made for every parameter: a check box, a combo box (for choices), or a text field with a "Browse..." button
 * for paths. On OK the values are given to the action (IAction::setParameter), so its error messages are shown here and
 * the dialog stays open until all values are accepted. The last values are remembered between runs. */
class ActionParametersDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param action        the action (parameters are set on it when OK is pressed)
    /// @param initialDirectory where the file dialogs start and where the default output file goes
    ActionParametersDialog(viz::actions::IAction& action, const QString& initialDirectory, QWidget* parent = nullptr);

    /// Values typed by the user (name -> text), valid after exec() returned Accepted.
    std::map<std::string, std::string> values() const;

public slots:
    void accept() override;

private:
    struct Field
    {
        viz::actions::ParameterInfo info;
        QWidget* widget = nullptr;
    };

    std::string valueOf(const Field& field) const;
    QString settingsKey(const viz::actions::ParameterInfo& parameter) const;
    void rememberValues() const;

    viz::actions::IAction& action_;
    std::vector<Field> fields_;
    QString actionId_;
};
