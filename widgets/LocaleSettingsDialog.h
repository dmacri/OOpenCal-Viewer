/** @file LocaleSettingsDialog.h
 * @brief Dialog (Settings -> Number Locale...) for choosing the number locale of the whole application.
 *
 * The locale decides how the model classes (which run inside the Viewer as plugins) and the Viewer itself read and write
 * numbers with the C library: "1008.5" or "1008,5". Results of a simulation which was run on another computer (e.g. a
 * supercomputer) may have been written in another locale than the one of the computer on which they are opened.
 *
 * The list contains the locales which are installed on this computer. For every locale the dialog shows its decimal
 * separator and what the model code is going to do with a number written with a dot and with a comma.
 * The choice is stored in QSettings ("locale/numeric"), and applied on every start of the application. */
#pragma once

#include <memory>

#include <QDialog>
#include <QString>

class QTreeWidgetItem;

namespace Ui
{
class LocaleSettingsDialog;
}

/** @class LocaleSettingsDialog
 * @brief Lets the user choose the LC_NUMERIC locale of the process; OK applies and stores it. */
class LocaleSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LocaleSettingsDialog(QWidget* parent = nullptr);
    ~LocaleSettingsDialog() override;

    /// @brief The locale which is stored in the settings: "C" (default), "" (the locale of the system) or e.g. "pl_PL.UTF-8"
    static QString savedLocaleName();

    /** @brief Applies the stored locale to the process; to be called once on start, after the application name is set.
     *  When the locale is not installed (any more), the default "C" locale stays and a warning is printed. */
    static void applySavedLocale();

    /// @brief True when the dialog was accepted and the locale differs from the one which was active when it was opened
    bool localeChanged() const { return changed; }

private slots:
    void onFilterChanged(const QString& text);
    void onCurrentItemChanged();
    void onRestoreDefaults();
    void onAccepted();

private:
    void populateList();
    QTreeWidgetItem* addItem(const QString& name, const QString& description, char decimalPoint);
    void selectLocale(const QString& name);
    void updatePreview();
    QString selectedLocaleName() const;

    std::unique_ptr<Ui::LocaleSettingsDialog> ui; ///< The layout is in LocaleSettingsDialog.ui
    QString initialName;
    bool changed = false;
};
