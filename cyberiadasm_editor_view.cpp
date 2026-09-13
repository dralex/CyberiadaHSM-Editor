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
#include <QRubberBand>
#include <QMouseEvent>
#include <QContextMenuEvent>

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
    case ToolType::Zoom:
        setCursor(QPixmap(":/Icons/images/zoom-in-32.png"));
        break;
    case ToolType::Pan:
        setDragMode(QGraphicsView::ScrollHandDrag);
        break;
    case ToolType::Transition:
    case ToolType::NewSM:
    case ToolType::NewState:
    case ToolType::NewInitial:
    case ToolType::NewFinal:
    case ToolType::NewChoice:
    case ToolType::NewTerminate:
    case ToolType::NewComment:
    case ToolType::NewFormalComment:
        setCursor(Qt::CrossCursor);
        break;
    default:
        setCursor(Qt::ArrowCursor);
        break;
    }
}

qreal CyberiadaSMGraphicsView::currentScale() const
{
    return transform().m11();
}

void CyberiadaSMGraphicsView::setScale(qreal scale)
{
    if (scale <= 0.0) return;
    QTransform t;
    t.scale(scale, scale);
    setTransform(t);
    emit scaleChanged(currentScale());
}

void CyberiadaSMGraphicsView::zoomBy(qreal factor)
{
    scale(factor, factor);
    emit scaleChanged(currentScale());
}

void CyberiadaSMGraphicsView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier){
        zoomBy(event->angleDelta().y() > 0 ? 1.1 : 1.0 / 1.1);
    }
    else {
        QGraphicsView::wheelEvent(event);
    }
}

void CyberiadaSMGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (currentTool == ToolType::Zoom) {
        if (event->button() == Qt::RightButton) {
            zoomBy(0.8);                       // right click zooms out
        } else if (event->button() == Qt::LeftButton) {
            // arm a rubber band; a plain click zooms in, a drag zooms to the rect
            zoomOrigin = event->pos();
            zoomDragging = false;
            if (!zoomBand) zoomBand = new QRubberBand(QRubberBand::Rectangle, viewport());
            zoomBand->setGeometry(QRect(zoomOrigin, QSize()));
            zoomBand->show();
        }
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void CyberiadaSMGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (currentTool == ToolType::Zoom && zoomBand && zoomBand->isVisible()) {
        if ((event->pos() - zoomOrigin).manhattanLength() >= 8) zoomDragging = true;
        zoomBand->setGeometry(QRect(zoomOrigin, event->pos()).normalized());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CyberiadaSMGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (currentTool == ToolType::Zoom && zoomBand && zoomBand->isVisible() &&
        event->button() == Qt::LeftButton) {
        QRect band = zoomBand->geometry();
        zoomBand->hide();
        if (zoomDragging && band.width() > 4 && band.height() > 4) {
            fitInView(mapToScene(band).boundingRect(), Qt::KeepAspectRatio);
            emit scaleChanged(currentScale());
        } else {
            zoomBy(1.25);                      // a plain click zooms in
        }
        zoomDragging = false;
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void CyberiadaSMGraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    // the zoom tool uses the right button to zoom out, so no context menu then
    if (currentTool == ToolType::Zoom) { event->accept(); return; }
    QGraphicsView::contextMenuEvent(event);
}
