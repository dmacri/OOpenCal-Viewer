/** @file ActionsMenu.h
 * @brief The `Actions` menu of the main window: lists the actions of loaded plugins (see plugins/ActionApi.h). */

#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <string>

class QMenu;
class QWidget;

/** @brief Menu with the actions of plugins, usable with the simulation which is currently open.
 *
 * The menu is rebuilt every time it is about to be shown, so it always reflects the loaded plugins and the current
 * model: actions which do not support the model are left out, all are disabled while no simulation is open.
 * Choosing an action asks for its parameters (ActionParametersDialog), runs it (with a progress dialog that can cancel
 * it) and shows the result. */
class ActionsMenu : public QObject
{
    Q_OBJECT

public:
    /// @param window         parent of the dialogs
    /// @param modelName      returns the name of the current model ("" if none)
    /// @param headerPath     returns the path to Header.txt of the open simulation ("" if none)
    ActionsMenu(QWidget* window, std::function<std::string()> modelName, std::function<QString()> headerPath);

    QMenu* menu() const
    {
        return menu_;
    }

signals:
    /// Emitted after an action was run (successfully or not), e.g. to refresh the view.
    void actionFinished();

private:
    void rebuild();
    void run(const std::string& actionId);

    QWidget* window_;
    QMenu* menu_;
    std::function<std::string()> modelName_;
    std::function<QString()> headerPath_;
};
