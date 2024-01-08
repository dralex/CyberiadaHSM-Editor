/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The assertion mechanism the State Machine Editor
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

#ifndef MYASSERT_HEADER
#define MYASSERT_HEADER

#include <QString>

void criticalMessage(const QString& msg);

/*#ifndef QT_NO_DEBUG
#define MY_ASSERT(a)	Q_ASSERT(a)
#else */
#define MY_ASSERT(a)	do { if (!(a)) { criticalMessage(QString("ASSERTION FAILED AT %1:%2").arg(__FILE__).arg(__LINE__));  } } while(0)
//#endif

#endif
