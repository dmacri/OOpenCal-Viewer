#include "ActionsMenu.h"

#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>

#include "ActionParametersDialog.h"
#include "plugins/ActionRegistry.h"

ActionsMenu::ActionsMenu(QMenu* menu, QWidget* window, std::function<std::string()> modelName, std::function<QString()> headerPath)
    : QObject(window)
    , menu(menu)
    , window(window)
    , modelName(std::move(modelName))
    , headerPath(std::move(headerPath))
{
    connect(menu, &QMenu::aboutToShow, this, &ActionsMenu::rebuild);
    rebuild();
}

void ActionsMenu::rebuild()
{
    menu->clear();

    const std::string model = modelName();
    const QString header = headerPath();
    const bool simulationOpen = ! model.empty() && ! header.isEmpty() && QFileInfo::exists(header);

    // Actions which do not work with the model are not shown; while nothing is open all of them are, but disabled
    auto& registry = viz::actions::ActionRegistry::instance();
    std::map<std::string, QMenu*> categories;
    int shown = 0;
    for (const auto& id : registry.ids())
    {
        if (simulationOpen && ! viz::actions::ActionRegistry::supportsModel(registry.find(id)->info(), model))
            continue;

        addEntry(categories, id, simulationOpen);
        ++shown;
    }

    addHint(shown, simulationOpen);
}

void ActionsMenu::addEntry(std::map<std::string, QMenu*>& categories, const std::string& actionId, bool enabled)
{
    const auto info = viz::actions::ActionRegistry::instance().find(actionId)->info();

    QMenu*& category = categories[info.category];
    if (! category)
        category = menu->addMenu(QString::fromStdString(info.category));

    QAction* entry = category->addAction(QString::fromStdString(info.title));
    QString toolTip = QString::fromStdString(info.description);
    if (! info.pluginName.empty())
        toolTip += tr("\n(plugin: %1)").arg(QString::fromStdString(info.pluginName));
    entry->setToolTip(toolTip);
    entry->setStatusTip(QString::fromStdString(info.description));
    entry->setEnabled(enabled);
    connect(entry, &QAction::triggered, this, [this, actionId] { run(actionId); });
}

void ActionsMenu::addHint(int shownActions, bool simulationOpen)
{
    if (shownActions == 0)
    {
        const QString text = simulationOpen ? tr("No actions for this model (load a plugin)") : tr("No actions available (load a plugin)");
        menu->addAction(text)->setEnabled(false);
    }
    else if (! simulationOpen)
    {
        menu->addSeparator();
        menu->addAction(tr("Open a simulation to use the actions"))->setEnabled(false);
    }
}

void ActionsMenu::run(const std::string& actionId)
{
    auto* action = viz::actions::ActionRegistry::instance().find(actionId);
    if (! action)
        return;

    const QString header = headerPath();
    ActionParametersDialog dialog(*action, QFileInfo(header).absolutePath(), window);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString title = QString::fromStdString(action->info().title).remove("...");
    QProgressDialog progress(tr("Running: %1").arg(title), tr("Cancel"), 0, 100, window);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    viz::actions::ActionJob job;
    job.actionId = actionId;
    job.modelName = modelName();
    job.headerPath = header.toStdString();
    job.parameters = dialog.values();
    job.onProgress = [&progress](std::size_t done, std::size_t total)
    {
        progress.setValue(static_cast<int>(100 * done / std::max<std::size_t>(total, 1)));
        QApplication::processEvents();
    };
    job.isCancelled = [&progress]
    {
        QApplication::processEvents();
        return progress.wasCanceled();
    };

    QApplication::setOverrideCursor(Qt::BusyCursor);
    const auto result = viz::actions::runAction(job);
    QApplication::restoreOverrideCursor();
    progress.close();

    showResult(result);
    emit actionFinished();
}

void ActionsMenu::showResult(const viz::actions::RunResult& result)
{
    if (result.cancelled)
    {
        QMessageBox::information(window, tr("Action cancelled"), tr("The action was cancelled."));
        return;
    }

    QMessageBox box(result.ok ? QMessageBox::Information : QMessageBox::Critical,
                    result.ok ? tr("Action finished") : tr("Action failed"),
                    QString::fromStdString(result.message),
                    QMessageBox::Ok,
                    window);

    QString warnings;
    for (const auto& warning : result.warnings)
        warnings += QString::fromStdString(warning) + "\n";
    if (! warnings.isEmpty())
        box.setDetailedText(tr("Warnings:\n") + warnings);

    box.exec();
}
