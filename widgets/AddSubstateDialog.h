/** @file AddSubstateDialog.h
 * @brief Dialog for adding a substate to the Substate Parameters panel (and to Header.txt).
 *
 * The user types the name of a substate; "Check" asks the model for the value of that name
 * (Cell::stringEncoding(name)) and accepts the name only when the model returns a plain number.
 * The min, max, no-value and colors are optional; min and max can be calculated from the data.
 * The dialog shows the text to paste into the `substates=` line of Header.txt. */
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include <QColor>
#include <QDialog>

#include "visualiser/SubstateInfo.h"

class QCheckBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

/// @brief Where the dialog gets the values of the cells from (the model which is shown in the Viewer)
struct SubstateValueSource
{
    /// Number of cells in the model (all layers of a native 3D model)
    std::size_t cellCount = 0;

    /// Text which Cell::stringEncoding(substateName) returns for the cell of the given index (0 .. cellCount - 1)
    std::function<std::string(const std::string& substateName, std::size_t cellIndex)> textOfCell;
};

/** @class AddSubstateDialog
 * @brief Collects the parameters of a new substate; OK is possible only for a name the model answers with numbers. */
class AddSubstateDialog : public QDialog
{
    Q_OBJECT

public:
    /** @param source access to the values of the model
     *  @param existing the substates which are already in the panel (in the order of the panel), used to refuse a
     *         duplicate and to write the whole `substates=` line
     *  @param parent parent widget */
    AddSubstateDialog(SubstateValueSource source, std::vector<SubstateInfo> existing, QWidget* parent = nullptr);

    /// @brief The new substate (valid after the dialog was accepted)
    SubstateInfo substate() const;

    /// @brief The text of the `substates=` line of Header.txt: the existing substates and the new one
    QString headerLine() const;

private slots:
    void onNameEdited();
    void onCheckClicked();
    void onCalculateMinMaxClicked();
    void onColorButtonClicked(bool isMinColor);
    void refresh();

private:
    /// @brief Asks the model about the name (sample of the cells). @return true if the model returned numbers
    bool checkName();

    /// @brief The substate as typed in the fields; sets @p error to what is wrong with them (empty if all is fine)
    SubstateInfo readFields(QString& error) const;

    void setStatus(const QString& text, bool ok);
    void setColorButton(QPushButton* button, const QColor& color);
    std::string trimmedName() const;

    SubstateValueSource m_source;
    std::vector<SubstateInfo> m_existing;
    std::vector<std::string> m_existingNames;

    QLineEdit* m_nameEdit = nullptr;
    QPushButton* m_checkButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLineEdit* m_formatEdit = nullptr;
    QLineEdit* m_minEdit = nullptr;
    QLineEdit* m_maxEdit = nullptr;
    QPushButton* m_calculateButton = nullptr;
    QCheckBox* m_noValueCheckBox = nullptr;
    QLineEdit* m_noValueEdit = nullptr;
    QGroupBox* m_colorsGroup = nullptr;
    QPushButton* m_minColorButton = nullptr;
    QPushButton* m_maxColorButton = nullptr;
    QLabel* m_inputErrorLabel = nullptr;
    QLineEdit* m_entryEdit = nullptr;
    QPlainTextEdit* m_lineEdit = nullptr;
    QDialogButtonBox* m_buttons = nullptr;

    QColor m_minColor{0, 0, 255};
    QColor m_maxColor{255, 0, 0};
    std::string m_checkedName; ///< The name the model was asked about and answered with numbers; empty if none
};
