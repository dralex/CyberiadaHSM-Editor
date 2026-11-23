/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The in-process tests of the disk check of the open tabs
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
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QUndoStack>
#include "smeditor_window.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_document.h"
#include "comparison_document.h"
#include "code_editor.h"
#include "disk_bar.h"
#include "disk_state.h"
#include "text_document.h"

// the disk check (EDIT-DOC-13..18, docs/GIT.md): a file changed under an open tab by
// another program — a clean tab reads it again, an edited one gets the bar
class TestDisk: public QObject {
	Q_OBJECT

private slots:
	void init();
	void cleanup();
	void test_state();
	void test_clean_reload();
	void test_touched();
	void test_edited_bar();
	void test_answer_reload();
	void test_answer_compare();
	void test_absent();
	void test_absent_close();
	void test_unreadable();
	void test_save_guard();
	void test_bar_follows_tab();
	void test_text_reload();
	void test_watcher();

private:
	// a copy of a test diagram in the temporary folder, open in a tab
	CyberiadaSMEditorDocument* openCopy(const QString& source, const QString& name);
	// the bytes of a test diagram written over the file, as another program does
	void writeOver(const QString& path, const QString& source);
	static int stateCount(CyberiadaSMModel* model);
	// the button of the pending prompt clicked once it is up
	void answerPrompt(const QString& text);

	CyberiadaSMEditorWindow* window = nullptr;
	QTemporaryDir* dir = nullptr;
};

void TestDisk::init()
{
	dir = new QTemporaryDir();
	QVERIFY(dir->isValid());
	window = new CyberiadaSMEditorWindow();
	window->resize(1200, 800);
	window->show();
	QVERIFY(QTest::qWaitForWindowExposed(window));
}

void TestDisk::cleanup()
{
	// the prompts of the closing window are not asked: the tabs go as they are
	while (window->documentCount() > 1) window->closeDocument(window->tabAt(0), true);
	delete window;
	window = nullptr;
	delete dir;
	dir = nullptr;
}

CyberiadaSMEditorDocument* TestDisk::openCopy(const QString& source, const QString& name)
{
	QString path = dir->filePath(name);
	QFile::remove(path);
	if (!QFile::copy(source, path)) return nullptr;
	QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
	QString error;
	if (!window->openFile(path, &error)) return nullptr;
	return window->currentDocument();
}

void TestDisk::writeOver(const QString& path, const QString& source)
{
	QFile in(source);
	QVERIFY(in.open(QIODevice::ReadOnly));
	QByteArray bytes = in.readAll();
	QFile out(path);
	QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
	QCOMPARE(out.write(bytes), qint64(bytes.size()));
	out.close();
}

int TestDisk::stateCount(CyberiadaSMModel* model)
{
	if (!model->rootDocument()) return 0;
	return int(model->rootDocument()->find_elements_by_type(Cyberiada::elementSimpleState).size() +
	           model->rootDocument()->find_elements_by_type(Cyberiada::elementCompositeState).size());
}

void TestDisk::answerPrompt(const QString& text)
{
	QTimer::singleShot(50, [text]() {
		QMessageBox* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
		if (!box) return;
		for (QAbstractButton* button : box->buttons()) {
			if (button->text().remove('&') == text) {
				button->click();
				return;
			}
		}
	});
}

// the remembered state against the file: the same, touched, changed, gone
void TestDisk::test_state()
{
	QString path = dir->filePath("state.txt");
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.write("one");
	file.close();
	DiskState remembered = DiskState::read(path);
	QVERIFY(remembered.exists);
	QCOMPARE(remembered.size, qint64(3));
	QVERIFY(!remembered.digest.isEmpty());
	DiskState now;
	// read at once after the write, the state is too close to the file's time: the bytes
	// are read again and taken
	QCOMPARE(checkDiskState(remembered, path, &now), DiskCheck::touched);
	QCOMPARE(now.digest, remembered.digest);
	// a state read long after the file's time ends the check at the size and the time
	DiskState settled = remembered;
	settled.checked = remembered.modified.addSecs(60);
	QCOMPARE(checkDiskState(settled, path, &now), DiskCheck::same);

	QVERIFY(file.open(QIODevice::ReadWrite));
	QVERIFY(file.setFileTime(remembered.modified.addSecs(10), QFileDevice::FileModificationTime));
	file.close();
	QCOMPARE(checkDiskState(remembered, path, &now), DiskCheck::touched);
	QCOMPARE(now.digest, remembered.digest);

	QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
	file.write("two");
	file.close();
	QCOMPARE(checkDiskState(remembered, path, &now), DiskCheck::changed);
	QVERIFY(now.digest != remembered.digest);

	QVERIFY(QFile::remove(path));
	QCOMPARE(checkDiskState(remembered, path, &now), DiskCheck::gone);
	QVERIFY(!now.exists);
}

// EDIT-DOC-14: a tab without edits reads its changed file again, its history emptied
void TestDisk::test_clean_reload()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	int before = stateCount(doc->model());
	QVERIFY(before > 0);
	writeOver(doc->filePath(), "diagrams/geometry.graphml");
	window->checkDisk();
	QVERIFY(stateCount(doc->model()) != before);
	QCOMPARE(doc->diskStatus(), EditorDocument::diskOk);
	QVERIFY(doc->isClean());
	QCOMPARE(doc->undoStack()->count(), 0);
	QVERIFY(window->diskBar()->isHidden());
	// the tree shows the document read again
	QVERIFY(window->getModel() == doc->model());
}

// EDIT-DOC-13: a touch, the same bytes at another time, reads nothing
void TestDisk::test_touched()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	const Cyberiada::LocalDocument* root = doc->model()->rootDocument();
	QFile file(doc->filePath());
	QVERIFY(file.open(QIODevice::ReadWrite));
	QDateTime later = doc->diskState().modified.addSecs(10);
	QVERIFY(file.setFileTime(later, QFileDevice::FileModificationTime));
	file.close();
	window->checkDisk();
	QVERIFY(doc->model()->rootDocument() == root);
	QCOMPARE(doc->diskState().modified, later);
	QCOMPARE(doc->diskStatus(), EditorDocument::diskOk);
}

// EDIT-DOC-15: an edited tab keeps its edits under the bar; Keep my edits takes the file
void TestDisk::test_edited_bar()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	CyberiadaSMModel* m = doc->model();
	QVERIFY(m->updateTitle(m->elementToIndex(m->idToElement("n0")), "Edited"));
	int steps = doc->undoStack()->count();
	writeOver(doc->filePath(), "diagrams/geometry.graphml");
	window->checkDisk();
	QCOMPARE(doc->diskStatus(), EditorDocument::diskChanged);
	QVERIFY(doc->diskBarShown());
	QVERIFY(!window->diskBar()->isHidden());
	QCOMPARE(window->diskBar()->text(), QString("h.graphml changed on disk"));
	QVERIFY(!window->diskBar()->reloadButton()->isHidden());
	QVERIFY(!window->diskBar()->compareButton()->isHidden());
	QVERIFY(window->diskBar()->closeButton()->isHidden());
	QCOMPARE(doc->undoStack()->count(), steps);
	QCOMPARE(QString::fromStdString(m->idToElement("n0")->get_name()), QString("Edited"));
	// a second check keeps the bar and the edits
	window->checkDisk();
	QCOMPARE(doc->diskStatus(), EditorDocument::diskChanged);

	QString error;
	QVERIFY(window->answerDiskBar("keep", &error));
	QCOMPARE(doc->diskStatus(), EditorDocument::diskOk);
	QVERIFY(window->diskBar()->isHidden());
	QCOMPARE(doc->undoStack()->count(), steps);
	// the file as it is now is the remembered one: a Save asks nothing (EDIT-DOC-17)
	QVERIFY(window->confirmDiskOverwrite(doc, &error));
	// an answer the bar does not offer is refused
	QVERIFY(!window->answerDiskBar("keep", &error));
	QVERIFY(!error.isEmpty());
}

// EDIT-DOC-15: Reload drops the edits and reads the file
void TestDisk::test_answer_reload()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	CyberiadaSMModel* m = doc->model();
	QVERIFY(m->updateTitle(m->elementToIndex(m->idToElement("n0")), "Edited"));
	writeOver(doc->filePath(), "diagrams/geometry.graphml");
	window->checkDisk();
	QCOMPARE(doc->diskStatus(), EditorDocument::diskChanged);
	QString error;
	QVERIFY(!window->answerDiskBar("close", &error));
	QVERIFY(window->answerDiskBar("reload", &error));
	QCOMPARE(doc->diskStatus(), EditorDocument::diskOk);
	QVERIFY(doc->isClean());
	QCOMPARE(doc->undoStack()->count(), 0);
	QVERIFY(!m->idToElement("n0") || m->idToElement("n0")->get_name() != "Edited");
	QVERIFY(window->diskBar()->isHidden());
}

// EDIT-DOC-15: Compare opens a diff tab of the file against the tab, the bar kept
void TestDisk::test_answer_compare()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	CyberiadaSMModel* m = doc->model();
	QVERIFY(m->updateTitle(m->elementToIndex(m->idToElement("n0")), "Edited"));
	writeOver(doc->filePath(), "diagrams/geometry.graphml");
	window->checkDisk();
	int tabs = window->documentCount();
	QString error;
	QVERIFY2(window->answerDiskBar("compare", &error), qPrintable(error));
	QCOMPARE(window->documentCount(), tabs + 1);
	QVERIFY(window->currentComparison());
	QVERIFY(window->currentComparison()->title().contains("h.graphml"));
	// the diff tab has no bar; the tab of the file keeps its own
	QVERIFY(window->diskBar()->isHidden());
	QCOMPARE(doc->diskStatus(), EditorDocument::diskChanged);
	window->setCurrentDocument(doc);
	QVERIFY(!window->diskBar()->isHidden());
}

// EDIT-DOC-16: a gone file counts as modified; Keep closes the bar, the bytes back
// close it with the modification
void TestDisk::test_absent()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	QString path = doc->filePath();
	QVERIFY(QFile::remove(path));
	window->checkDisk();
	QCOMPARE(doc->diskStatus(), EditorDocument::diskAbsent);
	QVERIFY(!doc->isClean());
	QCOMPARE(window->diskBar()->text(), QString("h.graphml is not on disk"));
	QVERIFY(window->diskBar()->reloadButton()->isHidden());
	QVERIFY(!window->diskBar()->closeButton()->isHidden());
	QVERIFY(window->tabBar()->tabText(window->tabBar()->currentIndex()).endsWith("*"));
	QString error;
	QVERIFY(!window->answerDiskBar("reload", &error));
	QVERIFY(window->answerDiskBar("keep", &error));
	QVERIFY(window->diskBar()->isHidden());
	QVERIFY(!doc->isClean());
	// a further check leaves the dismissed bar closed
	window->checkDisk();
	QVERIFY(window->diskBar()->isHidden());
	// the file back as the tab read it: unmodified again
	QVERIFY(QFile::copy("diagrams/hierarchy.graphml", path));
	window->checkDisk();
	QCOMPARE(doc->diskStatus(), EditorDocument::diskOk);
	QVERIFY(doc->isClean());
}

// EDIT-DOC-16: Close closes the tab of a gone file without a question
void TestDisk::test_absent_close()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	QVERIFY(openCopy("diagrams/geometry.graphml", "g.graphml"));
	window->setCurrentDocument(doc);
	int tabs = window->documentCount();
	QVERIFY(QFile::remove(doc->filePath()));
	window->checkDisk();
	QString error;
	QVERIFY(window->answerDiskBar("close", &error));
	QCOMPARE(window->documentCount(), tabs - 1);
}

// EDIT-DOC-14: a file that cannot be read keeps the tab as it was, under the bar
void TestDisk::test_unreadable()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	int before = stateCount(doc->model());
	QFile file(doc->filePath());
	QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
	file.write("not a diagram");
	file.close();
	window->checkDisk();
	QCOMPARE(doc->diskStatus(), EditorDocument::diskUnreadable);
	QCOMPARE(stateCount(doc->model()), before);
	QCOMPARE(window->diskBar()->text(), QString("h.graphml changed on disk and cannot be read"));
	QVERIFY(window->diskBar()->compareButton()->isHidden());
	// a readable file again: the clean tab reads it
	writeOver(doc->filePath(), "diagrams/geometry.graphml");
	window->checkDisk();
	QCOMPARE(doc->diskStatus(), EditorDocument::diskOk);
	QVERIFY(stateCount(doc->model()) != before);
}

// EDIT-DOC-17: a Save over a file changed since the read asks first, the batch refuses
void TestDisk::test_save_guard()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	CyberiadaSMModel* m = doc->model();
	QVERIFY(m->updateTitle(m->elementToIndex(m->idToElement("n0")), "Edited"));
	QString error;
	QVERIFY(window->confirmDiskOverwrite(doc, &error));
	// the change not seen yet counts too
	writeOver(doc->filePath(), "diagrams/geometry.graphml");
	qApp->setProperty("batchMode", true);
	QVERIFY(!window->confirmDiskOverwrite(doc, &error));
	qApp->setProperty("batchMode", QVariant());
	QVERIFY(error.contains("changed on disk"));
	answerPrompt(tr("Cancel"));
	QVERIFY(!window->confirmDiskOverwrite(doc, nullptr));
	answerPrompt(tr("Overwrite"));
	QVERIFY(window->confirmDiskOverwrite(doc, nullptr));
	// Save asks, writes, and the file written is the remembered one
	answerPrompt(tr("Overwrite"));
	window->actionSave->trigger();
	QVERIFY(doc->isClean());
	DiskCheck saved = checkDiskState(doc->diskState(), doc->filePath(), nullptr);
	QVERIFY(saved == DiskCheck::same || saved == DiskCheck::touched);
}

// EDIT-DOC-15: the bar is the active tab's
void TestDisk::test_bar_follows_tab()
{
	CyberiadaSMEditorDocument* first = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(first);
	CyberiadaSMModel* m = first->model();
	QVERIFY(m->updateTitle(m->elementToIndex(m->idToElement("n0")), "Edited"));
	CyberiadaSMEditorDocument* second = openCopy("diagrams/geometry.graphml", "g.graphml");
	QVERIFY(second && second != first);
	writeOver(first->filePath(), "diagrams/geometry.graphml");
	window->checkDisk();
	QVERIFY(window->diskBar()->isHidden());
	window->setCurrentDocument(first);
	QVERIFY(!window->diskBar()->isHidden());
	window->setCurrentDocument(second);
	QVERIFY(window->diskBar()->isHidden());
}

// EDIT-DOC-14: a text tab reads its file again, the cursor kept
void TestDisk::test_text_reload()
{
	QString path = dir->filePath("notes.md");
	QVERIFY(QFile::copy("texts/notes.md", path));
	QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
	QString error;
	QVERIFY(window->openFile(path, &error));
	TextDocument* text = window->currentText();
	QVERIFY(text);
	QTextCursor cursor = text->editor()->textCursor();
	cursor.setPosition(2);
	text->editor()->setTextCursor(cursor);
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
	file.write("# Rewritten\n\nby another program\n");
	file.close();
	window->checkDisk();
	QCOMPARE(text->text(), QString("# Rewritten\n\nby another program\n"));
	QCOMPARE(text->editor()->textCursor().position(), 2);
	QVERIFY(text->isClean());
	// an edited text gets the bar
	text->editor()->insertPlainText("x");
	QVERIFY(!text->isClean());
	QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
	file.write("third\n");
	file.close();
	window->checkDisk();
	QCOMPARE(text->diskStatus(), EditorDocument::diskChanged);
	QVERIFY(text->text().contains("x"));
}

// EDIT-DOC-13: the watcher checks a file changed while the editor runs, a file
// replaced by a rename too
void TestDisk::test_watcher()
{
	CyberiadaSMEditorDocument* doc = openCopy("diagrams/hierarchy.graphml", "h.graphml");
	QVERIFY(doc);
	int before = stateCount(doc->model());
	writeOver(doc->filePath(), "diagrams/geometry.graphml");
	QTRY_VERIFY_WITH_TIMEOUT(stateCount(doc->model()) != before, 5000);
	int after = stateCount(doc->model());
	// the way git and a save replace a file: a new file renamed over the old one
	QFile in("diagrams/hierarchy.graphml");
	QVERIFY(in.open(QIODevice::ReadOnly));
	QSaveFile out(doc->filePath());
	QVERIFY(out.open(QIODevice::WriteOnly));
	out.write(in.readAll());
	QVERIFY(out.commit());
	QTRY_VERIFY_WITH_TIMEOUT(stateCount(doc->model()) != after, 5000);
	QCOMPARE(stateCount(doc->model()), before);
	// and the replaced file is watched again
	writeOver(doc->filePath(), "diagrams/geometry.graphml");
	QTRY_VERIFY_WITH_TIMEOUT(stateCount(doc->model()) == after, 5000);
}

QTEST_MAIN(TestDisk)
#include "l4-disk.moc"
