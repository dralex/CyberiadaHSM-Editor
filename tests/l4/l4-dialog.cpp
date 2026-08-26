/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process open dialog test (see docs/TESTING.md, L4)
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
#include <QCheckBox>
#include <QGridLayout>
#include "dialogs/open_file_dialog.h"

class TestDialog: public QObject {
	Q_OBJECT

private slots:
	void test_options_injected();
	void test_default_options();
	void test_selected_file();
};

void TestDialog::test_options_injected()
{
	// the options share the single window with the file browser
	OpenFileDialog dlg;
	QVERIFY(qobject_cast<QGridLayout*>(dlg.layout()));
	QCOMPARE(dlg.fileMode(), QFileDialog::ExistingFile);
	QVERIFY(dlg.testOption(QFileDialog::DontUseNativeDialog));
	QVERIFY(dlg.findChild<QCheckBox*>("inspectorCheckBox"));
	QVERIFY(dlg.findChild<QCheckBox*>("reconstructCheckBox"));
	QVERIFY(dlg.findChild<QCheckBox*>("strictCheckBox"));
}

void TestDialog::test_default_options()
{
	// the document is inspected until the user asks for the editing, and the
	// inspected document is never given the geometry it does not have
	OpenFileDialog dlg;
	QCheckBox* inspector = dlg.findChild<QCheckBox*>("inspectorCheckBox");
	QCheckBox* reconstruct = dlg.findChild<QCheckBox*>("reconstructCheckBox");
	QVERIFY(inspector && reconstruct);
	QVERIFY(dlg.inspectorModeEnabled());
	QVERIFY(!dlg.reconstructionEnabled());
	QVERIFY(!reconstruct->isEnabled());
	QVERIFY(!dlg.strictModeEnabled());

	inspector->setChecked(false);
	QVERIFY(!dlg.inspectorModeEnabled());
	QVERIFY(reconstruct->isEnabled());
	reconstruct->setChecked(true);
	QVERIFY(dlg.reconstructionEnabled());

	inspector->setChecked(true);
	QVERIFY(!dlg.reconstructionEnabled());
	QVERIFY(!reconstruct->isEnabled());
}

void TestDialog::test_selected_file()
{
	OpenFileDialog dlg;
	dlg.selectFile("diagrams/hierarchy.graphml");
	QVERIFY(dlg.selectedFile().endsWith("hierarchy.graphml"));
}

QTEST_MAIN(TestDialog)
#include "l4-dialog.moc"
