/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Editor View Implementation
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
 *
 * Based on the Qt Visual Graph Editor (QVGE)
 * Copyright (C) 2016-2021 Ars L. Masiuk's <ars.masiuk@gmail.com>
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

#include "cyberiadasm_editor_view.h"

#include <QDebug>

CyberiadaSMGraphicsView::CyberiadaSMGraphicsView(QWidget *parent):
	QGraphicsView(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
	setViewportUpdateMode(BoundingRectViewportUpdate);

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    setRenderHint(QPainter::Antialiasing);
	setOptimizationFlags(DontSavePainterState);
    setOptimizationFlags(DontAdjustForAntialiasing);

	setFocus();

    setTransformationAnchor(AnchorUnderMouse);
}

void CyberiadaSMGraphicsView::setCurrentTool(ToolType tool) {
    currentTool = tool;

    if (tool != ToolType::Pan) {
        setDragMode(QGraphicsView::NoDrag);
    }

    unsetCursor();

    switch (currentTool) {
    case ToolType::ZoomIn:
        setCursor(QPixmap(":/Icons/images/zoom-in-32.png"));
        break;
    case ToolType::ZoomOut:
        setCursor(QPixmap(":/Icons/images/zoom-out-32.png"));
        break;
    case ToolType::Pan:
        setDragMode(QGraphicsView::ScrollHandDrag);
        break;
    default:
        setCursor(Qt::ArrowCursor);
        break;
    }
}

void CyberiadaSMGraphicsView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier){
        double scaleFactor = 1.1;
        if (event->angleDelta().y() > 0) {
            scale(scaleFactor, scaleFactor);
        } else {
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }
    }
    else {
        QGraphicsView::wheelEvent(event);
    }
}

void CyberiadaSMGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (currentTool == ToolType::ZoomIn) {
        scale(1.25, 1.25);
    } else if (currentTool == ToolType::ZoomOut) {
        scale(0.8, 0.8);
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}
