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

#include <QDateTime>
#include "main.h"
#include "smeditor_window.h"
#include "cyberiada_constants.h"
#include "settings_manager.h"

int main(int argc, char *argv[])
{
	qsrand(QDateTime::currentDateTime().toTime_t());
	CyberiadaSMEditorApplication app(argc, argv);
    try {
		CyberiadaSMEditorWindow win;
		win.show();
		int res = app.exec();

		return res;
	} catch(const QString& error) {
		app.printMessage(error);
	} catch(...) {
		app.printMessage();
	}
	return 1;
}
