#include "LocaleSettingsDialog.h"

#include <iostream>
#include <set>
#include <string>
#include <utility>

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QProcess>
#include <QProcessEnvironment>

#include "core/ApplicationLocale.h"

namespace
{
namespace appLocale = viz::appLocale;

constexpr const char* settingsKey = "locale/numeric";
constexpr const char* systemToken = "system"; // how the "system" choice (an empty locale name) is stored

constexpr int nameRole = Qt::UserRole;         // locale name for the C library (QString, "" = system)
constexpr int decimalPointRole = Qt::UserRole + 1; // decimal separator of the locale (int)

constexpr int nameColumn = 0;
constexpr int descriptionColumn = 1;
constexpr int decimalPointColumn = 2;

constexpr const char* goodColor = "#2e7d32";
constexpr const char* badColor = "#c62828";

QString toStoredValue(const QString& name)
{
    return name.isEmpty() ? QString(systemToken) : name;
}

QString fromStoredValue(const QString& stored)
{
    return stored == systemToken ? QString() : stored;
}

/// Names of the locales which the C library knows on this computer (`locale -a`); empty when the list is not available
QStringList installedLocaleNames()
{
#if defined(Q_OS_UNIX)
    QProcess process;
    process.start("locale", {"-a"});
    if (process.waitForFinished(3000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0)
        return QString::fromLocal8Bit(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
#endif
    return {};
}

/// A few common locales to offer when the list of the installed ones is not available (they are checked before use)
QStringList commonLocaleNames()
{
    return {"en_US.UTF-8", "en_GB.UTF-8", "pl_PL.UTF-8", "it_IT.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8", "es_ES.UTF-8"};
}

/// "Italian (Italy)" for it_IT.UTF-8; empty when Qt does not know the locale
QString describeLocale(const QString& name)
{
    if (name.startsWith("C.")) // C.utf8: the C format with UTF-8 characters
        return QObject::tr("Same number format as C");

    const QLocale qtLocale(name);
    if (qtLocale.language() == QLocale::C)
        return {};

    QString text = QLocale::languageToString(qtLocale.language());
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    if (qtLocale.territory() != QLocale::AnyTerritory)
        text += " (" + QLocale::territoryToString(qtLocale.territory()) + ")";
#else
    if (qtLocale.country() != QLocale::AnyCountry)
        text += " (" + QLocale::countryToString(qtLocale.country()) + ")";
#endif
    const QString native = qtLocale.nativeLanguageName();
    if (! native.isEmpty() && native.compare(QLocale::languageToString(qtLocale.language()), Qt::CaseInsensitive) != 0)
        text += " — " + native;
    return text;
}

/// What the environment says about the locale of the system, e.g. "LANG=pl_PL.UTF-8"
QString describeSystemLocale()
{
    const auto environment = QProcessEnvironment::systemEnvironment();
    for (const char* variable : {"LC_ALL", "LC_NUMERIC", "LANG"})
    {
        const QString value = environment.value(variable);
        if (! value.isEmpty())
            return QString("%1=%2").arg(variable, value);
    }
    return {};
}

QString numberText(double value)
{
    return QString::number(value, 'g', 10); // QString::number() always uses the C format
}

} // namespace

QString LocaleSettingsDialog::savedLocaleName()
{
    const QSettings settings;
    return fromStoredValue(settings.value(settingsKey, appLocale::defaultLocaleName).toString());
}

void LocaleSettingsDialog::applySavedLocale()
{
    const std::string name = savedLocaleName().toStdString();
    std::string error;
    if (appLocale::apply(name, &error))
        return;

    std::cerr << "Warning: " << error << " Numbers are read and written in the C locale instead "
              << "(see Settings -> Number Locale)." << std::endl;
    appLocale::apply(appLocale::defaultLocaleName);
}

LocaleSettingsDialog::LocaleSettingsDialog(QWidget* parent)
    : QDialog(parent)
    , initialName(savedLocaleName())
{
    setWindowTitle(tr("Number Locale"));
    setMinimumSize(680, 680);
    resize(720, 740);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    auto* intro = new QLabel(tr(
        "<b>Which locale were the numbers of the simulation written in?</b><br>"
        "The models run inside the Viewer, so this choice decides how they read and write numbers (<code>printf</code>, "
        "<code>strtod</code>, <code>std::stod</code>…) and how the substate values in <code>Header.txt</code> are read. "
        "Results which were written with a dot (<code>1008.5</code>) need the <b>C</b> locale; results written "
        "with a comma (<code>1008,5</code>), e.g. on an Italian or Polish computer, need a locale with a decimal comma."), this);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    mainLayout->addWidget(intro);

    filterEdit = new QLineEdit(this);
    filterEdit->setPlaceholderText(tr("Filter the list, e.g. pl, it_IT, German…"));
    filterEdit->setClearButtonEnabled(true);
    mainLayout->addWidget(filterEdit);

    list = new QTreeWidget(this);
    list->setColumnCount(3);
    list->setHeaderLabels({tr("Locale"), tr("Language"), tr("Decimal separator")});
    list->setRootIsDecorated(false);
    list->setAlternatingRowColors(true);
    list->setUniformRowHeights(true);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->header()->setStretchLastSection(false);
    list->header()->setSectionResizeMode(nameColumn, QHeaderView::ResizeToContents);
    list->header()->setSectionResizeMode(descriptionColumn, QHeaderView::Stretch);
    list->header()->setSectionResizeMode(decimalPointColumn, QHeaderView::ResizeToContents);
    list->setMinimumHeight(240);
    mainLayout->addWidget(list, /*stretch=*/1);

    auto* previewBox = new QGroupBox(tr("What the model code does in the selected locale"), this);
    auto* previewLayout = new QFormLayout(previewBox);
    const QFont monospace = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    for (QLabel** label : {&writesLabel, &readsDotLabel, &readsCommaLabel})
    {
        *label = new QLabel(previewBox);
        (*label)->setFont(monospace);
        (*label)->setTextFormat(Qt::RichText);
    }
    previewLayout->addRow(tr("Writes the number 1008.5 as"), writesLabel);
    previewLayout->addRow(tr("Reads the text \"1008.5\" as"), readsDotLabel);
    previewLayout->addRow(tr("Reads the text \"1008,5\" as"), readsCommaLabel);
    mainLayout->addWidget(previewBox);

    auto* hint = new QLabel(tr(
        "Only locales which are installed on this computer are listed. To add one on Ubuntu / Debian:<br>"
        "<code>sudo locale-gen it_IT.UTF-8 &amp;&amp; sudo update-locale</code><br>"
        "The choice is stored and used on every start. It is used when data is read, so reload the model after changing it."), this);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    hint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont hintFont = hint->font();
    hintFont.setPointSizeF(hintFont.pointSizeF() * 0.9);
    hint->setFont(hintFont);
    mainLayout->addWidget(hint);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText(tr("Use C (default)"));
    mainLayout->addWidget(buttons);

    connect(filterEdit, &QLineEdit::textChanged, this, &LocaleSettingsDialog::onFilterChanged);
    connect(list, &QTreeWidget::currentItemChanged, this, &LocaleSettingsDialog::onCurrentItemChanged);
    connect(list, &QTreeWidget::itemDoubleClicked, this, [this] { onAccepted(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &LocaleSettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &LocaleSettingsDialog::onRestoreDefaults);

    populateList();
    selectLocale(initialName);
    filterEdit->setFocus();
}

QTreeWidgetItem* LocaleSettingsDialog::addItem(const QString& name, const QString& description, char decimalPoint)
{
    auto* item = new QTreeWidgetItem(list);
    item->setText(nameColumn, name);
    item->setText(descriptionColumn, description);
    item->setText(decimalPointColumn, decimalPoint == ',' ? tr("comma  ,") : tr("dot  ."));
    item->setTextAlignment(decimalPointColumn, Qt::AlignCenter);
    item->setData(decimalPointColumn, decimalPointRole, static_cast<int>(decimalPoint));
    return item;
}

void LocaleSettingsDialog::populateList()
{
    QFont decimalFont = list->font();
    decimalFont.setBold(true);

    const auto finish = [&](QTreeWidgetItem* item, const QString& name)
    {
        item->setData(nameColumn, nameRole, name);
        item->setFont(decimalPointColumn, decimalFont);
    };

    // 1. The default: always available
    {
        auto* item = addItem(tr("C (default)"), tr("Standard format: numbers are written with a dot"), '.');
        finish(item, appLocale::defaultLocaleName);
    }

    // 2. The locale of the computer
    if (const auto decimalPoint = appLocale::decimalPointOf(appLocale::systemLocaleName))
    {
        const QString environment = describeSystemLocale();
        auto* item = addItem(tr("System locale"), environment.isEmpty() ? tr("The locale of this computer") : environment, *decimalPoint);
        finish(item, appLocale::systemLocaleName);
    }

    // 3. Everything which is installed
    QStringList names = installedLocaleNames();
    if (names.isEmpty())
        names = commonLocaleNames();
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);

    for (const QString& name : std::as_const(names))
    {
        if (name == "C" || name == "POSIX")
            continue;

        const auto decimalPoint = appLocale::decimalPointOf(name.toStdString());
        if (! decimalPoint)
            continue; // listed, but cannot be loaded (e.g. a broken locale archive)

        finish(addItem(name, describeLocale(name), *decimalPoint), name);
    }

    list->resizeColumnToContents(nameColumn);
}

void LocaleSettingsDialog::selectLocale(const QString& name)
{
    QTreeWidgetItem* fallback = list->topLevelItemCount() > 0 ? list->topLevelItem(0) : nullptr;
    for (int i = 0; i < list->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = list->topLevelItem(i);
        if (item->data(nameColumn, nameRole).toString() == name)
        {
            list->setCurrentItem(item);
            list->scrollToItem(item);
            return;
        }
    }
    if (fallback) // the saved locale is not installed (any more): the default is offered
        list->setCurrentItem(fallback);
}

QString LocaleSettingsDialog::selectedLocaleName() const
{
    const QTreeWidgetItem* item = list->currentItem();
    return item ? item->data(nameColumn, nameRole).toString() : QString(appLocale::defaultLocaleName);
}

void LocaleSettingsDialog::onFilterChanged(const QString& text)
{
    QTreeWidgetItem* firstVisible = nullptr;
    for (int i = 0; i < list->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = list->topLevelItem(i);
        const bool matches = text.isEmpty()
                             || item->text(nameColumn).contains(text, Qt::CaseInsensitive)
                             || item->text(descriptionColumn).contains(text, Qt::CaseInsensitive);
        item->setHidden(! matches);
        if (matches && ! firstVisible)
            firstVisible = item;
    }

    // Keep the selection on something which can be seen
    if (firstVisible && (! list->currentItem() || list->currentItem()->isHidden()))
        list->setCurrentItem(firstVisible);
}

void LocaleSettingsDialog::onCurrentItemChanged()
{
    updatePreview();
}

void LocaleSettingsDialog::onRestoreDefaults()
{
    filterEdit->clear();
    selectLocale(appLocale::defaultLocaleName);
}

void LocaleSettingsDialog::updatePreview()
{
    const QTreeWidgetItem* item = list->currentItem();
    if (! item)
        return;

    const char decimalPoint = static_cast<char>(item->data(decimalPointColumn, decimalPointRole).toInt());
    constexpr double expected = 1008.5;

    const auto coloredNumber = [](double value, bool correct)
    {
        return QString("<span style=\"color:%1;\"><b>%2</b> %3</span>")
            .arg(correct ? goodColor : badColor, numberText(value), correct ? "✔" : "✘ digits are lost");
    };

    const double readFromDot = appLocale::readLikeStrtod("1008.5", decimalPoint);
    const double readFromComma = appLocale::readLikeStrtod("1008,5", decimalPoint);

    writesLabel->setText(QString("<b>%1</b>").arg(QString::fromStdString(appLocale::writeLikePrintf(expected, 1, decimalPoint))));
    readsDotLabel->setText(coloredNumber(readFromDot, readFromDot == expected));
    readsCommaLabel->setText(coloredNumber(readFromComma, readFromComma == expected));
}

void LocaleSettingsDialog::onAccepted()
{
    const QString chosen = selectedLocaleName();

    std::string error;
    if (! appLocale::apply(chosen.toStdString(), &error))
    {
        QMessageBox::warning(this, tr("Number Locale"), QString::fromStdString(error));
        return;
    }

    QSettings settings;
    settings.setValue(settingsKey, toStoredValue(chosen));

    changed = (chosen != initialName);
    accept();
}
