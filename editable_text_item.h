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


class EditableTextItem : public QGraphicsTextItem {
    Q_OBJECT
public:
    explicit EditableTextItem(QGraphicsItem *parent = nullptr);
    explicit EditableTextItem(const QString &text, QGraphicsItem *parent = nullptr);

    void setFontStyleChangeable(bool isChangeable);
    void setFontBoldness(bool isBold);
    void setTextMargin(double newTextMargin);

protected:
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setTextAlignment(Qt::Alignment alignment);
    void setTextWidthEnabled(bool enabled);

signals:
    void sizeChanged();
    void editingFinished();

protected slots:
    void onFontChanged(const QFont &newFont) ;

protected:
    void updateTextWidth();
    bool isEdit;
    bool align;
    bool isFontStyleChangeable = true;
    bool isBold = false;
    bool isTextWidthEnabled = true;
    double textMargin;
};



#endif // EDITABLETEXTITEM_H
