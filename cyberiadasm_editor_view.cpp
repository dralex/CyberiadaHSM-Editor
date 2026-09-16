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
#include <QScrollBar>

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

    // the view owns the pan gesture (see mousePressEvent), so no drag mode is
    // needed; ScrollHandDrag was fragile - a handle under the cursor ate the press
    setDragMode(QGraphicsView::NoDrag);
    panning = false;

    // a QGraphicsView shows the viewport()'s cursor and overwrites the view's on
    // hover, so the tool cursor must be set on the viewport to be visible
    viewport()->unsetCursor();

    switch (currentTool) {
    case ToolType::Zoom:
        viewport()->setCursor(QPixmap(":/Icons/images/zoom-in-32.png"));
        break;
    case ToolType::Pan:
        viewport()->setCursor(Qt::OpenHandCursor);
        break;
    case ToolType::Transition:
    case ToolType::NewSM:
    case ToolType::NewState:
    case ToolType::NewInitial:
    case ToolType::NewFinal:
    case ToolType::NewChoice:
    case ToolType::NewTerminate:
    case ToolType::NewShallowHistory:
    case ToolType::NewDeepHistory:
    case ToolType::NewComment:
    case ToolType::NewFormalComment:
        viewport()->setCursor(Qt::CrossCursor);
        break;
    default:
        viewport()->setCursor(Qt::ArrowCursor);
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

QString CyberiadaSMGraphicsView::viewState() const
{
    return QString("%1 %2 %3").arg(currentScale())
        .arg(horizontalScrollBar()->value()).arg(verticalScrollBar()->value());
}

void CyberiadaSMGraphicsView::applyViewState(const QString& state)
{
    QStringList parts = state.split(' ', QString::SkipEmptyParts);
    if (parts.isEmpty()) return;
    bool ok = false;
    double scale = parts.at(0).toDouble(&ok);
    if (ok && scale > 0.0) setScale(scale);
    if (parts.size() >= 3) {
        horizontalScrollBar()->setValue(parts.at(1).toInt());
        verticalScrollBar()->setValue(parts.at(2).toInt());
    }
}

void CyberiadaSMGraphicsView::wheelEvent(QWheelEvent *event) {
    // the zoom tool (or Ctrl) turns the wheel into zoom; every other tool scrolls
    if (currentTool == ToolType::Zoom || (event->modifiers() & Qt::ControlModifier)) {
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
    // the view owns panning too (unlike ScrollHandDrag it never yields to an item)
    if (currentTool == ToolType::Pan && event->button() == Qt::LeftButton) {
        lastPanPos = event->pos();
        panning = true;
        viewport()->setCursor(Qt::ClosedHandCursor);
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
    if (currentTool == ToolType::Pan && panning) {
        QPoint d = event->pos() - lastPanPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        lastPanPos = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CyberiadaSMGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (currentTool == ToolType::Pan && panning && event->button() == Qt::LeftButton) {
        panning = false;
        viewport()->setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
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
