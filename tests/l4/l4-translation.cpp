/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process interface translation test (see docs/TESTING.md, L4)
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

#include <QtTest>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include "smeditor_window.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_properties_widget.h"
#include "cyberiadasm_editor_document.h"
#include "dialogs/preferences_dialog.h"
#include "batch_script.h"
#include "settings_manager.h"
#include "interface_language.h"
#include "qtpropertymanager.h"

// the editor creation is protected in the browser
class PropertiesProbe: public CyberiadaSMPropertiesWidget {
public:
	using CyberiadaSMPropertiesWidget::createEditor;
};

// the EDIT-UI laws in the GUI: the bundled languages, the Preferences selector and
// the Russian interface. The English checks run first: a translator stays
// installed till the end of the process
class TestTranslation: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void test_languages();
	void test_resolve();
	void test_preferences();
	void test_english_menus();
	void test_install();
	void test_russian_menus();
	void test_qt_strings();
	void test_property_browser_labels();
	void test_default_names();
	void test_property_lookup();
	void test_plugin_output_tab();
	void test_adopt_settings();

private:
	// the visible menu texts by their place in the menu bar
	static QMap<QString, QString> menuTexts(QMainWindow* window);
	static void collect(const QList<QAction*>& actions, const QString& path, QMap<QString, QString>& texts);

	QTemporaryDir settings;
	QMap<QString, QString> english;
};

void TestTranslation::collect(const QList<QAction*>& actions, const QString& path,
							  QMap<QString, QString>& texts)
{
	for (int i = 0; i < actions.size(); i++) {
		QAction* a = actions.at(i);
		if (a->isSeparator() || a->text().isEmpty()) continue;
		QString place = QString("%1/%2").arg(path).arg(i);
		texts.insert(place, a->text());
		// the code and the markup languages are names, the same in every language
		if (a->menu() && a->menu()->objectName() != "menuLanguage" &&
			a->menu()->objectName() != "menuMarkup") {
			collect(a->menu()->actions(), place, texts);
		}
	}
}

QMap<QString, QString> TestTranslation::menuTexts(QMainWindow* window)
{
	QMap<QString, QString> texts;
	collect(window->menuBar()->actions(), QString(), texts);
	return texts;
}

void TestTranslation::initTestCase()
{
	// the stored language must not reach the profile the other cases share
	QVERIFY(settings.isValid());
	QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings.path());
	SettingsManager::instance().setInterfaceLanguage(QString());
}

void TestTranslation::test_languages()
{
	// English is the source, Russian is bundled; each in its own name
	QList<UiLanguage> languages = availableUiLanguages();
	QVERIFY(languages.size() >= 2);
	QCOMPARE(languages.first().code, QString(UI_LANGUAGE_SOURCE));
	QCOMPARE(languages.first().name, QString("English"));
	bool russian = false;
	for (const UiLanguage& l: languages) {
		if (l.code == "ru") {
			QCOMPARE(l.name, QString::fromUtf8("Русский"));
			russian = true;
		}
	}
	QVERIFY(russian);
}

void TestTranslation::test_resolve()
{
	// the system default takes the first system language bundled (EDIT-UI-1)
	QCOMPARE(resolveUiLanguage(QString(), QStringList() << "ru-RU" << "en-US"), QString("ru"));
	QCOMPARE(resolveUiLanguage(QString(), QStringList() << "de-DE" << "ru"), QString("ru"));
	QCOMPARE(resolveUiLanguage(QString(), QStringList() << "de-DE"), QString(UI_LANGUAGE_SOURCE));
	QCOMPARE(resolveUiLanguage(QString(), QStringList()), QString(UI_LANGUAGE_SOURCE));
	// an explicit choice wins over the system; an unknown one falls back
	QCOMPARE(resolveUiLanguage("en", QStringList() << "ru-RU"), QString(UI_LANGUAGE_SOURCE));
	QCOMPARE(resolveUiLanguage("ru", QStringList() << "en-US"), QString("ru"));
	QCOMPARE(resolveUiLanguage("xx", QStringList() << "ru-RU"), QString(UI_LANGUAGE_SOURCE));
}

void TestTranslation::test_preferences()
{
	SettingsManager& sm = SettingsManager::instance();
	sm.setInterfaceLanguage("ru");
	PreferencesDialog dialog;
	QComboBox* combo = dialog.findChild<QComboBox*>("interfaceLanguageComboBox");
	QLabel* note = dialog.findChild<QLabel*>("interfaceLanguageNote");
	QVERIFY(combo);
	QVERIFY(note);
	// System default, then the languages
	QVERIFY(combo->count() >= 3);
	QCOMPARE(combo->itemData(0).toString(), QString());
	QVERIFY(combo->findData(QString(UI_LANGUAGE_SOURCE)) > 0);
	// the stored choice is shown; it differs from the running English, so the note is up
	QCOMPARE(combo->currentData().toString(), QString("ru"));
	QVERIFY(!note->isHidden());

	// Apply stores the choice, the running language hides the note
	combo->setCurrentIndex(combo->findData(QString(UI_LANGUAGE_SOURCE)));
	QVERIFY(note->isHidden());
	QDialogButtonBox* buttons = dialog.findChild<QDialogButtonBox*>();
	QVERIFY(buttons);
	buttons->button(QDialogButtonBox::Apply)->click();
	QCOMPARE(sm.getInterfaceLanguage(), QString(UI_LANGUAGE_SOURCE));
	QCOMPARE(QSettings().value("interface/language").toString(), QString(UI_LANGUAGE_SOURCE));

	// Reset returns to the system default
	buttons->button(QDialogButtonBox::Reset)->click();
	QCOMPARE(sm.getInterfaceLanguage(), QString());
	QCOMPARE(combo->currentIndex(), 0);
}

void TestTranslation::test_english_menus()
{
	CyberiadaSMEditorWindow window;
	english = menuTexts(&window);
	QVERIFY(!english.isEmpty());
	QCOMPARE(currentUiLanguage(), QString(UI_LANGUAGE_SOURCE));
}

void TestTranslation::test_install()
{
	SettingsManager::instance().setInterfaceLanguage("ru");
	QCOMPARE(installUiTranslators(*qApp), QString("ru"));
	QCOMPARE(currentUiLanguage(), QString("ru"));
}

void TestTranslation::test_russian_menus()
{
	// every menu and action text is translated: an unwrapped or a missing
	// string keeps its English text at the same place
	CyberiadaSMEditorWindow window;
	QMap<QString, QString> russian = menuTexts(&window);
	QCOMPARE(russian.keys(), english.keys());
	QStringList same;
	for (QMap<QString, QString>::const_iterator i = russian.begin(); i != russian.end(); i++) {
		if (i.value() == english.value(i.key())) same << i.value();
	}
	QVERIFY2(same.isEmpty(), qPrintable(same.join(", ")));
	QCOMPARE(window.QMainWindow::menuBar()->actions().first()->text(), QString::fromUtf8("&Файл"));
	QVERIFY(window.windowTitle().contains(QString::fromUtf8("без имени")));
}

void TestTranslation::test_qt_strings()
{
	// the standard buttons come from Qt's own catalogue (EDIT-UI-3)
	QDialogButtonBox box(QDialogButtonBox::Cancel);
	QCOMPARE(box.button(QDialogButtonBox::Cancel)->text(), QString::fromUtf8("Отмена"));
}

void TestTranslation::test_property_browser_labels()
{
	QtBoolPropertyManager bools;
	QtProperty* flag = bools.addProperty("flag");
	QCOMPARE(flag->valueText(), QString::fromUtf8("Нет"));
	QtRectFPropertyManager rects;
	QtProperty* rect = rects.addProperty("rect");
	QStringList names;
	for (QtProperty* p: rect->subProperties()) names << p->propertyName();
	QCOMPARE(names, QStringList() << "X" << "Y" << QString::fromUtf8("Ширина") << QString::fromUtf8("Высота"));
}

void TestTranslation::test_default_names()
{
	// the GUI names the new elements in the interface language (EDIT-UI-3);
	// the batch run of the same script keeps the sources (ui-batch-english)
	CyberiadaSMEditorWindow window;
	QString error;
	QVERIFY2(runEditScript(&window, "scripts/ui-batch-english.script", &error), qPrintable(error));
	CyberiadaSMModel* model = window.currentDocument()->model();
	Cyberiada::ElementList sms = model->rootDocument()->find_elements_by_type(Cyberiada::elementSM);
	QCOMPARE(int(sms.size()), 1);
	QCOMPARE(QString::fromUtf8(sms.front()->get_name().c_str()), QString::fromUtf8("Новая машина состояний"));
	Cyberiada::ElementList states = model->rootDocument()->find_elements_by_type(Cyberiada::elementSimpleState);
	QCOMPARE(int(states.size()), 1);
	QCOMPARE(QString::fromUtf8(states.front()->get_name().c_str()), QString::fromUtf8("Новое состояние"));
}

void TestTranslation::test_property_lookup()
{
	// the panel finds its rows by the label: a rename through the Russian row
	// reaches the model, and a model change refreshes the row
	CyberiadaSMModel model(nullptr);
	CyberiadaSMEditorScene scene(&model);
	PropertiesProbe view;
	view.setModel(&model);
	QVERIFY(model.loadDocument("diagrams/geometry.graphml"));
	scene.loadScene();
	view.slotElementSelected(model.elementToIndex(model.idToElement("node-0-0-1")));
	QtProperty* name = nullptr;
	QtStringPropertyManager* m = nullptr;
	QList<QtProperty*> rows = view.properties();
	while (!rows.isEmpty()) {
		QtProperty* p = rows.takeFirst();
		rows << p->subProperties();
		if (p->propertyName() == QString::fromUtf8("Имя")) {
			name = p;
			m = dynamic_cast<QtStringPropertyManager*>(p->propertyManager());
		}
	}
	QVERIFY(name);
	QVERIFY(m);
	const Cyberiada::Element* element = model.idToElement("node-0-0-1");
	m->setValue(name, QString::fromUtf8("Переименовано"));
	QCOMPARE(QString::fromUtf8(element->get_name().c_str()), QString::fromUtf8("Переименовано"));
	QVERIFY(model.updateTitle(model.elementToIndex(element), "node 0-0-1"));
	QCOMPARE(m->value(name), QString("node 0-0-1"));
}

void TestTranslation::test_plugin_output_tab()
{
	// the title of a plugin output tab (EDIT-PLUG-17)
	QCOMPARE(QCoreApplication::translate("PluginDock", "%1 output").arg("Echo"), QString::fromUtf8("Вывод Echo"));
}

void TestTranslation::test_adopt_settings()
{
	// the pre-release store fills an empty one once and stays as it was (EDIT-UI-5)
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QSettings legacy(dir.filePath("legacy.ini"), QSettings::IniFormat);
	QSettings current(dir.filePath("current.ini"), QSettings::IniFormat);
	QSettings empty(dir.filePath("empty.ini"), QSettings::IniFormat);
	QVERIFY(!SettingsManager::adoptSettings(empty, current));
	QVERIFY(current.allKeys().isEmpty());
	QStringList recent = QStringList() << "a.graphml" << "b.graphml";
	legacy.setValue("interface/language", "ru");
	legacy.setValue("files/recentFiles", recent);
	QVERIFY(SettingsManager::adoptSettings(legacy, current));
	QCOMPARE(current.value("interface/language").toString(), QString("ru"));
	QCOMPARE(current.value("files/recentFiles").toStringList(), recent);
	QCOMPARE(legacy.allKeys().size(), 2);
	// a filled store is never overwritten
	legacy.setValue("interface/language", "en");
	QVERIFY(!SettingsManager::adoptSettings(legacy, current));
	QCOMPARE(current.value("interface/language").toString(), QString("ru"));
}

QTEST_MAIN(TestTranslation)
#include "l4-translation.moc"
