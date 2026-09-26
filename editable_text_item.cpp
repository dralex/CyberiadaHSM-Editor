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
#include "cyberiadasm_editor_items.h"
#include "fontmanager.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiada_constants.h"
#include "settings_manager.h"


EditableTextItem::EditableTextItem(QGraphicsItem *parent):
    QGraphicsTextItem(parent)
{
    setFlags(QGraphicsItem::ItemIsSelectable);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFont(FontManager::instance().font(fontRole));
    connect(&FontManager::instance(), &FontManager::fontsChanged, this, &EditableTextItem::applyFont);
}

EditableTextItem::EditableTextItem(const QString &text, QGraphicsItem *parent):
    QGraphicsTextItem(text, parent)
{
    setFlags(QGraphicsItem::ItemIsSelectable);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFont(FontManager::instance().font(fontRole));
    connect(&FontManager::instance(), &FontManager::fontsChanged, this, &EditableTextItem::applyFont);
}

void EditableTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        SettingsManager::instance().getInspectorMode()) {
        event->ignore();
        return;
    }
    // the border zone of the box belongs to the box: the press falls through
    CyberiadaSMEditorAbstractItem* box = parentBox();
    if (box && !hasFocus() && box->borderZone(mapToParent(event->pos())) != 0) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton && !hasFocus()) {
        event->accept();

        QGraphicsScene *scene = this->scene();
        if (scene) {
            scene->clearSelection();
        }
        if (parentItem()) {
            parentItem()->setSelected(true);
        }

        return;
    }

    QGraphicsTextItem::mousePressEvent(event);
}

void EditableTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        SettingsManager::instance().getInspectorMode()) {
        event->ignore();
        return;
    }
    startEditing();
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void EditableTextItem::startEditing() {
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFocus();
    isEdit = true;
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
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        SettingsManager::instance().getInspectorMode()) {
        event->ignore();
        return;
    }

    if (hasFocus()) {
        setCursor(QCursor(Qt::IBeamCursor));
    }

    QGraphicsTextItem::hoverEnterEvent(event);
}

void EditableTextItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    // the resize cursor of the box shows through the text at its border
    CyberiadaSMEditorAbstractItem* box = parentBox();
    if (box && !hasFocus() && box->isEditable()) {
        CyberiadaSMEditorAbstractItem::applyZoneCursor(this, box->borderZone(mapToParent(event->pos())));
    }
    QGraphicsTextItem::hoverMoveEvent(event);
}

CyberiadaSMEditorAbstractItem* EditableTextItem::parentBox() const
{
    CyberiadaSMEditorAbstractItem* box = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
    if (!box) return nullptr;
    int type = box->type();
    if (type != CyberiadaSMEditorAbstractItem::StateItem &&
        type != CyberiadaSMEditorAbstractItem::CompositeStateItem &&
        type != CyberiadaSMEditorAbstractItem::CommentItem) {
        return nullptr;
    }
    return box;
}

void EditableTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setFont(QFont(font()));

    QGraphicsTextItem::paint(painter, option, widget);
}

void EditableTextItem::setTextAlignment(Qt::Alignment alignment) {
    QTextOption textOption;
    textOption.setAlignment(alignment);
    // wrap at word boundaries (spaces), not mid-word; a long unbroken token overflows
    textOption.setWrapMode(QTextOption::WordWrap);
    document()->setDefaultTextOption(textOption);
}

void EditableTextItem::setFontRole(FontRole role)
{
    fontRole = role;
    // text inside a sized box (a state or a comment) wraps at the box width
    isTextWidthEnabled = (role == fontRoleStateTitle || role == fontRoleStateAction ||
                          role == fontRoleComment || role == fontRoleFormalComment);
    applyFont();
}

void EditableTextItem::setTextWidthEnabled(bool on)
{
    isTextWidthEnabled = on;
    if (!on) {
        setTextWidth(-1);   // natural width, no wrapping
    } else {
        updateTextWidth();
    }
}

void EditableTextItem::setTextMargin(double newTextMargin)
{
    textMargin = newTextMargin;
    updateTextWidth();
}

void EditableTextItem::applyFont()
{
    setFont(FontManager::instance().font(fontRole));
    emit sizeChanged();
    updateTextWidth();
}

void EditableTextItem::updateTextWidth()
{
    if (!isTextWidthEnabled) return;

    CyberiadaSMEditorAbstractItem *parentSMEItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
    if (parentSMEItem && parentSMEItem->hasGeometry()) {
        setTextWidth(parentSMEItem->boundingRect().width() - textMargin);
    }
}

