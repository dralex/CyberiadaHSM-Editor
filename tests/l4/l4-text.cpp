/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process text font test (see docs/TESTING.md, L4)
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
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "editable_text_item.h"
#include "settings_manager.h"
#include "fontmanager.h"

// the absolute sizes follow the text engine, so the test states the relations
class TestText: public QObject {
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanupTestCase();
	void test_bundled_font();
	void test_role_font();
	void test_formal_comment_family();
	void test_item_sizes();
	void test_title_size_change();
	void test_title_rewrap();
	void test_size_limits();

private:
	QList<EditableTextItem*> textItems(FontRole role);
	CyberiadaSMModel* model;
	CyberiadaSMEditorScene* scene;
};

QList<EditableTextItem*> TestText::textItems(FontRole role)
{
	QList<EditableTextItem*> result;
	const QList<QGraphicsItem*> items = scene->items();
	for (QList<QGraphicsItem*>::const_iterator i = items.begin(); i != items.end(); i++) {
		EditableTextItem* text = dynamic_cast<EditableTextItem*>(*i);
		if (text && text->getFontRole() == role) result.append(text);
	}
	return result;
}

void TestText::initTestCase()
{
	FontManager::instance().loadBundledFont();
	// the setting is persisted, so the test states its own starting point
	SettingsManager& settings = SettingsManager::instance();
	settings.setFontFamily(QString());
	for (int role = 0; role < fontRolesCount; role++) {
		settings.setFontSize(FontRole(role), FONT_SIZE);
	}

	model = new CyberiadaSMModel(this);
	scene = new CyberiadaSMEditorScene(model, this);
	QVERIFY(model->loadDocument("diagrams/label-geometry.graphml"));
	scene->loadScene();
}

void TestText::cleanupTestCase()
{
	SettingsManager::instance().loadDefaults();
}

void TestText::test_bundled_font()
{
	// the resources are linked into the core, so the font is here as well
	QString family = FontManager::instance().bundledFamily();
	QVERIFY(!family.isEmpty());
	QCOMPARE(family, QString("Cyberiada Mono"));
}

void TestText::test_role_font()
{
	FontManager& fonts = FontManager::instance();
	SettingsManager& settings = SettingsManager::instance();
	settings.setFontSize(fontRoleStateTitle, 20);
	settings.setFontSize(fontRoleStateAction, 14);
	settings.setFontSize(fontRoleTransition, 8);
	settings.setFontSize(fontRoleComment, 16);

	// the point size is converted to pixels in the font manager (96 dpi
	// fixed), so the metrics do not depend on the machine
	QCOMPARE(fonts.font(fontRoleStateTitle).pixelSize(), 27);
	QCOMPARE(fonts.font(fontRoleStateAction).pixelSize(), 19);
	QCOMPARE(fonts.font(fontRoleTransition).pixelSize(), 11);
	QCOMPARE(fonts.font(fontRoleComment).pixelSize(), 21);
	// the formal comment is written in the size of the comment
	QCOMPARE(fonts.font(fontRoleFormalComment).pixelSize(), 21);

	// only the header is bold
	QVERIFY(fonts.font(fontRoleStateTitle).bold());
	QVERIFY(!fonts.font(fontRoleStateAction).bold());
	QVERIFY(!fonts.font(fontRoleTransition).bold());
	QVERIFY(!fonts.font(fontRoleComment).bold());

	settings.setFontSize(fontRoleStateTitle, FONT_SIZE);
	settings.setFontSize(fontRoleStateAction, FONT_SIZE);
	settings.setFontSize(fontRoleTransition, FONT_SIZE);
	settings.setFontSize(fontRoleComment, FONT_SIZE);
}

void TestText::test_formal_comment_family()
{
	FontManager& fonts = FontManager::instance();
	SettingsManager& settings = SettingsManager::instance();
	settings.setFontFamily("Helvetica");

	QCOMPARE(fonts.font(fontRoleComment).family(), QString("Helvetica"));
	// the formal comment keeps the monospace of the standard
	QCOMPARE(fonts.font(fontRoleFormalComment).family(), fonts.bundledFamily());

	settings.setFontFamily(QString());
	QCOMPARE(fonts.font(fontRoleComment).family(), fonts.bundledFamily());
}

void TestText::test_item_sizes()
{
	// every text item is drawn in the font of its role
	QVERIFY(!textItems(fontRoleStateTitle).isEmpty());
	QVERIFY(!textItems(fontRoleStateAction).isEmpty());
	QVERIFY(!textItems(fontRoleTransition).isEmpty());
	QVERIFY(!textItems(fontRoleComment).isEmpty());

	for (int role = 0; role < fontRolesCount; role++) {
		const QList<EditableTextItem*> items = textItems(FontRole(role));
		for (QList<EditableTextItem*>::const_iterator i = items.begin(); i != items.end(); i++) {
			QCOMPARE((*i)->font().pixelSize(),
					 qRound(SettingsManager::instance().getFontSize(FontRole(role)) * 96.0 / 72.0));
		}
	}
}

void TestText::test_title_size_change()
{
	EditableTextItem* title = textItems(fontRoleStateTitle).first();
	EditableTextItem* action = textItems(fontRoleStateAction).first();
	double title_height = title->boundingRect().height();
	double action_height = action->boundingRect().height();

	SettingsManager::instance().setFontSize(fontRoleStateTitle, FONT_SIZE * 2);

	// the header follows its own size and leaves the other roles alone
	QCOMPARE(title->font().pixelSize(), FONT_SIZE * 2 * 96 / 72);
	QVERIFY(title->boundingRect().height() > title_height);
	QCOMPARE(action->font().pixelSize(), FONT_SIZE * 96 / 72);
	QCOMPARE(action->boundingRect().height(), action_height);

	SettingsManager::instance().setFontSize(fontRoleStateTitle, FONT_SIZE);
	QCOMPARE(title->boundingRect().height(), title_height);
}

void TestText::test_title_rewrap()
{
	// the header is bold: the boldness used to stop the width update
	EditableTextItem* title = textItems(fontRoleStateTitle).first();
	CyberiadaSMEditorAbstractItem* state =
		dynamic_cast<CyberiadaSMEditorAbstractItem*>(title->parentItem());
	QVERIFY(state);
	QVERIFY(state->hasGeometry());

	SettingsManager::instance().setFontSize(fontRoleStateTitle, FONT_SIZE * 2);
	QCOMPARE(title->textWidth(), state->boundingRect().width());
	SettingsManager::instance().setFontSize(fontRoleStateTitle, FONT_SIZE);
	QCOMPARE(title->textWidth(), state->boundingRect().width());
}

void TestText::test_size_limits()
{
	SettingsManager& settings = SettingsManager::instance();
	settings.setFontSize(fontRoleComment, FONT_SIZE_MIN - 1);
	QCOMPARE(settings.getFontSize(fontRoleComment), FONT_SIZE);
	settings.setFontSize(fontRoleComment, FONT_SIZE_MAX + 1);
	QCOMPARE(settings.getFontSize(fontRoleComment), FONT_SIZE);
	settings.setFontSize(fontRoleComment, FONT_SIZE_MAX);
	QCOMPARE(settings.getFontSize(fontRoleComment), FONT_SIZE_MAX);
	settings.setFontSize(fontRoleComment, FONT_SIZE);
}

QTEST_MAIN(TestText)
#include "l4-text.moc"
