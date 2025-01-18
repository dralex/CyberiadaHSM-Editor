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
#include "fontmanager.h"


EditableTextItem::EditableTextItem(const QString &text, QGraphicsItem *parent, bool align, bool parentHasGeometry)
    : QGraphicsTextItem(text, parent), align(align), parentHasGeometry(parentHasGeometry) {
    setFlags(QGraphicsItem::ItemIsSelectable);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setAlign();
    setFont(FontManager::instance().getFont());

    connect(&FontManager::instance(), &FontManager::fontChanged, this, &EditableTextItem::onFontChanged);
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
        // CyberiadaSMEditorStateItem *parentRectangle = dynamic_cast<CyberiadaSMEditorStateItem*>(parentItem());
        CyberiadaSMEditorAbstractItem *parentSMEItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
        QGraphicsTextItem::keyPressEvent(event);
        parentSMEItem->setPositionText();
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

    QGraphicsTextItem::hoverMoveEvent(event);
}

void EditableTextItem::setAlign(){
    if (parentHasGeometry) {
        // CyberiadaSMEditorStateItem *rectParent = dynamic_cast<CyberiadaSMEditorStateItem*>(parentItem());
        CyberiadaSMEditorAbstractItem *parentSMEItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
        if (!align) {
            // setTextWidth(parentSMEItem->rect().width() - 30);
            setTextWidth(parentSMEItem->boundingRect().width() - 30);
        }
        // if (boundingRect().width() > parentSMEItem->rect().width() - 30) {
        if (boundingRect().width() > parentSMEItem->boundingRect().width() - 30) {
            // setTextWidth(parentSMEItem->rect().width() - 30);
            setTextWidth(parentSMEItem->boundingRect().width() - 30);
        }
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

void EditableTextItem::setFontStyleChangeable(bool isChangeable)
{
    isFontStyleChangeable = isChangeable;
}

void EditableTextItem::onFontChanged(const QFont &newFont)
{
    if(isFontStyleChangeable) {
        QFont newFontDiffSize = font();
        newFontDiffSize.setPointSize(newFont.pointSize());
        setFont(newFontDiffSize);
        emit sizeChanged();
        return;
    }
    setFont(newFont);
    emit sizeChanged();
}

