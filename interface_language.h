/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Interface Language: the bundled translations and their installation
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
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

#ifndef INTERFACE_LANGUAGE_H
#define INTERFACE_LANGUAGE_H

#include <QList>
#include <QString>
#include <QStringList>

class QCoreApplication;

// the source language of the interface: no translator is installed for it
#define UI_LANGUAGE_SOURCE "en"

// an interface language (EDIT-UI-1, docs/TRANSLATION.md): its code and its own name
struct UiLanguage {
	QString code;
	QString name;
};

// the source language and every bundled translation, by code
QList<UiLanguage> availableUiLanguages();
// the language a setting stands for: the setting itself when bundled, for an
// empty one the first system UI language bundled, else the source language
QString resolveUiLanguage(const QString& setting, const QStringList& systemLanguages);
// the own name of a language code, the code when unknown
QString uiLanguageName(const QString& code);
// install the translators of the configured language (the GUI only, EDIT-UI-2);
// returns the language installed
QString installUiTranslators(QCoreApplication& app);
// the language installed, the source language when none is
QString currentUiLanguage();

#endif
