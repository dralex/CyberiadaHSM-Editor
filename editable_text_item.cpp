/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * Editable Text Item for the State Machine Editor Scene Items
 *
 * Copyright (C) 2025 Anastasia Viktorova <viktorovaa.04@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see https://www.gnu.org/licenses/
 *
 * ----------------------------------------------------------------------------- */

#include <QGraphicsSceneMouseEvent>
#include <QTextCursor>
#include <QFocusEvent>
#include <iostream>
#include <QCursor>
#include <QPainter>
#include <QTextDocument>
#include <QTextBlockFormat>
#include <QDebug>

#include "editable_text_item.h"
// #include "cyberiadasm_editor_state_item.h"
#include "fontmanager.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiada_constants.h"


EditableTextItem::EditableTextItem(const QString &text, QGraphicsItem *parent):
    QGraphicsTextItem(text, parent)
{
    setFlags(QGraphicsItem::ItemIsSelectable);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFont(FontManager::instance().getFont());
    connect(&FontManager::instance(), &FontManager::fontChanged, this, &EditableTextItem::onFontChanged);
}

void EditableTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton && !hasFocus()) {
        event->accept();
        return;
    }

    QGraphicsTextItem::mousePressEvent(event);
}

void EditableTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select) {
        event->ignore();
        return;
    }
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFocus();
    isEdit = true;
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void EditableTextItem::keyPressEvent(QKeyEvent *event) {
    if (isEdit) {
        QGraphicsTextItem::keyPressEvent(event);
    }
}

void EditableTextItem::focusOutEvent(QFocusEvent *event) {
    setTextInteractionFlags(Qt::NoTextInteraction);
    isEdit = false;
    setPlainText(toPlainText().trimmed());
    QGraphicsTextItem::focusOutEvent(event);
    emit editingFinished();
}

void EditableTextItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    // event->ignore();

    if (hasFocus()) {
        setCursor(QCursor(Qt::IBeamCursor));
    }

    QGraphicsTextItem::hoverEnterEvent(event);
}

void EditableTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setFont(QFont(font()));

    QGraphicsTextItem::paint(painter, option, widget);
}

void EditableTextItem::setTextAlignment(Qt::Alignment alignment) {
    QTextOption textOption;
    textOption.setAlignment(alignment);
    textOption.setWrapMode(QTextOption::WrapAnywhere);
    document()->setDefaultTextOption(textOption);
}

void EditableTextItem::setTextWidthEnabled(bool enabled) {
    isTextWidthEnabled = enabled;
}

void EditableTextItem::setFontStyleChangeable(bool isChangeable) {
    isFontStyleChangeable = isChangeable;
}

void EditableTextItem::setFontBoldness(bool isBold)
{
    this->isBold = isBold;
    onFontChanged(font());
}

void EditableTextItem::setTextMargin(double newTextMargin)
{
    textMargin = newTextMargin;
    updateTextWidth();
}

void EditableTextItem::onFontChanged(const QFont &newFont)
{
    if(!isFontStyleChangeable) {
        QFont newFontDiffSize = font();
        newFontDiffSize.setPointSize(newFont.pointSize());
        setFont(newFontDiffSize);
        emit sizeChanged();
        return;
    }
    if (isBold) {
        QFont newBoldFont = newFont;
        newBoldFont.setBold(isBold);
        setFont(newBoldFont);
        emit sizeChanged();
        return;
    }
    setFont(newFont);
    emit sizeChanged();

    updateTextWidth();
}

void EditableTextItem::updateTextWidth()
{
    if (!isTextWidthEnabled) return;

    CyberiadaSMEditorAbstractItem *parentSMEItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
    if (parentSMEItem && parentSMEItem->hasGeometry()) {
        setTextWidth(parentSMEItem->boundingRect().width() - textMargin);
        qDebug() << "updateTextWidth with margin " << toPlainText() << textMargin;
    }
}

