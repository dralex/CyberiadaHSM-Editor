/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Editor Scene
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

#ifndef CYBERIADA_SM_EDITOR_SCENE_HEADER
#define CYBERIADA_SM_EDITOR_SCENE_HEADER

#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QSet>
#include <QMenu>
#include <QByteArrayList>

#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_items.h"

class CyberiadaSMEditorScene: public QGraphicsScene {
Q_OBJECT

public:
    CyberiadaSMEditorScene(CyberiadaSMModel* model, QObject *parent = NULL);
	virtual ~CyberiadaSMEditorScene();

	void reset();
	
    void  setGridSize(int newSize);
    int   getGridSize() const { return gridSize; }

    bool  isGridEnabled() const	{ return gridEnabled; }
    bool  isGridSnapEnabled() const { return gridSnap; }

    void  setGridPen(const QPen& gridPen);
    const QPen& getGridPen() const { return gridPen; }

    void updateScene();

public slots:
	void  slotElementSelected(const QModelIndex& index);
	
    void  enableGrid(bool on = true);
    void  enableGridSnap(bool on = true);

protected:
	void  drawBackground(QPainter *painter, const QRectF &);
	
private:
	void  addItemsRecursively(QGraphicsItem* parent, Cyberiada::ElementCollection* element);
	
	CyberiadaSMModel*              model;
	Cyberiada::StateMachine*       currentSM;
    QMap<Cyberiada::ID, QGraphicsItem*> elementItem;
	
    int                            gridSize;
    bool                           gridEnabled;
    bool                           gridSnap;
    QPen                           gridPen;
};

#endif
