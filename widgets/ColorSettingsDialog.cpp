#include "ColorSettingsDialog.h"

#include <QColorDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>

#include "ui_ColorSettingsDialog.h"
#include "widgets/ColorSettings.h"


ColorSettingsDialog::ColorSettingsDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::ColorSettingsDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Color Settings"));

    // Connect signals and slots
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ColorSettingsDialog::onAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ColorSettingsDialog::onRejected);
    connect(ui->btnResetColors, &QPushButton::clicked, this, &ColorSettingsDialog::onResetColors);

    // Connect color buttons
    connect(ui->btnBackgroundColor, &QPushButton::clicked, this, &ColorSettingsDialog::onBackgroundColorClicked);
    connect(ui->btnTextColor, &QPushButton::clicked, this, &ColorSettingsDialog::onTextColorClicked);
    connect(ui->btnGridColor, &QPushButton::clicked, this, &ColorSettingsDialog::onGridColorClicked);
    connect(ui->btnHighlightColor, &QPushButton::clicked, this, &ColorSettingsDialog::onHighlightColorClicked);
    connect(ui->btnFlatSceneBackgroundColor, &QPushButton::clicked, this, &ColorSettingsDialog::onFlatSceneBackgroundColorClicked);

    // Load current settings
    loadSettings();
    updateColorPreviews();
}

ColorSettingsDialog::~ColorSettingsDialog()
{
    delete ui;
}

void ColorSettingsDialog::onBackgroundColorClicked()
{
    if (selectColor(m_backgroundColor, tr("Select Background Color")))
    {
        updateColorPreviews();
    }
}

void ColorSettingsDialog::onTextColorClicked()
{
    if (selectColor(m_textColor, tr("Select Text Color")))
    {
        updateColorPreviews();
    }
}

void ColorSettingsDialog::onGridColorClicked()
{
    if (selectColor(m_gridColor, tr("Select Grid Color")))
    {
        updateColorPreviews();
    }
}

void ColorSettingsDialog::onHighlightColorClicked()
{
    if (selectColor(m_highlightColor, tr("Select Highlight Color")))
    {
        updateColorPreviews();
    }
}

void ColorSettingsDialog::onFlatSceneBackgroundColorClicked()
{
    if (selectColor(m_flatSceneBackgroundColor, tr("Select Flat Scene Background Color")))
    {
        updateColorPreviews();
    }
}

void ColorSettingsDialog::onResetColors()
{
    auto reply = QMessageBox::question(this, 
                                     tr("Reset Colors"),
                                     tr("Are you sure you want to reset all colors to their default values?"),
                                     QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes)
    {
        m_backgroundColor = ColorSettings::DEFAULT_BACKGROUND;
        m_textColor = ColorSettings::DEFAULT_TEXT;
        m_gridColor = ColorSettings::DEFAULT_GRID;
        m_highlightColor = ColorSettings::DEFAULT_HIGHLIGHT;
        m_flatSceneBackgroundColor = ColorSettings::DEFAULT_FLAT_SCENE_BACKGROUND;
        updateColorPreviews();
    }
}

void ColorSettingsDialog::onAccepted()
{
    // Save settings
    ColorSettings& settings = ColorSettings::instance();
    settings.setBackgroundColor(m_backgroundColor);
    settings.setTextColor(m_textColor);
    settings.setGridColor(m_gridColor);
    settings.setHighlightColor(m_highlightColor);
    settings.setFlatSceneBackgroundColor(m_flatSceneBackgroundColor);
    settings.saveSettings();

    accept();
}

void ColorSettingsDialog::onRejected()
{
    // Reload settings to discard changes
    loadSettings();
    reject();
}

void ColorSettingsDialog::updateColorPreviews()
{
    updateColorButton(ui->btnBackgroundColor, m_backgroundColor);
    updateColorButton(ui->btnTextColor, m_textColor);
    updateColorButton(ui->btnGridColor, m_gridColor);
    updateColorButton(ui->btnHighlightColor, m_highlightColor);
    updateColorButton(ui->btnFlatSceneBackgroundColor, m_flatSceneBackgroundColor);

    // Update preview text
    QString style = QString("color: %1; background-color: %2;")
                    .arg(m_textColor.name(), m_backgroundColor.name());
    ui->lblPreview->setStyleSheet(style);
}

void ColorSettingsDialog::updateColorButton(QPushButton* button, const QColor& color)
{
    QString style = QString("background-color: %1; border: 1px solid #000000; min-width: 80px;")
                    .arg(color.name());
    button->setStyleSheet(style);
}

bool ColorSettingsDialog::selectColor(QColor& color, const QString& title)
{
    QColorDialog dialog(color, this);
    dialog.setWindowTitle(title);
    dialog.setOptions(QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);

    if (dialog.exec() == QDialog::Accepted)
    {
        color = dialog.selectedColor();
        return true;
    }
    return false;
}

void ColorSettingsDialog::loadSettings()
{
    ColorSettings& settings = ColorSettings::instance();
    m_backgroundColor = settings.backgroundColor();
    m_textColor = settings.textColor();
    m_gridColor = settings.gridColor();
    m_highlightColor = settings.highlightColor();
    m_flatSceneBackgroundColor = settings.flatSceneBackgroundColor();
}

void ColorSettingsDialog::saveSettings()
{
    ColorSettings& settings = ColorSettings::instance();
    settings.setBackgroundColor(m_backgroundColor);
    settings.setTextColor(m_textColor);
    settings.setGridColor(m_gridColor);
    settings.setHighlightColor(m_highlightColor);
    settings.setFlatSceneBackgroundColor(m_flatSceneBackgroundColor);
    settings.saveSettings();
}
