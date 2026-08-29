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
#include <QComboBox>
#include <QGridLayout>
#include "cyberiadasm_model.h"
#include "dialogs/open_file_dialog.h"
#include "dialogs/save_file_dialog.h"
#include "dialogs/export_image_dialog.h"

class TestDialog: public QObject {
	Q_OBJECT

private slots:
	void test_options_injected();
	void test_default_options();
	void test_selected_file();
	void test_save_formats();
	void test_save_options();
	void test_save_refused_format();
	void test_export_image();
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
	QVERIFY(dlg.findChild<QCheckBox*>("reconstructSMCheckBox"));
	QVERIFY(dlg.findChild<QCheckBox*>("strictCheckBox"));
}

void TestDialog::test_default_options()
{
	// the document is inspected until the user asks for the editing, and the
	// inspected document is never given the geometry it does not have
	OpenFileDialog dlg;
	QCheckBox* inspector = dlg.findChild<QCheckBox*>("inspectorCheckBox");
	QCheckBox* reconstruct = dlg.findChild<QCheckBox*>("reconstructCheckBox");
	QCheckBox* reconstructSM = dlg.findChild<QCheckBox*>("reconstructSMCheckBox");
	QVERIFY(inspector && reconstruct && reconstructSM);
	QVERIFY(dlg.inspectorModeEnabled());
	QVERIFY(!dlg.reconstructionEnabled());
	QVERIFY(!reconstruct->isEnabled());
	QVERIFY(!dlg.reconstructionSMEnabled());
	QVERIFY(!reconstructSM->isEnabled());
	QVERIFY(!dlg.strictModeEnabled());

	inspector->setChecked(false);
	QVERIFY(!dlg.inspectorModeEnabled());
	QVERIFY(reconstruct->isEnabled());
	// the border is created only within the reconstruction
	QVERIFY(!reconstructSM->isEnabled());
	reconstruct->setChecked(true);
	QVERIFY(dlg.reconstructionEnabled());
	QVERIFY(reconstructSM->isEnabled());
	reconstructSM->setChecked(true);
	QVERIFY(dlg.reconstructionSMEnabled());

	reconstruct->setChecked(false);
	QVERIFY(!dlg.reconstructionSMEnabled());
	QVERIFY(!reconstructSM->isEnabled());
	reconstruct->setChecked(true);
	reconstructSM->setChecked(true);

	inspector->setChecked(true);
	QVERIFY(!dlg.reconstructionEnabled());
	QVERIFY(!reconstruct->isEnabled());
	QVERIFY(!dlg.reconstructionSMEnabled());
	QVERIFY(!reconstructSM->isEnabled());
}

void TestDialog::test_selected_file()
{
	OpenFileDialog dlg;
	dlg.selectFile("diagrams/hierarchy.graphml");
	QVERIFY(dlg.selectedFile().endsWith("hierarchy.graphml"));
}

void TestDialog::test_save_formats()
{
	// every writable format is offered for a document the library can express
	CyberiadaSMModel model(this);
	QVERIFY(model.loadDocument("diagrams/geometry.graphml"));
	SaveFileDialog dlg(NULL, model.rootDocument());
	QComboBox* formats = dlg.findChild<QComboBox*>("formatComboBox");
	QVERIFY(formats);
	QCOMPARE(formats->count(), 3);
	QCOMPARE(dlg.selectedFormat(), Cyberiada::formatCyberiada10);
	QCOMPARE(dlg.acceptMode(), QFileDialog::AcceptSave);

	formats->setCurrentIndex(1);
	QCOMPARE(dlg.selectedFormat(), Cyberiada::formatLegacyYEDOstranna);
	formats->setCurrentIndex(2);
	QCOMPARE(dlg.selectedFormat(), Cyberiada::formatLegacyYEDBerloga16);
}

void TestDialog::test_save_options()
{
	// the yEd formats require the geometry, and the library allows no other
	// option beside the skipped one
	CyberiadaSMModel model(this);
	QVERIFY(model.loadDocument("diagrams/geometry.graphml"));
	SaveFileDialog dlg(NULL, model.rootDocument());
	QComboBox* formats = dlg.findChild<QComboBox*>("formatComboBox");
	QCheckBox* skip = dlg.findChild<QCheckBox*>("skipGeometryCheckBox");
	QCheckBox* round = dlg.findChild<QCheckBox*>("roundCheckBox");
	QVERIFY(formats && skip && round);

	QVERIFY(skip->isEnabled());
	skip->setChecked(true);
	QVERIFY(dlg.skipGeometryEnabled());
	QVERIFY(!round->isEnabled());
	QVERIFY(!dlg.roundEnabled() || !round->isEnabled());

	skip->setChecked(false);
	QVERIFY(round->isEnabled());
	formats->setCurrentIndex(1);
	QVERIFY(!skip->isEnabled());
	QVERIFY(!dlg.skipGeometryEnabled());
}

void TestDialog::test_save_refused_format()
{
	// the yEd formats keep a single state machine only
	CyberiadaSMModel model(this);
	QVERIFY(model.loadDocument("diagrams/two-sms.graphml"));
	SaveFileDialog dlg(NULL, model.rootDocument());
	QComboBox* formats = dlg.findChild<QComboBox*>("formatComboBox");
	QVERIFY(formats);
	QCOMPARE(formats->count(), 3);
	// the format is shown with the reason but cannot be chosen
	QVERIFY(!formats->model()->flags(formats->model()->index(1, 0)).testFlag(Qt::ItemIsEnabled));
	QVERIFY(!formats->model()->flags(formats->model()->index(2, 0)).testFlag(Qt::ItemIsEnabled));
	QCOMPARE(dlg.selectedFormat(), Cyberiada::formatCyberiada10);
}

void TestDialog::test_export_image()
{
	// the suffix follows the selected image format
	ExportImageDialog dlg;
	QCOMPARE(dlg.acceptMode(), QFileDialog::AcceptSave);
	QCOMPARE(dlg.defaultSuffix(), QString("png"));
	dlg.selectNameFilter("JPEG (*.jpg *.jpeg)");
	dlg.updateSuffix();
	QCOMPARE(dlg.defaultSuffix(), QString("jpg"));
	dlg.selectNameFilter("TIFF (*.tiff)");
	dlg.updateSuffix();
	QCOMPARE(dlg.defaultSuffix(), QString("tiff"));
	// the vector formats are offered beside the raster ones
	dlg.selectNameFilter("SVG (*.svg)");
	dlg.updateSuffix();
	QCOMPARE(dlg.defaultSuffix(), QString("svg"));
	dlg.selectNameFilter("PDF (*.pdf)");
	dlg.updateSuffix();
	QCOMPARE(dlg.defaultSuffix(), QString("pdf"));
}

QTEST_MAIN(TestDialog)
#include "l4-dialog.moc"
