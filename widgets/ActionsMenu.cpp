#include "ActionsMenu.h"

#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <map>

#include "ActionParametersDialog.h"
#include "plugins/ActionRegistry.h"
#include "plugins/ActionRunner.h"

ActionsMenu::ActionsMenu(QWidget* window, std::function<std::string()> modelName, std::function<QString()> headerPath)
    : QObject(window)
    , window_(window)
    , menu_(new QMenu(tr("&Actions"), window))
    , modelName_(std::move(modelName))
    , headerPath_(std::move(headerPath))
{
    connect(menu_, &QMenu::aboutToShow, this, &ActionsMenu::rebuild);
    rebuild();
}

void ActionsMenu::rebuild()
{
    menu_->clear();

    const std::string model = modelName_();
    const QString header = headerPath_();
    const bool simulationOpen = ! model.empty() && ! header.isEmpty() && QFileInfo::exists(header);

    // Actions which do not work with the model are not shown; while nothing is open all of them are, but disabled
    std::map<std::string, QMenu*> categories;
    auto& registry = viz::actions::ActionRegistry::instance();
    int shown = 0;
    for (const auto& id : registry.ids())
    {
        const auto* action = registry.find(id);
        const auto info = action->info();
        if (simulationOpen && ! viz::actions::ActionRegistry::supportsModel(info, model))
            continue;

        QMenu*& category = categories[info.category];
        if (! category)
            category = menu_->addMenu(QString::fromStdString(info.category));

        QAction* entry = category->addAction(QString::fromStdString(info.title));
        QString toolTip = QString::fromStdString(info.description);
        if (! info.pluginName.empty())
            toolTip += tr("\n(plugin: %1)").arg(QString::fromStdString(info.pluginName));
        entry->setToolTip(toolTip);
        entry->setStatusTip(QString::fromStdString(info.description));
        entry->setEnabled(simulationOpen);
        connect(entry, &QAction::triggered, this, [this, id] { run(id); });
        ++shown;
    }

    if (shown == 0)
    {
        QAction* none = menu_->addAction(simulationOpen ? tr("No actions for this model (load a plugin)")
                                                        : tr("No actions available (load a plugin)"));
        none->setEnabled(false);
    }
    else if (! simulationOpen)
    {
        menu_->addSeparator();
        menu_->addAction(tr("Open a simulation to use the actions"))->setEnabled(false);
    }
}

void ActionsMenu::run(const std::string& actionId)
{
    auto* action = viz::actions::ActionRegistry::instance().find(actionId);
    if (! action)
        return;

    const QString header = headerPath_();
    ActionParametersDialog dialog(*action, QFileInfo(header).absolutePath(), window_);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QProgressDialog progress(tr("Running: %1").arg(QString::fromStdString(action->info().title).remove("...")), tr("Cancel"), 0, 100, window_);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    viz::actions::ActionJob job;
    job.actionId = actionId;
    job.modelName = modelName_();
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

    QString details;
    for (const auto& warning : result.warnings)
        details += QString::fromStdString(warning) + "\n";

    if (result.cancelled)
    {
        QMessageBox::information(window_, tr("Action cancelled"), tr("The action was cancelled."));
    }
    else
    {
        QMessageBox box(result.ok ? QMessageBox::Information : QMessageBox::Critical,
                        result.ok ? tr("Action finished") : tr("Action failed"),
                        QString::fromStdString(result.message),
                        QMessageBox::Ok,
                        window_);
        if (! details.isEmpty())
            box.setDetailedText(tr("Warnings:\n") + details);
        box.exec();
    }
    emit actionFinished();
}
