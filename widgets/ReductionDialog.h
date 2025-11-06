/** @file ReductionDialog.h
 * @brief Declaration of the ReductionDialog class for displaying reduction values. */

#pragma once

#include <QDialog>
#include <QString>
#include <map>

/** @class ReductionDialog
 * @brief A dialog window for displaying reduction values for a specific step.
 *
 * This dialog shows all reduction values (sum, min, max, etc.) for the current
 * simulation step in a nicely formatted list. */
class ReductionDialog : public QDialog
{
    Q_OBJECT

public:
    /** @brief Constructs a ReductionDialog with parent widget.
     *  @param reductionData Map of reduction names to their values
     *  @param stepNumber The current simulation step
     *  @param parent Parent widget */
    explicit ReductionDialog(const std::map<QString, QString>& reductionData,
                            int stepNumber,
                            QWidget* parent = nullptr);

    ~ReductionDialog() = default;

private:
    /** @brief Creates and configures the UI elements */
    void setupUI(const std::map<QString, QString>& reductionData, int stepNumber);
};
