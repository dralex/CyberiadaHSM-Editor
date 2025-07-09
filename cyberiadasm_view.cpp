/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine View implementation
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
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

#include <QDebug>
#include <QDrag>
#include <QCursor>
#include <QTreeView>

#include "cyberiadasm_model.h"
#include "cyberiadasm_view.h"
#include "myassert.h"

CyberiadaSMView::CyberiadaSMView(QWidget* parent):
	QTreeView(parent)
{
	setDragEnabled(true);
	setAcceptDrops(true);
	setDropIndicatorShown(true);
	setDragDropMode(QAbstractItemView::DragDrop);
	setDefaultDropAction(Qt::MoveAction);
	setEditTriggers(QAbstractItemView::SelectedClicked);
//	setSelectionMode(QAbstractItemView::MultiSelection);
}

void CyberiadaSMView::select(const QModelIndex& index)
{
	selectionModel()->select(index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    emit currentIndexActivated(index);
}

void CyberiadaSMView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QTreeView::currentChanged(current, previous);
    // qDebug() << "current changed" << current.row() << current.column() << (void*)current.internalPointer();
    emit currentIndexActivated(current);
}

void CyberiadaSMView::startDrag(Qt::DropActions)
{
	QDrag* drag = new QDrag(this);
	QModelIndexList indexes;
	QModelIndex current = currentIndex();
	indexes.append(current);
	CyberiadaSMModel* m = static_cast<CyberiadaSMModel*>(model());
	drag->setMimeData(m->mimeData(indexes));
	drag->setPixmap(m->getIndexIcon(current).pixmap(32, 32));
	drag->setDragCursor(QCursor(Qt::ClosedHandCursor).pixmap(), Qt::MoveAction);
	drag->exec(Qt::MoveAction);
}

void CyberiadaSMView::slotModelDataChanged(const QModelIndex& topLeft,
											const QModelIndex&)
{
	//qDebug() << "data changed";
	if (topLeft.isValid()) {
        // CyberiadaSMTreeProxyModel* proxyModel = static_cast<CyberiadaSMTreeProxyModel*>(model());
        // //qDebug() << "update" << topLeft.row() << topLeft.column() << (void*)topLeft.internalPointer();
        // update(proxyModel->mapFromSource(topLeft));
	}
}

