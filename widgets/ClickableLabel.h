/** @file ClickableLabel.h
 * @brief Declaration of the ClickableLabel class widget (used in GUI), a QLabel with double click handling.
 * The class stores the path to the loaded Header.txt while displaying the selected simulation directory. */

#pragma once

#include <QLabel>

class QMouseEvent;

/** @class ClickableLabel
 * @brief A QLabel that emits a signal when double-clicked.
 * 
 * This class extends QLabel to provide double-click functionality and loaded path association. */
class ClickableLabel : public QLabel
{
    Q_OBJECT

public:
    /// @brief Constructs a ClickableLabel with the given parent.
    explicit ClickableLabel(QWidget* parent = nullptr);

    /// @brief Sets the loaded Header.txt path associated with this label.
    void setFileName(QString fileName);

    /// @brief Returns the loaded Header.txt path associated with this label.
    const QString& getFileName() const
    {
        return fileName;
    }

signals:
    /// @brief Emitted when the label is double-clicked.
    void doubleClicked();

protected:
    /// @brief Handles mouse double-click events.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QString fileName;
};
