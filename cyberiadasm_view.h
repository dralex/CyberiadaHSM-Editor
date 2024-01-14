/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine View
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

#ifndef CYBERIADA_SM_VIEW
#define CYBERIADA_SM_VIEW

#include <QTreeView>

class CyberiadaSMView: public QTreeView {
Q_OBJECT
public:
	CyberiadaSMView(QWidget* parent);

public slots:
	void slotSourceDataChanged(const QModelIndex &topLeft,
							   const QModelIndex &bottomRight);
									
protected slots:
	void currentChanged(const QModelIndex &current, const QModelIndex &previous);

signals:
	void currentIndexActivated(const QModelIndex& current);
	
protected:
	void startDrag(Qt::DropActions);
};

#endif
