/** @file CompilationLogWidget.cpp
 * @brief Implementation of CompilationLogWidget. */

#include "CompilationLogWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QFontDatabase>

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

    // Title: Compilation Status
    auto* statusLabel = new QLabel("Compilation Status:", this);
    QFont boldFont = statusLabel->font();
    boldFont.setBold(true);
    statusLabel->setFont(boldFont);
    mainLayout->addWidget(statusLabel);

    // Dynamic status text
    auto* statusText = new QLabel(this);
    statusText->setObjectName("statusText");
    statusText->setWordWrap(true);
    mainLayout->addWidget(statusText);

    // Separator
    mainLayout->addWidget(new QLabel("────────────────────────────────────────────", this));

    // File Information
    auto* fileLabel = new QLabel("File Information:", this);
    fileLabel->setFont(boldFont);
    mainLayout->addWidget(fileLabel);

    // File info text
    auto* fileText = new QTextEdit(this);
    fileText->setObjectName("fileText");
    fileText->setReadOnly(true);
    fileText->setMaximumHeight(80);
    fileText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    mainLayout->addWidget(fileText);

    // Separator
    mainLayout->addWidget(new QLabel("────────────────────────────────────────────", this));

    // Compiler Output
    auto* outputLabel = new QLabel("Compiler Output:", this);
    outputLabel->setFont(boldFont);
    mainLayout->addWidget(outputLabel);

    // Compiler output text
    auto* outputText = new QTextEdit(this);
    outputText->setObjectName("outputText");
    outputText->setReadOnly(true);
    outputText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    outputText->setLineWrapMode(QTextEdit::NoWrap);
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

    // --- Status ---
    if (result.success)
    {
        statusText->setText("<span style='color: green; font-weight: bold;'>✓ Compilation Successful</span>");
    }
    else
    {
        // Check if this is a compiler not found error
        if (result.stderr.find("No C++ compiler found") != std::string::npos)
        {
            statusText->setText("<span style='color: red; font-weight: bold;'>✗ C++ Compiler Not Found</span>");
        }
        else
        {
            statusText->setText("<span style='color: red; font-weight: bold;'>✗ Compilation Failed (Exit Code: " +
                                QString::number(result.exitCode) + ")</span>");
        }
    }
    // --- File info ---
    QString fileInfo;
    fileInfo += "Source File: " + QString::fromStdString(result.sourceFile) + "\n";
    fileInfo += "Output File: " + QString::fromStdString(result.outputFile) + "\n";
    fileInfo += "Command: " + QString::fromStdString(result.compileCommand);
    fileText->setPlainText(fileInfo);

    // --- Output formatting ---
    QString output;
    if (! result.stdout.empty())
    {
        output += "<b>=== Standard Output ===</b><br>";
        QString stdoutFormatted = QString::fromStdString(result.stdout);
        stdoutFormatted.replace("&", "&amp;");
        stdoutFormatted.replace("<", "&lt;");
        stdoutFormatted.replace(">", "&gt;");
        stdoutFormatted.replace("\n", "<br>");
        output += "<pre style='font-family: monospace;'>" + stdoutFormatted + "</pre><br>";
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

    // Escape HTML
    formatted.replace("&", "&amp;");
    formatted.replace("<", "&lt;");
    formatted.replace(">", "&gt;");
    formatted.replace("\"", "&quot;");

    // Preserve line breaks
    formatted.replace("\n", "<br>");

    // Highlight compiler message types
    formatted.replace("error:",   "<span style='color: red; font-weight: bold;'>error:</span>");
    formatted.replace("warning:", "<span style='color: orange; font-weight: bold;'>warning:</span>");
    formatted.replace("note:",    "<span style='color: blue;'>note:</span>");

    // Monospace block
    formatted = "<pre style='font-family: monospace;'>" + formatted + "</pre>";

    return formatted;
}
