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

#ifndef EDITABLETEXTITEM_H
#define EDITABLETEXTITEM_H

#include <QGraphicsTextItem>

#include "cyberiada_constants.h"
#include "code_highlighter.h"

class CyberiadaSMEditorAbstractItem;


class EditableTextItem : public QGraphicsTextItem {
    Q_OBJECT
public:
    explicit EditableTextItem(QGraphicsItem *parent = nullptr);
    explicit EditableTextItem(const QString &text, QGraphicsItem *parent = nullptr);

    // the role sets the font: the size, the boldness and the fixed family
    void setFontRole(FontRole role);
    FontRole getFontRole() const { return fontRole; }
    // the code parts are highlighted by the platform language (EDIT-TEXT-7)
    void setCodeRole(CodeRole role);
    CodeRole getCodeRole() const { return highlighter ? highlighter->getRole() : codeRoleNone; }
    // the language of the document; the scene pushes a change
    void setCodeLanguage(const CodeLanguage* language);
    const CodeLanguage* getCodeLanguage() const { return highlighter ? highlighter->getLanguage() : nullptr; }
    // the code language of the document drawn by the scene, or null
    static const CodeLanguage* sceneCodeLanguage(QGraphicsScene* s);
    // the wrap width follows the parent box, refreshed after a resize
    void updateTextWidth();
    // a title that hugs its text (a state machine header) rather than the box
    void setTextWidthEnabled(bool on);
    void setTextAlignment(Qt::Alignment alignment);
    void setTextMargin(double newTextMargin);
    // enter edit mode programmatically (a double click on the owner, say)
    void startEditing();
    // the leading characters an edit may not touch (the action type)
    virtual int protectedLength() const { return 0; }
    // re-apply per-character formatting (a bold prefix) after the font changes or
    // after the owner sets the text; a plain text item has none
    virtual void applyRichFormat() {}

protected:
    // the items are built before they join the scene of their document
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // weight the leading [0, length) characters bold or normal (idempotent)
    void setBoldRange(int length, bool bold);

    // the resizable box the text belongs to, or null
    CyberiadaSMEditorAbstractItem* parentBox() const;

signals:
    void sizeChanged();
    // the edit mode ended: the owner reads the text back
    void editingFinished();

protected slots:
    void applyFont();

protected:
    bool isEdit = false;
    bool align = false;
    bool isTextWidthEnabled = true;
    FontRole fontRole = fontRoleStateAction;
    // the comment body keeps the default: it wraps at the full element width
    double textMargin = 0;
    // owned by the document
    CodeHighlighter* highlighter = nullptr;
};



#endif // EDITABLETEXTITEM_H
