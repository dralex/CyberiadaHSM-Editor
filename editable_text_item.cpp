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

#include <QGraphicsSceneMouseEvent>
#include <QTextCursor>
#include <QFocusEvent>
#include <iostream>
#include <QCursor>
#include <QPainter>
#include <QTextDocument>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QDebug>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QGraphicsSceneContextMenuEvent>

#include "editable_text_item.h"
#include "cyberiadasm_editor_items.h"
#include "fontmanager.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiada_constants.h"
#include "settings_manager.h"
#include "gesture_log.h"


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
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->readOnly()) {
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
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->readOnly()) {
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
    // a fragment underlined in this text gives way while it is edited (EDIT-EDGE-21)
    if (CyberiadaSMEditorScene* sc = dynamic_cast<CyberiadaSMEditorScene*>(scene())) sc->updateSubjectLinks();
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
    if (CyberiadaSMEditorScene* sc = dynamic_cast<CyberiadaSMEditorScene*>(scene())) sc->updateSubjectLinks();
    if (CyberiadaSMEditorScene::discardsEdits(this)) return;
    emit editingFinished();
}

void EditableTextItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    CyberiadaSMEditorScene* sc = dynamic_cast<CyberiadaSMEditorScene*>(scene());
    if (!isEdit || !sc) {
        QGraphicsTextItem::contextMenuEvent(event);
        return;
    }
    // the menu takes the focus, which ends the edit and may rebuild this item: the
    // selection is read now and nothing of the item is used after the menu
    QString selected = textCursor().selectedText();
    selected.replace(QChar::ParagraphSeparator, '\n').replace(QChar::LineSeparator, '\n');
    Cyberiada::Element* target = nullptr;
    Cyberiada::CommentSubjectType type = Cyberiada::commentSubjectElement;
    QString fragment;
    bool commentable = !dynamic_cast<CyberiadaSMEditorScene*>(scene())->readOnly() &&
        sc->selectionFragment(this, &target, &type, &fragment);
    QMenu menu;
    QAction* copy = menu.addAction(tr("Copy"));
    copy->setEnabled(!selected.isEmpty());
    menu.addSeparator();
    QMenu* comments = menu.addMenu(tr("Comment the Selection"));
    comments->setEnabled(commentable);
    QMap<QAction*, Cyberiada::ID> choices;
    choices.insert(comments->addAction(tr("New Comment")), Cyberiada::ID());
    QList<Cyberiada::Comment*> existing = sc->selectionComments(this);
    if (!existing.isEmpty()) comments->addSeparator();
    for (Cyberiada::Comment* c : existing) {
        // a comment reads by its name or the first line of its body
        QString label = c->has_name() && !c->get_name().empty() ? QString::fromStdString(c->get_name()) :
            QString::fromStdString(c->get_body()).section('\n', 0, 0);
        if (label.length() > 32) label = label.left(31) + QStringLiteral("…");
        choices.insert(comments->addAction(QString("%1 (%2)").arg(label, QString::fromStdString(c->get_id()))),
                       c->get_id());
    }
    // the commit the focus change makes is replayed by the verb logged below
    GestureLog::instance().enterGesture();
    QAction* chosen = menu.exec(event->screenPos());
    GestureLog::instance().leaveGesture();
    event->accept();
    if (chosen && choices.contains(chosen)) {
        sc->commentFragment(target, type, fragment, choices.value(chosen));
        return;
    }
    if (chosen == copy) QApplication::clipboard()->setText(selected);
    GestureLog::instance().logGesture("commit");
}

void EditableTextItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    if (dynamic_cast<CyberiadaSMEditorScene*>(scene())->getCurrentTool() != ToolType::Select ||
        dynamic_cast<CyberiadaSMEditorScene*>(scene())->readOnly()) {
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

void EditableTextItem::setCodeRole(CodeRole role)
{
    if (role == codeRoleNone) {
        delete highlighter;
        highlighter = nullptr;
    } else if (highlighter) {
        highlighter->setRole(role);
    } else {
        highlighter = new CodeHighlighter(document(), role, sceneCodeLanguage(scene()));
        documentMarkup = sceneMarkup(scene());
        applyMarkup();
    }
}

void EditableTextItem::setMarkupOverride(const QString& markup)
{
    markupOverride = markup;
    applyMarkup();
}

void EditableTextItem::setDocumentMarkup(CodeMarkup markup)
{
    documentMarkup = markup;
    applyMarkup();
}

void EditableTextItem::applyMarkup()
{
    if (!highlighter) return;
    highlighter->setMarkup(CodeStyle::commentMarkup(markupOverride, documentMarkup));
}

CodeMarkup EditableTextItem::sceneMarkup(QGraphicsScene* s)
{
    CyberiadaSMEditorScene* smScene = dynamic_cast<CyberiadaSMEditorScene*>(s);
    return smScene ? smScene->markupLanguage() : codeMarkupPlain;
}

void EditableTextItem::setCodeLanguage(const CodeLanguage* language)
{
    if (highlighter) highlighter->setLanguage(language);
}

const CodeLanguage* EditableTextItem::sceneCodeLanguage(QGraphicsScene* s)
{
    CyberiadaSMEditorScene* smScene = dynamic_cast<CyberiadaSMEditorScene*>(s);
    return smScene ? smScene->codeLanguage() : nullptr;
}

QVariant EditableTextItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemSceneHasChanged) {
        QGraphicsScene* s = value.value<QGraphicsScene*>();
        setCodeLanguage(sceneCodeLanguage(s));
        setDocumentMarkup(sceneMarkup(s));
    }
    return QGraphicsTextItem::itemChange(change, value);
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
    applyRichFormat();
    emit sizeChanged();
    updateTextWidth();
}

void EditableTextItem::setBoldRange(int length, bool bold)
{
    // the first `length` characters are bold (when asked), the rest is forced back
    // to normal - so bold left on the tail by in-place editing (e.g. a guard typed
    // right after the event name) never survives the reformat
    int end = document()->characterCount() - 1;
    int split = qBound(0, length, end);
    QTextCursor cursor(document());
    if (split > 0) {
        cursor.setPosition(0);
        cursor.setPosition(split, QTextCursor::KeepAnchor);
        QTextCharFormat fmt;
        fmt.setFontWeight(bold ? QFont::Bold : QFont::Normal);
        cursor.mergeCharFormat(fmt);
    }
    if (split < end) {
        cursor.setPosition(split);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        QTextCharFormat fmt;
        fmt.setFontWeight(QFont::Normal);
        cursor.mergeCharFormat(fmt);
    }
}

void EditableTextItem::updateTextWidth()
{
    if (!isTextWidthEnabled) return;

    CyberiadaSMEditorAbstractItem *parentSMEItem = dynamic_cast<CyberiadaSMEditorAbstractItem*>(parentItem());
    if (parentSMEItem && parentSMEItem->hasGeometry()) {
        setTextWidth(parentSMEItem->boundingRect().width() - textMargin);
    }
}

