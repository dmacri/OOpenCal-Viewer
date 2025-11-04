/** @file CompilationLogWidget.cpp
 * @brief Implementation of CompilationLogWidget. */

#include "CompilationLogWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFont>

#include "utilities/ModelLoader.h"


CompilationLogWidget::CompilationLogWidget(QWidget* parent)
    : QDialog(parent)
    , ui(nullptr)
{
    setWindowTitle("Compilation Log");
    setMinimumWidth(700);
    setMinimumHeight(500);
    setModal(true);

    // Create main layout
    auto* mainLayout = new QVBoxLayout(this);

    // Status label
    auto* statusLabel = new QLabel("Compilation Status:", this);
    QFont boldFont = statusLabel->font();
    boldFont.setBold(true);
    statusLabel->setFont(boldFont);
    mainLayout->addWidget(statusLabel);

    // Status text
    auto* statusText = new QLabel(this);
    statusText->setObjectName("statusText");
    mainLayout->addWidget(statusText);

    // Separator
    auto* separator1 = new QLabel(this);
    separator1->setText("─────────────────────────────────────────");
    mainLayout->addWidget(separator1);

    // File info label
    auto* fileLabel = new QLabel("File Information:", this);
    fileLabel->setFont(boldFont);
    mainLayout->addWidget(fileLabel);

    // File info text
    auto* fileText = new QTextEdit(this);
    fileText->setObjectName("fileText");
    fileText->setReadOnly(true);
    fileText->setMaximumHeight(80);
    mainLayout->addWidget(fileText);

    // Separator
    auto* separator2 = new QLabel(this);
    separator2->setText("─────────────────────────────────────────");
    mainLayout->addWidget(separator2);

    // Compiler output label
    auto* outputLabel = new QLabel("Compiler Output:", this);
    outputLabel->setFont(boldFont);
    mainLayout->addWidget(outputLabel);

    // Compiler output text
    auto* outputText = new QTextEdit(this);
    outputText->setObjectName("outputText");
    outputText->setReadOnly(true);
    mainLayout->addWidget(outputText);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    auto* closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

CompilationLogWidget::~CompilationLogWidget() = default;

void CompilationLogWidget::displayCompilationResult(const viz::plugins::CompilationResult& result)
{
    auto* statusText = findChild<QLabel*>("statusText");
    auto* fileText = findChild<QTextEdit*>("fileText");
    auto* outputText = findChild<QTextEdit*>("outputText");

    if (!statusText || !fileText || !outputText)
        return;

    // Display status
    if (result.success)
    {
        statusText->setText("<span style='color: green; font-weight: bold;'>✓ Compilation Successful</span>");
    }
    else
    {
        statusText->setText("<span style='color: red; font-weight: bold;'>✗ Compilation Failed (Exit Code: " +
                           QString::number(result.exitCode) + ")</span>");
    }
    // Display file information
    QString fileInfo;
    fileInfo += "Source File: " + QString::fromStdString(result.sourceFile) + "\n";
    fileInfo += "Output File: " + QString::fromStdString(result.outputFile) + "\n";
    fileInfo += "Command: " + QString::fromStdString(result.compileCommand);
    fileText->setPlainText(fileInfo);

    QString output;

    if (!result.stdout.empty())
    {
        output += "<b>=== Standard Output ===</b><br>";
        output += QString::fromStdString(result.stdout).replace("<", "&lt;").replace(">", "&gt;") + "<br><br>";
    }

    if (!result.stderr.empty())
    {
        output += "<b>=== Error Output ===</b><br>";
        output += formatErrorOutput(result.stderr);
    }

    if (output.isEmpty())
    {
        output = "(No compiler output)";
    }

    outputText->setHtml(output);
}

void CompilationLogWidget::clearLog()
{
    auto* statusText = findChild<QLabel*>("statusText");
    auto* fileText = findChild<QTextEdit*>("fileText");
    auto* outputText = findChild<QTextEdit*>("outputText");

    if (statusText)
        statusText->clear();
    if (fileText)
        fileText->clear();
    if (outputText)
        outputText->clear();
}

QString CompilationLogWidget::formatErrorOutput(const std::string& errorText)
{
    QString formatted = QString::fromStdString(errorText);

    // First, escape HTML special characters
    formatted.replace("&", "&amp;");
    formatted.replace("<", "&lt;");
    formatted.replace(">", "&gt;");
    formatted.replace("\"", "&quot;");

    // Then highlight error lines with proper HTML
    formatted.replace("error:", "<span style='color: red; font-weight: bold;'>error:</span>");
    formatted.replace("warning:", "<span style='color: orange; font-weight: bold;'>warning:</span>");
    formatted.replace("note:", "<span style='color: blue;'>note:</span>");

    return formatted;
}
