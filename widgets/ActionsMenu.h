/** @file ActionsMenu.h
 * @brief The `Actions` menu of the main window: lists the actions of loaded plugins (see plugins/ActionApi.h). */

#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <map>
#include <string>

#include "plugins/ActionRunner.h"

class QMenu;
class QWidget;

/** @class ActionsMenu
 *  @brief Fills the `Actions` menu with the actions of plugins, usable with the simulation which is currently open.
 *
 *  The menu itself is declared in mainwindow.ui (`menuActions`); its entries depend on the loaded plugins and on the
 *  model, so they are made here. The menu is rebuilt every time it is about to be shown, therefore it always reflects
 *  the loaded plugins and the current model: actions which do not support the model are left out, and all are
 *  disabled while no simulation is open.
 *
 *  Choosing an action asks for its parameters (ActionParametersDialog), runs it (with a progress dialog that can cancel
 *  it) and shows the result. */
class ActionsMenu : public QObject
{
    Q_OBJECT

public:
    /** @param menu        the menu to fill (it is not owned)
     *  @param window      parent of the dialogs
     *  @param modelName   returns the name of the current model ("" if none)
     *  @param headerPath  returns the path to Header.txt of the open simulation ("" if none) */
    ActionsMenu(QMenu* menu, QWidget* window, std::function<std::string()> modelName, std::function<QString()> headerPath);

signals:
    /** @brief Emitted after an action was run (successfully or not), e.g. to refresh the view. */
    void actionFinished();

private:
    void rebuild();
    void addEntry(std::map<std::string, QMenu*>& categories, const std::string& actionId, bool enabled);
    void addHint(int shownActions, bool simulationOpen);
    void run(const std::string& actionId);
    void showResult(const viz::actions::RunResult& result);

    QMenu* menu;
    QWidget* window;
    std::function<std::string()> modelName;
    std::function<QString()> headerPath;
};
