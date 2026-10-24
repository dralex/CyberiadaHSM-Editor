/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The canonical document/scene dump for the batch mode
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

#include <algorithm>
#include <string>
#include <vector>

#include <QDir>
#include <QString>
#include <QUndoStack>
#include <QStringList>
#include <QLineF>

#include "cyberiadasm_dump.h"
#include "cyberiadasm_editor_subject_item.h"
#include "plugin_runner.h"
#include <algorithm>
#include "smeditor_window.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "editable_text_item.h"
#include "settings_manager.h"

// an untitled document before its first edit has no root yet
void dumpDocument(CyberiadaSMModel* model, std::ostream& os)
{
	if (!model->rootDocument()) {
		os << "(empty)" << std::endl;
		return;
	}
	os << *model->rootDocument() << std::endl;
}

static const char* elementTypeName(Cyberiada::ElementType t)
{
	switch (t) {
	case Cyberiada::elementRoot:           return "Document";
	case Cyberiada::elementSM:             return "State Machine";
	case Cyberiada::elementSimpleState:    return "Simple State";
	case Cyberiada::elementCompositeState: return "Composite State";
	case Cyberiada::elementComment:        return "Comment";
	case Cyberiada::elementFormalComment:  return "Formal Comment";
	case Cyberiada::elementInitial:        return "Initial";
	case Cyberiada::elementFinal:          return "Final";
	case Cyberiada::elementChoice:         return "Choice";
	case Cyberiada::elementTerminate:      return "Terminate";
	case Cyberiada::elementShallowHistory: return "Shallow History";
	case Cyberiada::elementDeepHistory:    return "Deep History";
	case Cyberiada::elementSubmachineState: return "Submachine State";
	case Cyberiada::elementEntryPoint:     return "Entry Point";
	case Cyberiada::elementExitPoint:      return "Exit Point";
	case Cyberiada::elementTransition:     return "Transition";
	default:                               return "Unknown";
	}
}

// a fixed 2-decimal coordinate, locale-independent, without -0.00
static QString sceneNumber(double v)
{
	// the subject-link ends and its underline derive from the drawn text layout, which
	// shifts by a fraction of a pixel with the font renderer across machines (observed
	// ~0.34px, clustering on half-pixel positions). Snap to the nearest half pixel so
	// the golden is portable: the jitter stays well inside one half-pixel cell, while a
	// whole-pixel grid would split a value sitting on an x.5 boundary.
	double r = qRound(v * 2.0) / 2.0;
	if (r == 0.0) r = 0.0;  // avoid the -0 output
	return QString::number(r, 'f', 1);
}

// the == scene accumulation the dump reader applies: the sum of item->pos() over the
// element and its model ancestors. The children of a composite live in its StateRegion,
// a non-model item offset below the title, which the reader does not see - so a nested
// element's reconstructed origin omits that title-band offset.
static QPointF dumpSceneOrigin(CyberiadaSMEditorScene* scene, const Cyberiada::Element* element)
{
	QPointF o;
	for (const Cyberiada::Element* e = element; e != NULL; e = e->get_parent()) {
		QGraphicsItem* it = scene->getMap().value(e->get_id(), NULL);
		if (it) o += it->pos();
	}
	return o;
}

// a link end comes back in true scene coordinates; the dumped element rects omit the
// StateRegion title-band offset of a nested owner, so bring the end into the same frame
// (subtract that offset) and it lies on the dumped comment/target border (EDIT-EDGE-17/19)
static QPointF inDumpFrame(CyberiadaSMEditorScene* scene, const Cyberiada::Element* owner, const QPointF& p)
{
	if (!owner) return p;
	QGraphicsItem* it = scene->getMap().value(owner->get_id(), NULL);
	if (!it) return p;
	return p - (it->scenePos() - dumpSceneOrigin(scene, owner));
}

static QString escapeText(const QString& text);

// the drawn links of a comment: their target and ends (EDIT-EDGE-6/17), a fragment
// with its underline in the frame of the target (EDIT-EDGE-21)
static void dumpSubjectLinks(CyberiadaSMEditorScene* scene, const Cyberiada::Element* comment,
							 int depth, std::ostream& os)
{
	QList<CyberiadaSMEditorSubjectItem*> links = scene->subjectLinks(comment->get_id());
	for (CyberiadaSMEditorSubjectItem* link : links) {
		const Cyberiada::Element* target = link->subject().get_element();
		QPointF from = inDumpFrame(scene, comment, link->sourceEnd());
		QPointF to = inDumpFrame(scene, target, link->targetEnd());
		QString fragment;
		if (link->subject().has_fragment()) {
			QString f = escapeText(QString::fromStdString(link->subject().get_fragment()));
			fragment = QString(", fragment: '%1'").arg(f.replace("'", "\\'"));
			QStringList lines;
			for (const QLineF& l : link->underline()) {
				QPointF a = inDumpFrame(scene, target, l.p1()), b = inDumpFrame(scene, target, l.p2());
				lines << QString("(%1; %2; %3)").arg(sceneNumber(a.x()), sceneNumber(a.y()), sceneNumber(b.x()));
			}
			if (!lines.isEmpty()) fragment += ", underline: [" + lines.join(", ") + "]";
		}
		os << std::string(size_t(depth) * 2, ' ') << "Subject Link: {index: " << link->getIndex()
		   << ", target: '" << (target ? target->get_id() : std::string()) << "', "
		   << QString("from: (%1; %2), to: (%3; %4)")
			  .arg(sceneNumber(from.x()), sceneNumber(from.y()), sceneNumber(to.x()), sceneNumber(to.y()))
			  .toStdString()
		   << fragment.toStdString() << "}" << std::endl;
	}
}

static void dumpSceneElement(CyberiadaSMEditorScene* scene, Cyberiada::Element* element,
							 int depth, std::ostream& os)
{
	Cyberiada::ID id = element->get_id();
	QGraphicsItem* item = scene->getMap().value(id, NULL);
	if (item && element->get_type() != Cyberiada::elementRoot) {
		QPointF p = item->pos();
		QRectF r = item->boundingRect();
		double v[6] = { p.x(), p.y(), r.x(), r.y(), r.width(), r.height() };
		QString numbers[6];
		for (int j = 0; j < 6; j++) {
			if (v[j] == 0.0) v[j] = 0.0;  // avoid the -0.00 output
			// QString::number is locale-independent, unlike printf which
			// follows LC_NUMERIC set by QApplication from the environment
			numbers[j] = QString::number(v[j], 'f', 2);
		}
		QString geometry = QString("pos: (%1; %2), rect: (%3; %4; %5; %6)")
			.arg(numbers[0], numbers[1], numbers[2], numbers[3], numbers[4], numbers[5]);
		std::string color;
		if (Cyberiada::element_has_color(element)) {
			color = ", color: '" + Cyberiada::element_get_color(element) + "'";
		}
		os << std::string(size_t(depth) * 2, ' ')
		   << elementTypeName(element->get_type())
		   << ": {id: '" << id << "', " << geometry.toStdString() << color << "}" << std::endl;
		if (Cyberiada::element_is_comment(element)) {
			dumpSubjectLinks(scene, element, depth + 1, os);
		}
	}
	Cyberiada::ElementCollection* collection = dynamic_cast<Cyberiada::ElementCollection*>(element);
	if (collection) {
		const Cyberiada::ElementList& children = collection->get_children();
		for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
			dumpSceneElement(scene, *i, depth + 1, os);
		}
	}
}

void dumpScene(CyberiadaSMEditorScene* scene, CyberiadaSMModel* model, std::ostream& os)
{
	if (!model->rootDocument()) return;
	dumpSceneElement(scene, model->rootDocument(), 0, os);
}

static const char* fontRoleName(FontRole role)
{
	switch (role) {
	case fontRoleStateTitle:    return "title";
	case fontRoleStateAction:   return "action";
	case fontRoleTransition:    return "transition";
	case fontRoleComment:       return "comment";
	case fontRoleFormalComment: return "formal comment";
	default:                    return "unknown";
	}
}

// the text is one line here, so the reference files stay comparable
static QString escapeText(const QString& text)
{
	QString result = text;
	result.replace("\\", "\\\\");
	result.replace("\n", "\\n");
	return result;
}

// the layout is compared at the pixel: the fractions differ between the
// text engines even when the font file and the point size are the same
static QString roundedNumber(double value)
{
	double rounded = double(qRound(value));
	if (rounded == 0.0) rounded = 0.0;  // avoid the -0 output
	return QString::number(rounded, 'f', 0);
}

static void dumpTextElement(CyberiadaSMEditorScene* scene, Cyberiada::Element* element,
							int depth, std::ostream& os)
{
	Cyberiada::ID id = element->get_id();
	QGraphicsItem* item = scene->getMap().value(id, NULL);
	if (item && element->get_type() != Cyberiada::elementRoot) {
		std::vector<EditableTextItem*> texts = textItemsOf(item);
		for (size_t i = 0; i < texts.size(); i++) {
			EditableTextItem* text = texts[i];
			QFont font = text->font();
			QRectF r = text->boundingRect();
			// pos is local to the element; abs is the scene position of the text
			// item, so a checker can probe the ink without re-deriving the layout
			QPointF sp = text->scenePos();
			QString line = QString("{id: '%1', role: %2, font: '%3' %4%5, pos: (%6; %7), abs: (%8; %9), size: (%10; %11), text: '%12'}")
				.arg(QString::fromStdString(id))
				.arg(fontRoleName(text->getFontRole()))
				.arg(font.family())
				.arg(SettingsManager::instance().getFontSize(text->getFontRole()))
				.arg(font.bold() ? " bold" : "")
				.arg(roundedNumber(text->pos().x()), roundedNumber(text->pos().y()))
				.arg(roundedNumber(sp.x()), roundedNumber(sp.y()))
				.arg(roundedNumber(r.width()), roundedNumber(r.height()))
				.arg(escapeText(text->toPlainText()));
			os << std::string(size_t(depth) * 2, ' ')
			   << elementTypeName(element->get_type()) << ": "
			   << line.toStdString() << std::endl;
		}
	}
	Cyberiada::ElementCollection* collection = dynamic_cast<Cyberiada::ElementCollection*>(element);
	if (collection) {
		const Cyberiada::ElementList& children = collection->get_children();
		for (Cyberiada::ElementList::const_iterator i = children.begin(); i != children.end(); i++) {
			dumpTextElement(scene, *i, depth + 1, os);
		}
	}
}

std::vector<EditableTextItem*> textItemsOf(QGraphicsItem* item)
{
	std::vector<EditableTextItem*> texts;
	const QList<QGraphicsItem*>& children = item->childItems();
	for (QList<QGraphicsItem*>::const_iterator i = children.begin(); i != children.end(); i++) {
		EditableTextItem* text = dynamic_cast<EditableTextItem*>(*i);
		// the dump mirrors the render: a hidden title (a rect-less state
		// machine has one but does not draw it) is not listed
		if (text && text->isVisible()) texts.push_back(text);
	}
	// the insertion order of the children is not a part of the contract
	std::sort(texts.begin(), texts.end(), [](EditableTextItem* a, EditableTextItem* b) {
		if (a->getFontRole() != b->getFontRole()) return a->getFontRole() < b->getFontRole();
		if (a->pos().y() != b->pos().y()) return a->pos().y() < b->pos().y();
		return a->pos().x() < b->pos().x();
	});
	return texts;
}

void dumpText(CyberiadaSMEditorScene* scene, CyberiadaSMModel* model, std::ostream& os)
{
	dumpTextElement(scene, model->rootDocument(), 0, os);
}

void dumpStack(CyberiadaSMModel* model, std::ostream& os)
{
	const QUndoStack* stack = model->undoStack();
	os << "count: " << stack->count() << std::endl;
	os << "index: " << stack->index() << std::endl;
	os << "clean: " << (stack->isClean() ? "yes" : "no") << std::endl;
}

void dumpTabs(CyberiadaSMEditorWindow* win, std::ostream& os)
{
	os << "count: " << win->documentCount() << std::endl;
	for (int i = 0; i < win->documentCount(); i++) {
		CyberiadaSMEditorDocument* doc = win->documentAt(i);
		os << i << " " << doc->title().toStdString() << (doc->isClean() ? "" : "*");
		if (doc == win->currentDocument()) os << " active";
		os << std::endl;
	}
}

void dumpPlugins(const QList<PluginEntry>& entries, std::ostream& os)
{
	for (const PluginEntry& e: entries) {
		QString version = e.descriptor.version.isEmpty() ? QString("-") : e.descriptor.version;
		os << e.name().toStdString() << " " << version.toStdString() << " "
		   << PluginEntry::statusName(e.status).toStdString() << " " << e.origin.toStdString();
		if (!e.reason.isEmpty()) os << ": " << e.reason.simplified().toStdString();
		os << std::endl;
		for (const QString& w: e.descriptor.warnings) {
			os << "  warning: " << w.toStdString() << std::endl;
		}
	}
}

// the machine paths of a run are printed by name, longest first (EDIT-PLUG-28)
static QString normalizePaths(QString text, const PluginResult& result)
{
	QList<QPair<QString, QString> > names;
	if (!result.runDir.isEmpty()) names.append(qMakePair(result.runDir, QString("<rundir>")));
	if (!result.pluginDir.isEmpty()) names.append(qMakePair(result.pluginDir, QString("<plugin_dir>")));
	if (!result.documentDir.isEmpty()) names.append(qMakePair(result.documentDir, QString("<document_dir>")));
	// a Windows program prints them with its own separators too
	for (int i = 0, n = names.size(); i < n; i++) {
		QString native = QDir::toNativeSeparators(names.at(i).first);
		if (native != names.at(i).first) names.append(qMakePair(native, names.at(i).second));
	}
	std::sort(names.begin(), names.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
		return a.first.size() > b.first.size();
	});
	for (const QPair<QString, QString>& n: names) text.replace(n.first, n.second);
	return text;
}

void dumpPluginRun(const PluginEntry& plugin, const PluginAction& action,
				   const PluginResult& result, std::ostream& os)
{
	static const char* const states[] = { "done", "failed", "cancelled" };
	os << "action: " << plugin.descriptor.id.toStdString() << ":" << action.id.toStdString() << std::endl;
	os << "result: " << PluginDescriptor::resultName(action.result).toStdString() << std::endl;
	static const char* const metaKinds[] = { "set", "kept", "skipped" };
	for (const PluginMetaNote& note: result.metaNotes) {
		os << "meta: " << metaKinds[note.kind] << " " << note.key.toStdString();
		if (note.kind != PluginMetaNote::metaSkipped) os << " " << note.value.toStdString();
		os << std::endl;
	}
	os << "state: " << states[result.state] << std::endl;
	os << "exit: ";
	if (result.exitCode < 0) os << "-";
	else os << result.exitCode;
	os << std::endl;
	if (result.state == PluginResult::stateDone) {
		if (action.result == PluginAction::resultReplace) {
			os << "changes: " << (result.changed ? "yes" : "no") << std::endl;
		} else if (action.result == PluginAction::resultNewDocument) {
			os << "tab: " << result.tab << std::endl;
		} else if (action.result == PluginAction::resultFiles && !result.files.isEmpty()) {
			os << "files:" << std::endl;
			for (const PluginFileChange& f: result.files) {
				os << "  " << (f.isNew ? "new " : "changed ") << f.path.toStdString() << std::endl;
			}
		}
	} else {
		os << "error: " << normalizePaths(result.error, result).toStdString() << std::endl;
	}
	if (!result.stdoutText.isEmpty()) {
		os << "stdout:" << std::endl;
		QString text = normalizePaths(result.stdoutText, result);
		for (const QString& line: text.split('\n')) {
			if (line.isEmpty()) continue;
			os << "  " << line.toStdString() << std::endl;
		}
	}
}

void dumpDiagnostics(const PluginResult& result, std::ostream& os)
{
	os << "count: " << result.diagnostics.size() << std::endl;
	for (const PluginDiagnostic& d: result.diagnostics) {
		os << d.severity.toStdString() << " " << (d.rule.isEmpty() ? std::string("-") : d.rule.toStdString())
		   << " " << (d.element.isEmpty() ? std::string("-") : d.element.toStdString());
		if (!d.element.isEmpty() && !d.linked) os << " (absent)";
		if (d.line > 0) os << " line " << d.line;
		os << ": " << d.message.toStdString() << std::endl;
	}
}
