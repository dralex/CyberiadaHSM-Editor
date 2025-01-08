#include "editable_text_item.h"

#include <QGraphicsSceneMouseEvent>
#include <QTextCursor>
#include <QFocusEvent>
#include <iostream>
#include <QCursor>
#include <QPainter>
#include <QTextDocument>
#include <QTextBlockFormat>
#include <QDebug>

#include "cyberiadasm_editor_state_item.h"


EditableTextItem::EditableTextItem(const QString &text, QGraphicsItem *parent, bool align)
    : QGraphicsTextItem(text, parent), align(align) {
    setFlags(QGraphicsItem::ItemIsSelectable);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setAlign();
}

void EditableTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton and !hasFocus()) {
        event->accept();
        return;
    }
    QGraphicsTextItem::mousePressEvent(event);
}

void EditableTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFocus();
    isEdit = true;
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void EditableTextItem::keyPressEvent(QKeyEvent *event){
    if (isEdit) {
        CyberiadaSMEditorStateItem *parentRectangle = dynamic_cast<CyberiadaSMEditorStateItem*>(parentItem());
        QGraphicsTextItem::keyPressEvent(event);
        parentRectangle->setPositionText();
        parentItem()->update();
    }
}

void EditableTextItem::focusOutEvent(QFocusEvent *event) {
    setTextInteractionFlags(Qt::NoTextInteraction);
    isEdit = false;
    setPlainText(toPlainText().trimmed());
    QGraphicsTextItem::focusOutEvent(event);
}

void EditableTextItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    if (hasFocus()) {
        setCursor(QCursor(Qt::IBeamCursor));
    } else {
        setCursor(QCursor(Qt::ArrowCursor));
    }

    QGraphicsTextItem::hoverEnterEvent(event);
}

void EditableTextItem::setAlign(){
    CyberiadaSMEditorStateItem *rectParent = dynamic_cast<CyberiadaSMEditorStateItem*>(parentItem());
    if (!align) {
        setTextWidth(rectParent->rect().width() - 30);
    }
    if (boundingRect().width() > rectParent->rect().width() - 30) {
        setTextWidth(rectParent->rect().width() - 30);
    }
    QTextBlockFormat blockFormat;
    if (align) {
        blockFormat.setAlignment(Qt::AlignCenter);
    } else {
        blockFormat.setAlignment(Qt::AlignLeft);
    }
    textCursor().mergeBlockFormat(blockFormat);
}

void EditableTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    setAlign();
    painter->setFont(QFont(font()));

    QGraphicsTextItem::paint(painter, option, widget);
}

