/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
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
#include <QFontComboBox>
#include <QGridLayout>
#include <QFileInfo>
#include "cyberiadasm_model.h"
#include "dialogs/open_file_dialog.h"
#include "dialogs/save_file_dialog.h"
#include "dialogs/export_image_dialog.h"
#include "dialogs/stateactiondialog.h"
#include "dialogs/about_dialog.h"
#include "dialogs/fragmentdialog.h"
#include <QPushButton>
#include "version.h"
#include <QPlainTextEdit>
#include <QLabel>

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
	void test_export_default_name();
	void test_action_dialog();
	void test_transition_dialog();
	void test_about_dialog();
	void test_fragment_dialog();
	void test_fragment_dialog_transition();
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
	// documents open in edit mode by default (inspection is opt-in); an
	// inspected document is never given the geometry it does not have
	OpenFileDialog dlg;
	QCheckBox* inspector = dlg.findChild<QCheckBox*>("inspectorCheckBox");
	QCheckBox* reconstruct = dlg.findChild<QCheckBox*>("reconstructCheckBox");
	QCheckBox* reconstructSM = dlg.findChild<QCheckBox*>("reconstructSMCheckBox");
	QVERIFY(inspector && reconstruct && reconstructSM);
	QVERIFY(!dlg.inspectorModeEnabled());
	QVERIFY(reconstruct->isEnabled());
	QVERIFY(!dlg.reconstructionEnabled());
	QVERIFY(!dlg.reconstructionSMEnabled());
	QVERIFY(!reconstructSM->isEnabled());
	QVERIFY(!dlg.strictModeEnabled());

	// turning inspection on disables reconstruction
	inspector->setChecked(true);
	QVERIFY(dlg.inspectorModeEnabled());
	QVERIFY(!reconstruct->isEnabled());

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
	QCOMPARE(formats->count(), 4);
	QCOMPARE(dlg.selectedFormat(), Cyberiada::formatCyberiada10);
	QCOMPARE(dlg.acceptMode(), QFileDialog::AcceptSave);

	formats->setCurrentIndex(1);
	QCOMPARE(dlg.selectedFormat(), Cyberiada::formatLegacyYEDOstranna);
	formats->setCurrentIndex(2);
	QCOMPARE(dlg.selectedFormat(), Cyberiada::formatLegacyYEDBerloga16);
	// the Lapki format needs the platform name
	QVERIFY(!formats->itemData(3, Qt::UserRole + 1).toString().isEmpty());

	CyberiadaSMModel platform_model(this);
	QVERIFY(platform_model.loadDocument("diagrams/meta.graphml"));
	SaveFileDialog lapki(NULL, platform_model.rootDocument());
	QComboBox* lapki_formats = lapki.findChild<QComboBox*>("formatComboBox");
	QVERIFY(lapki_formats);
	QVERIFY(lapki_formats->itemData(3, Qt::UserRole + 1).toString().isEmpty());
	lapki_formats->setCurrentIndex(3);
	QCOMPARE(lapki.selectedFormat(), Cyberiada::formatLapki);
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
	QCOMPARE(formats->count(), 4);
	// the format is shown with the reason but cannot be chosen
	QVERIFY(!formats->model()->flags(formats->model()->index(1, 0)).testFlag(Qt::ItemIsEnabled));
	QVERIFY(!formats->model()->flags(formats->model()->index(2, 0)).testFlag(Qt::ItemIsEnabled));
	// no platform name for the Lapki format
	QVERIFY(!formats->model()->flags(formats->model()->index(3, 0)).testFlag(Qt::ItemIsEnabled));
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

	// the export font is chosen from the monospace families and reported (#4)
	QFontComboBox* fontCombo = dlg.findChild<QFontComboBox*>("fontComboBox");
	QVERIFY(fontCombo);
	QCOMPARE(fontCombo->fontFilters(), QFontComboBox::FontFilters(QFontComboBox::MonospacedFonts));
	QVERIFY(!dlg.fontFamily().isEmpty());
	QCOMPARE(dlg.fontFamily(), fontCombo->currentFont().family());
}

void TestDialog::test_export_default_name()
{
	// export proposes the diagram's base name (window slotFileExport): the dialog
	// keeps it and its default suffix supplies the extension
	ExportImageDialog dlg;
	QString base = QFileInfo("diagrams/geometry.graphml").completeBaseName();
	QCOMPARE(base, QString("geometry"));
	dlg.selectFile(base);
	QCOMPARE(QFileInfo(dlg.selectedFile()).completeBaseName(), QString("geometry"));
}

QTEST_MAIN(TestDialog)
void TestDialog::test_action_dialog()
{
	StateActionDialog dialog("entry");
	QPlainTextEdit* edit = dialog.findChild<QPlainTextEdit*>();
	QVERIFY(edit);
	// the keyword is pre-filled; the behaviour follows on the same line or,
	// as in the document format, on the next one
	QCOMPARE(edit->toPlainText(), QString("entry/"));
	QVERIFY(dialog.parseInput());
	QCOMPARE(dialog.getBehaviour(), QString(""));

	edit->setPlainText("entry/ foo();");
	QVERIFY(dialog.parseInput());
	QCOMPARE(dialog.getBehaviour(), QString("foo();"));

	edit->setPlainText("entry/\nfoo();\nbar();");
	QVERIFY(dialog.parseInput());
	QCOMPARE(dialog.getBehaviour(), QString("foo();\nbar();"));

	// a damaged keyword is refused
	edit->setPlainText("entrance/ foo();");
	QVERIFY(!dialog.parseInput());
}

void TestDialog::test_transition_dialog()
{
	// the internal-transition mode splits "EVENT [guard] / behaviour" into parts
	StateActionDialog dialog(StateActionDialog::Mode::Transition);
	QPlainTextEdit* edit = dialog.findChild<QPlainTextEdit*>();
	QVERIFY(edit);
	QCOMPARE(edit->toPlainText(), QString(""));   // no keyword pre-fill

	edit->setPlainText("TICK [x > 0] / count()");
	QVERIFY(dialog.parseInput());
	QCOMPARE(dialog.getTrigger(), QString("TICK"));
	QCOMPARE(dialog.getGuard(), QString("x > 0"));
	QCOMPARE(dialog.getBehaviour(), QString("count()"));

	// the guard and the behaviour are optional, the event is not
	edit->setPlainText("TICK");
	QVERIFY(dialog.parseInput());
	QCOMPARE(dialog.getTrigger(), QString("TICK"));
	QVERIFY(dialog.getGuard().isEmpty());
	QVERIFY(dialog.getBehaviour().isEmpty());

	// no event: refused
	edit->setPlainText("[x > 0] / count()");
	QVERIFY(!dialog.parseInput());
}

void TestDialog::test_about_dialog()
{
	AboutDialog dialog;
	// the logo resource is bundled and loads
	QLabel* logo = nullptr;
	QString allText;
	// QLabel::pixmap() returns a const QPixmap* on Qt 5.12 (the minimum)
	for (QLabel* l : dialog.findChildren<QLabel*>()) {
		if (l->pixmap() && !l->pixmap()->isNull()) logo = l;
		allText += l->text();
	}
	QVERIFY(logo);
	// the box names the application and shows the version and revision
	QVERIFY(allText.contains(CYBERIADA_APP_NAME));
	QVERIFY(allText.contains(CYBERIADA_VERSION));
	QVERIFY(allText.contains(CYBERIADA_REVISION));
	QVERIFY(allText.contains("fedoseev.net"));
}

#include "l4-dialog.moc"

// select [from, from + length) in a box as a drag would
static void selectIn(QPlainTextEdit* box, int from, int length)
{
	QTextCursor c = box->textCursor();
	c.setPosition(from);
	c.setPosition(from + length, QTextCursor::KeepAnchor);
	box->setTextCursor(c);
}

void TestDialog::test_fragment_dialog()
{
	// one read-only box per field in the search order; a selection in one box
	// fills the fragment and the type, the underlined occurrence is marked, a
	// fragment outside one field is refused (EDIT-TOOL-16)
	CyberiadaSMModel model(this);
	QVERIFY(model.loadDocument("diagrams/geometry.graphml"));
	Cyberiada::Element* state = model.idToElement("node-0-0-2");
	QModelIndex si = model.elementToIndex(state);
	QVERIFY(model.updateTitle(si, "Moving"));
	QVERIFY(model.newAction(si, Cyberiada::actionEntry, QString(), QString(), "motor.on()"));
	QVERIFY(model.newAction(si, Cyberiada::actionTransition, "TIMEOUT", "count > 3", "stop()"));
	QVERIFY(model.newAction(si, Cyberiada::actionExit, QString(), QString(), "stop(); log()"));

	FragmentDialog dlg(state, Cyberiada::commentSubjectElement, QString());
	QCOMPARE(dlg.fieldCount(), 6);
	QCOMPARE(dlg.field(0).kind, SubjectFragment::fieldName);
	QCOMPARE(dlg.fieldBox(0)->toPlainText(), QString("Moving"));
	QCOMPARE(dlg.field(1).kind, SubjectFragment::fieldBehaviour);
	QCOMPARE(dlg.field(2).kind, SubjectFragment::fieldTrigger);
	QCOMPARE(dlg.field(3).kind, SubjectFragment::fieldGuard);
	QCOMPARE(dlg.field(4).kind, SubjectFragment::fieldBehaviour);
	QCOMPARE(dlg.field(5).kind, SubjectFragment::fieldBehaviour);
	for (int i = 0; i < dlg.fieldCount(); i++) QVERIFY(dlg.fieldBox(i)->isReadOnly());
	// an element subject starts empty
	QVERIFY(!dlg.acceptable());
	QCOMPARE(dlg.subjectType(), Cyberiada::commentSubjectData);

	// a guard selection gives a data fragment, marked in its own box
	selectIn(dlg.fieldBox(3), 0, 5);
	QCOMPARE(dlg.fragment(), QString("count"));
	QCOMPARE(dlg.subjectType(), Cyberiada::commentSubjectData);
	QVERIFY(dlg.acceptable());
	QCOMPARE(dlg.markedBox(), 3);
	// the name box gives a name fragment and clears the other selection
	selectIn(dlg.fieldBox(0), 0, 3);
	QCOMPARE(dlg.fragment(), QString("Mov"));
	QCOMPARE(dlg.subjectType(), Cyberiada::commentSubjectName);
	QVERIFY(!dlg.fieldBox(3)->textCursor().hasSelection());
	QCOMPARE(dlg.markedBox(), 0);
	// a later occurrence: the first one is marked and the note says so
	selectIn(dlg.fieldBox(5), 0, 6);
	QCOMPARE(dlg.fragment(), QString("stop()"));
	QCOMPARE(dlg.markedBox(), 4);
	QVERIFY(dlg.acceptable());
	QVERIFY(dlg.note().contains("first occurrence"));
	// a typed fragment spanning two fields is refused
	dlg.fragmentEditor()->setPlainText("TIMEOUT [count");
	QVERIFY(!dlg.acceptable());
	QCOMPARE(dlg.markedBox(), -1);
	dlg.fragmentEditor()->setPlainText("log()");
	QVERIFY(dlg.acceptable());
	QCOMPARE(dlg.markedBox(), 5);

	// an existing fragment comes selected where it is underlined
	FragmentDialog again(state, Cyberiada::commentSubjectData, "count > 3");
	QCOMPARE(again.markedBox(), 3);
	QVERIFY(again.fieldBox(3)->textCursor().hasSelection());
	QCOMPARE(again.fragment(), QString("count > 3"));
	QVERIFY(again.acceptable());
}

void TestDialog::test_fragment_dialog_transition()
{
	// a transition shows its label fields; a selection keeps the kind
	CyberiadaSMModel model(this);
	QVERIFY(model.loadDocument("diagrams/comment-transition.graphml"));
	Cyberiada::Element* t = model.idToElement("t0");
	// a transition has no name (CGML-8.5-4): the dialog offers only a data fragment
	FragmentDialog dlg(t, Cyberiada::commentSubjectName, "EVENT");
	QCOMPARE(dlg.fieldCount(), 2);
	QCOMPARE(dlg.field(0).kind, SubjectFragment::fieldTrigger);
	QCOMPARE(dlg.field(1).kind, SubjectFragment::fieldBehaviour);
	QCOMPARE(dlg.subjectType(), Cyberiada::commentSubjectData);
	selectIn(dlg.fieldBox(1), 0, 4);
	QCOMPARE(dlg.fragment(), QString("go()"));
	QCOMPARE(dlg.subjectType(), Cyberiada::commentSubjectData);
	QVERIFY(dlg.acceptable());
}

