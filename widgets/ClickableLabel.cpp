#include <QMouseEvent>
#include <QFontMetrics>
#include <QResizeEvent>
#include <algorithm>
#include <utility>
#include "ClickableLabel.h"


ClickableLabel::ClickableLabel(QWidget* parent) : QLabel(parent)
{
    setTextFormat(Qt::RichText);
}

void ClickableLabel::setFileName(QString fileName)
{
    this->fileName = std::move(fileName);
    autoDisplayDirectoryPath = false;

    if (this->fileName.isEmpty())
    {
        displayDirectoryPath.clear();
        QLabel::setText(QString());
        return;
    }

    displayDirectoryPath = this->fileName;
    updateDisplayedDirectoryPath();
}

void ClickableLabel::setDisplayDirectoryPath(QString directoryPath)
{
    displayDirectoryPath = std::move(directoryPath);
    autoDisplayDirectoryPath = true;
    updateDisplayedDirectoryPath();
}

void ClickableLabel::updateDisplayedDirectoryPath()
{
    const QString prefix = tr("Input directory: ");
    const QFontMetrics metrics(font());
    const int prefixWidth = metrics.horizontalAdvance(prefix);
    const int availableWidth = std::max(width() - prefixWidth - 16, 48);
    const QString visiblePath = metrics.elidedText(displayDirectoryPath, Qt::ElideMiddle, availableWidth);

    const QString styledText = QString("<span style='color:gray'>%1</span> <b>%2</b>")
                                   .arg(prefix.toHtmlEscaped())
                                   .arg(visiblePath.toHtmlEscaped());
    QLabel::setText(styledText);
}

QSize ClickableLabel::sizeHint() const
{
    QSize hint = QLabel::sizeHint();
    hint.setWidth(360);
    return hint;
}

QSize ClickableLabel::minimumSizeHint() const
{
    QSize hint = QLabel::minimumSizeHint();
    hint.setWidth(160);
    return hint;
}

void ClickableLabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit doubleClicked();
    }
    QLabel::mouseDoubleClickEvent(event);
}

void ClickableLabel::resizeEvent(QResizeEvent* event)
{
    QLabel::resizeEvent(event);

    if (autoDisplayDirectoryPath)
    {
        updateDisplayedDirectoryPath();
    }
}
