/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
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

#include <QString>

#include "cyberiadasm_dump.h"
#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "editable_text_item.h"
#include "settings_manager.h"

void dumpDocument(CyberiadaSMModel* model, std::ostream& os)
{
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
	case Cyberiada::elementTransition:     return "Transition";
	default:                               return "Unknown";
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
		os << std::string(size_t(depth) * 2, ' ')
		   << elementTypeName(element->get_type())
		   << ": {id: '" << id << "', " << geometry.toStdString() << "}" << std::endl;
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
		for (size_t i = 0; i < texts.size(); i++) {
			EditableTextItem* text = texts[i];
			QFont font = text->font();
			QRectF r = text->boundingRect();
			QString line = QString("{id: '%1', role: %2, font: '%3' %4%5, pos: (%6; %7), size: (%8; %9), text: '%10'}")
				.arg(QString::fromStdString(id))
				.arg(fontRoleName(text->getFontRole()))
				.arg(font.family())
				.arg(SettingsManager::instance().getFontSize(text->getFontRole()))
				.arg(font.bold() ? " bold" : "")
				.arg(roundedNumber(text->pos().x()), roundedNumber(text->pos().y()))
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

void dumpText(CyberiadaSMEditorScene* scene, CyberiadaSMModel* model, std::ostream& os)
{
	dumpTextElement(scene, model->rootDocument(), 0, os);
}
