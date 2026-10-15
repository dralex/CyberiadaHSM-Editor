/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
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

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include "interface_language.h"
#include "settings_manager.h"

// the compiled translations, see CMakeLists.txt
#define UI_TRANSLATIONS_PATH  ":/i18n"
#define UI_TRANSLATION_DOMAIN "cyberiada-pro"

static QString installedLanguage = UI_LANGUAGE_SOURCE;

// the resources of a static library are registered by hand
static void initTranslationResources()
{
	static bool done = false;
	if (done) return;
	Q_INIT_RESOURCE(i18n);
	done = true;
}

QList<UiLanguage> availableUiLanguages()
{
	initTranslationResources();
	QList<UiLanguage> languages;
	languages.append(UiLanguage{UI_LANGUAGE_SOURCE, uiLanguageName(UI_LANGUAGE_SOURCE)});
	QStringList files = QDir(UI_TRANSLATIONS_PATH).entryList(QStringList() << UI_TRANSLATION_DOMAIN "_*.qm",
															  QDir::Files, QDir::Name);
	for (const QString& file: files) {
		QString code = QFileInfo(file).completeBaseName().section('_', 1);
		languages.append(UiLanguage{code, uiLanguageName(code)});
	}
	return languages;
}

QString uiLanguageName(const QString& code)
{
	// the native name of English is "American English"
	if (code == UI_LANGUAGE_SOURCE) return QString("English");
	QLocale locale(code);
	QString name = locale.language() == QLocale::C ? QString() : locale.nativeLanguageName();
	if (name.isEmpty()) return code;
	return name.left(1).toUpper() + name.mid(1);
}

QString resolveUiLanguage(const QString& setting, const QStringList& systemLanguages)
{
	QStringList codes;
	for (const UiLanguage& l: availableUiLanguages()) codes.append(l.code);
	if (!setting.isEmpty()) return codes.contains(setting) ? setting : QString(UI_LANGUAGE_SOURCE);
	// the system list is in the order of preference, as "ru-RU"
	for (QString language: systemLanguages) {
		language.replace('-', '_');
		if (codes.contains(language)) return language;
		QString base = language.section('_', 0, 0);
		if (codes.contains(base)) return base;
	}
	return UI_LANGUAGE_SOURCE;
}

static bool installTranslator(QCoreApplication& app, const QString& name, const QString& dir)
{
	QTranslator* translator = new QTranslator(&app);
	if (!translator->load(name, dir)) {
		delete translator;
		return false;
	}
	app.installTranslator(translator);
	return true;
}

QString installUiTranslators(QCoreApplication& app)
{
	QString language = resolveUiLanguage(SettingsManager::instance().getInterfaceLanguage(),
										 QLocale::system().uiLanguages());
	if (language == UI_LANGUAGE_SOURCE) return language;
	initTranslationResources();
	// the catalogue of the running Qt matches its strings best; the bundled copy
	// serves the bundles that carry none
	QString qtbase = "qtbase_" + language;
	if (!installTranslator(app, qtbase, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
		installTranslator(app, qtbase, UI_TRANSLATIONS_PATH);
	}
	installTranslator(app, "qtpropertybrowser_" + language, UI_TRANSLATIONS_PATH);
	installTranslator(app, UI_TRANSLATION_DOMAIN "_" + language, UI_TRANSLATIONS_PATH);
	installedLanguage = language;
	return language;
}

QString currentUiLanguage()
{
	return installedLanguage;
}
