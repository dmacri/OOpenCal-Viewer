#include <QMouseEvent>
#include "ClickableLabel.h"


ClickableLabel::ClickableLabel(QWidget* parent) : QLabel(parent)
{
    setTextFormat(Qt::RichText);
}

void ClickableLabel::setFileName(QString fileName)
{
    this->fileName = fileName;
    QString styledText = QString("<span style='color:gray'>%1</span> <b>%2</b>")
                             .arg(tr("Input directory: "), fileName);
    setText(styledText);
}

void ClickableLabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit doubleClicked();
    }
    QLabel::mouseDoubleClickEvent(event);
}
