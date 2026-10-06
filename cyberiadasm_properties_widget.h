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
#include <QWidget>

#include "cyberiadasm_model.h"

class CyberiadaSMEditorScene;
class MultilineEditorFactory;

class CyberiadaSMPropertiesWidget: public QtTreePropertyBrowser {
Q_OBJECT

public:
	CyberiadaSMPropertiesWidget(QWidget *parent = NULL);

	void                     setModel(CyberiadaSMModel* model);
	// the scene supplies the graphics item for an element so a geometry edit can
	// clamp to content and re-base the nested states, as the border drag does
	void                     setScene(CyberiadaSMEditorScene* scene);

	// open the multiline editor for a behaviour or a comment body row and write
	// the result back to the model through the row's manager (EDIT-TEXT-10)
	void                     editMultilineProperty(QtProperty* property);

public slots:
	void                     slotElementSelected(const QModelIndex& index);
    void                     slotModelDataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight);
	void slotModelAboutToBeReset();
	void                     slotPropertyChanged(QtProperty* property);
	void                     slotInspectorModeChanged(bool on);
	void                     slotCurrentItemChanged(QtBrowserItem* item);

protected:
	// the document metainformation: add a standard parameter or remove a free-form one
	void                     contextMenuEvent(QContextMenuEvent* event) override;

private:
	// clear and rebuild the property tree for the current element (row set changed)
	void                     rebuildProperties();

	CyberiadaSMModel*        model;
	CyberiadaSMEditorScene*  scene = nullptr;
	Cyberiada::Element*      element;
    bool                     updating;
    // the free-form metainformation rows, mapped to their canonical parameter key
    QMap<QtProperty*, QString> metaStringKeys;


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
		propGroupLabelRect,
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
		propMetaGeometry,
		propMetaStandardVersion,
		propMetaString,
		propMetaTransitionOrder,
		propName,
		propSource,
		propSubmachineRef,
		propSubjectTarget,
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
		propEditorEventPropagation,
		propEditorFlag,
		propEditorFormatType,
		propEditorGeometryDeclaration,
		propEditorGroup,
		propEditorPointGroup,
		propEditorPolylineGroup,
		propEditorRectGroup,
		propEditorSourceElementLink,
		propEditorString,
		propEditorMultilineString,
		propEditorSubjectElementLink,
		propEditorSubjectType,
		propEditorTargetElementLink,
		propEditorTransitionOrder,
	};

	// the element lists offered by the link properties
	enum ElementListKind {
		listSource,                                   // the transition source
		listTarget,                                   // the transition target
		listSubject,                                  // the comment subject
	};
	
	struct CyberiadaProperty {
		CyberiadaPropertyName   name;
		CyberiadaPropertyEditor editor;
		QString                 propName;
		QString                 metaName;
	};

    QVector<CyberiadaProperty>  cProperties;

	QtGroupPropertyManager*     groupManager;
	QtStringPropertyManager*    stringManager;
	// the multiline rows (a behaviour, a comment body) edited through a dialog
	QtStringPropertyManager*    multilineStringManager;
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
	QStringList                 transitionOrderEnumNames;
	QStringList                 eventPropagationEnumNames;
	QStringList                 geometryDeclarationEnumNames;
	
	QtLineEditFactory*          lineEditFactory;
	MultilineEditorFactory*     multilineEditFactory;
    QtEnumEditorFactory*        enumEditorFactory;
    // QtDateTimeEditorFactory*    dateTimeEditorFactory;
    QtCheckBoxFactory*          checkBoxFactory;
    QtDoubleSpinBoxFactory*     doubleSpinBoxFactory;

	void                        clearProperties();
	void                        newElement(Cyberiada::Element* new_element);
    void                        updateElement();
	// the string value of a row, read from its own manager (string or multiline)
	QString                     propertyString(QtProperty* property) const;
	QtProperty*                 constructProperty(CyberiadaPropertyName prop, const QString& alt_name = "");
	CyberiadaProperty&          findPropertyStruct(CyberiadaPropertyName prop);
	CyberiadaProperty&          findPropertyStruct(const QString& propName, const QString& alt_name = "");
    CyberiadaProperty&          findProperty(const QtProperty*);
    QtProperty*                 findQtProperty(QtProperty* root, const QString& prop_name, int index = 0);
    QtProperty*                 getPropertyParent(QtProperty* property);
    int                         getPropertyIndex(QtProperty* property);
	Cyberiada::ConstElementList getAllElements(ElementListKind kind) const;
	QStringList                 generateElementNames(ElementListKind kind) const;
	QMap<int, QIcon>            generateElementIcons(ElementListKind kind) const;
	int                         getElementNumber(ElementListKind kind, const Cyberiada::Element* e) const;
    const Cyberiada::Element*   getElementByNumber(ElementListKind kind, int index) const;
};

// the editor of a multiline string row: a one-line preview and a button that
// opens the multiline dialog through the owning widget (EDIT-TEXT-10)
class MultilineEditButton: public QWidget {
public:
	MultilineEditButton(CyberiadaSMPropertiesWidget* owner, QtStringPropertyManager* manager,
	                    QtProperty* property, QWidget* parent);
private:
	void                        refresh();
	CyberiadaSMPropertiesWidget* owner;
	QtStringPropertyManager*    manager;
	QtProperty*                 property;
	class QLineEdit*            preview;
};

// the factory wiring the multiline string manager to the button editor
class MultilineEditorFactory: public QtAbstractEditorFactory<QtStringPropertyManager> {
public:
	MultilineEditorFactory(CyberiadaSMPropertiesWidget* owner, QObject* parent = NULL):
	    QtAbstractEditorFactory<QtStringPropertyManager>(parent), owner(owner) {}
protected:
	void     connectPropertyManager(QtStringPropertyManager*) {}
	void     disconnectPropertyManager(QtStringPropertyManager*) {}
	QWidget* createEditor(QtStringPropertyManager* manager, QtProperty* property, QWidget* parent) {
		return new MultilineEditButton(owner, manager, property, parent);
	}
private:
	CyberiadaSMPropertiesWidget* owner;
};

#endif
