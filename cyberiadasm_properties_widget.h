/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Properties Widget
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
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

#ifndef CYBERIADA_PROPERTIES_WIDGET
#define CYBERIADA_PROPERTIES_WIDGET

#include <qttreepropertybrowser.h>
#include <qtpropertymanager.h>
#include <qteditorfactory.h>
#include <QVector>
#include <QMap>

#include "cyberiadasm_model.h"

class CyberiadaSMPropertiesWidget: public QtTreePropertyBrowser {
Q_OBJECT

public:
	CyberiadaSMPropertiesWidget(QWidget *parent = NULL);

	void                     setModel(CyberiadaSMModel* model);

public slots:
	void                     slotElementSelected(const QModelIndex& index);
	void                     slotPropertyChanged(QtProperty* property);
	
private:
	
	CyberiadaSMModel*        model;
	Cyberiada::Element*      element;

	enum CyberiadaPropertyName {
		propActionType,
		propBehavior,
		propBody,
		propColor,
		propFormat,
		propFragment,
		propGroupAction,
		propGroupActions,
		propGroupComment,
        // propGroupBoundingRect,
		propGroupDocument,
		propGroupElement,
		propGroupGeometry,
		propGroupLabelPoint,
		propGroupMeta,
		propGroupPoint,
		propGroupPolyline,
		propGroupRect,
		propGroupSourcePoint,
		propGroupSubject,
		propGroupSubjects,
		propGroupTargetPoint,
		propGroupTransition,
		propGuard,
		propID,
		propMarkup,
		propMetaEventPropagation,
		propMetaStandardVersion,
		propMetaString,
		propMetaTransitionOrder,
		propName,
		propSource,
		propSubjectType,
		propTarget,
		propTrigger,
		propType,
	};

	enum CyberiadaPropertyEditor {
		propEditorActionType,
		propEditorColor,
		propEditorDate,
		propEditorElementType,
		propEditorFlag,
		propEditorFormatType,
		propEditorGroup,
		propEditorPointGroup,
		propEditorPolylineGroup,
		propEditorRectGroup,
		propEditorSourceElementLink,
		propEditorString,
		propEditorSubjectType,
		propEditorTargetElementLink,
	};
	
	struct CyberiadaProperty {
		CyberiadaPropertyName   name;
		CyberiadaPropertyEditor editor;
		QString                 propName;
		QString                 metaName;
	};

	QVector<CyberiadaProperty>  properties;

	QtGroupPropertyManager*     groupManager;
	QtStringPropertyManager*    stringManager;
	QtEnumPropertyManager*      enumManager;
	QtPointFPropertyManager*    pointManager;
	QtRectFPropertyManager*     rectManager;
	QtDateTimePropertyManager*  dateManager;
	QtBoolPropertyManager*      boolManager;
	
	QStringList                 elementTypesEnumNames;
	QMap<int, QIcon>            elementTypesEnumIcons;
	QStringList                 actionTypesEnumNames;
	QMap<int, QIcon>            actionTypesEnumIcons;	
	QStringList                 subjectTypesEnumNames;
	QMap<int, QIcon>            subjectTypesEnumIcons;	
	QStringList                 formatTypesEnumNames;
	QMap<int, QIcon>            formatTypesEnumIcons;	
	
	QtLineEditFactory*          lineEditFactory;

	void                        clearProperties();
	void                        newElement(Cyberiada::Element* new_element);
	QtProperty*                 constructProperty(CyberiadaPropertyName prop, const QString& alt_name = "");
	CyberiadaProperty&          findPropertyStruct(CyberiadaPropertyName prop);
	CyberiadaProperty&          findPropertyStruct(const QString& propName, const QString& alt_name = "");
	CyberiadaProperty&          findProperty(const QtProperty*);
	Cyberiada::ConstElementList getAllElements(bool source) const;
	QStringList                 generateElementNames(bool source) const;
	QMap<int, QIcon>            generateElementIcons(bool source) const;
	int                         getElementNumber(bool source, const Cyberiada::Element* e) const;
};

#endif
