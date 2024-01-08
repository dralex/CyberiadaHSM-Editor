/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Application
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

#ifndef CYBERIADA_SM_APPLICATION
#define CYBERIADA_SM_APPLICATION

#include <QApplication>
#include <QMessageBox>

class CyberiadaSMEditorApplication: public QApplication {
Q_OBJECT
public:
	CyberiadaSMEditorApplication(int & argc, char ** argv):
		QApplication(argc, argv) 
		{}

	virtual bool notify(QObject * receiver, QEvent * e) {
		try {
			return QApplication::notify(receiver, e);
		} catch(const QString& error) {
			printMessage(error);
			quit();
		} catch(...) {
			printMessage();
			quit();
		}
		return false;
	}
	
	void printMessage(const QString& msg = "") {
		if(!msg.isEmpty()) {
			QMessageBox::critical(0,
								  "Cyberiada State Machine Editor",
								  QString("Error while running the program: %1").arg(msg));	
		} else {
			QMessageBox::critical(0,
								  "Cyberiada State Machine Editor",
								  "Crytical error");
		}
	}
};

#endif
