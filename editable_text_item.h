/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
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
#include "find_query.h"

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
    // the markup of an informal comment body: its own dMarkup, else the
    // document markupLanguage the scene pushes (EDIT-TEXT-11)
    void setMarkupOverride(const QString& markup);
    void setDocumentMarkup(CodeMarkup markup);
    CodeMarkup getCodeMarkup() const { return highlighter ? highlighter->getMarkup() : codeMarkupPlain; }
    static CodeMarkup sceneMarkup(QGraphicsScene* s);
    // a Markdown text drawn rendered while it is not edited (EDIT-TEXT-14), its
    // images relative to baseDir; the owner turns it on for a comment body
    void setMarkdownRendering(bool on, const QString& baseDir);
    bool isRendered() const { return rendered; }
    // the text as written, also while it is drawn rendered
    QString sourceText() const { return rendered ? source : toPlainText(); }
    void setSourceText(const QString& text);
    // the wrap width follows the parent box, refreshed after a resize
    void updateTextWidth();
    // the height of the whole text, uncut by its band (EDIT-NODE-16)
    qreal fullHeight() const { return QGraphicsTextItem::boundingRect().height(); }
    // the height its band leaves the text (EDIT-NODE-16): a taller one is cut at
    // its last whole line with an ellipsis, the whole text in the tooltip, and
    // shown whole while edited; a negative height sets no band
    void setBandHeight(qreal height);
    bool isCut() const;
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF& point) const override;
    // a title that hugs its text (a state machine header) rather than the box
    void setTextWidthEnabled(bool on);
    void setTextAlignment(Qt::Alignment alignment);
    void setTextMargin(double newTextMargin);
    // enter edit mode programmatically (a double click on the owner, say)
    void startEditing();
    bool isEditing() const { return isEdit; }
    // leave the edit mode with the text as it was at its start, the document
    // untouched (EDIT-KEYS-11)
    void cancelEditing();
    // the leading characters an edit may not touch (the action type)
    virtual int protectedLength() const { return 0; }
    // re-apply per-character formatting (a bold prefix) after the font changes or
    // after the owner sets the text; a plain text item has none
    virtual void applyRichFormat() {}

    // the find marks (EDIT-FIND-7): boxes behind the ranges, the current one orange
    void setFindRanges(const FindRanges& ranges, int current);
    const FindRanges& getFindRanges() const { return findMarks; }
    int getFindCurrent() const { return findCurrentMark; }
    // the box of a range, or the ellipsis when the cut hides a part of it
    QRectF findRangeBounds(int from, int to) const;

protected:
    // the items are built before they join the scene of their document
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event);
    // Esc is the edit's ahead of the action it is a key of
    bool sceneEvent(QEvent* event) override;
    // while editing: Copy and Comment the Selection (EDIT-TOOL-17)
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    // the find marks behind the glyphs: a label with its own background paints them over it
    void paintFindMarks(QPainter* painter) const;

    // weight the leading [0, length) characters bold or normal (idempotent)
    void setBoldRange(int length, bool bold);
    // the edit mode shows the whole text: the box changes with it
    void setEditing(bool on);

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
    qreal bandHeight = -1.0;
    bool isTextWidthEnabled = true;
    FontRole fontRole = fontRoleStateAction;
    // the comment body keeps the default: it wraps at the full element width
    double textMargin = 0;
    // owned by the document
    CodeHighlighter* highlighter = nullptr;
    QString markupOverride;
    CodeMarkup documentMarkup = codeMarkupPlain;

private:
    // the shown part of a cut text and its ellipsis; false when it is not cut
    bool cutParts(QPainterPath* shown, QRectF* ellipsis) const;
    // a part of the range is not in the shown part of a cut text
    bool rangeHidden(int from, int to, const QPainterPath& shown) const;
    // the drawn text changed: the scene tells the find (EDIT-FIND-8)
    void reportTextsChanged();

    void applyMarkup();
    // rendered when asked, not edited and Markdown; the source otherwise
    void applyRendering();
    void showSource();

    bool renderMarkdown = false;
    bool rendered = false;
    QString source;
    // the text at the start of the edit, which Esc gives back
    QString editOrigin;
    QString markdownBase;
    FindRanges findMarks;
    int findCurrentMark = -1;
};



#endif // EDITABLETEXTITEM_H
