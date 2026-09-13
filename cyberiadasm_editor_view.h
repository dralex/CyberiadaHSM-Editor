/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Editor View
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

#ifndef CYBERIADA_SM_EDITOR_VIEW_HEADER
#define CYBERIADA_SM_EDITOR_VIEW_HEADER

#include <QGraphicsView>
#include <QPaintEvent>

#include "cyberiada_constants.h"

class QRubberBand;

class CyberiadaSMGraphicsView: public QGraphicsView {
Q_OBJECT

public:
	CyberiadaSMGraphicsView(QWidget *parent = NULL);

	void paintEvent(QPaintEvent * event) {
        QPaintEvent* newEvent = new QPaintEvent(event->region().boundingRect());
        QGraphicsView::paintEvent(newEvent);
		delete newEvent;
	}

    void setCurrentTool(ToolType tool);

    // the uniform zoom factor (1.0 == 100%); setScale re-zooms around the view
    // centre and both paths emit scaleChanged so the zoom combo stays in sync
    qreal currentScale() const;
    void  setScale(qreal scale);

    // the view state persisted in the document (scale and scroll offsets)
    QString viewState() const;
    void    applyViewState(const QString& state);

public slots:
    void  zoomIn()  { zoomBy(1.25); }
    void  zoomOut() { zoomBy(0.8); }

signals:
    void scaleChanged(qreal scale);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    // scale by a factor about the cursor and announce the new scale
    void zoomBy(qreal factor);

    ToolType currentTool = ToolType::Select;
    // the zoom tool's rubber band: a left drag zooms to the framed rect
    QRubberBand* zoomBand = nullptr;
    QPoint       zoomOrigin;
    bool         zoomDragging = false;
};

#endif
