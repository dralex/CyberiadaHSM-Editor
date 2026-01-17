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

#include <QDebug>

#include "myassert.h"
#include "cyberiadasm_properties_widget.h"
#include "cyberiada_constants.h"

CyberiadaSMPropertiesWidget::CyberiadaSMPropertiesWidget(QWidget *parent):
	QtTreePropertyBrowser(parent), model(NULL), element(NULL)
{
    cProperties = {
		{propActionType,           propEditorActionType,        tr("Action Type", "Property name")},
		{propBehavior,             propEditorString,            tr("Behavior", "Property name")},
		{propBody,                 propEditorString,            tr("Body", "Property name")},
		{propColor,                propEditorColor,             tr("Color", "Property name")},
		{propFormat,               propEditorFormatType,        tr("Format", "Property name")},
		{propFragment,             propEditorString,            tr("Fragment", "Property name")},
		{propGroupAction,          propEditorGroup,             tr("Action", "Property name")},
		{propGroupActions,         propEditorGroup,             tr("Actions", "Property name")},
		{propGroupComment,         propEditorGroup,             tr("Comment", "Property name")},
		{propGroupDocument,        propEditorGroup,             tr("Document", "Property name")},
		{propGroupElement,         propEditorGroup,             tr("Element", "Property name")},
		{propGroupGeometry,        propEditorGroup,             tr("Geometry", "Property name")},
		{propGroupLabelPoint,      propEditorPointGroup,        tr("Label Point", "Property name")},
		{propGroupMeta,            propEditorGroup,             tr("Metainformation", "Property name")},
		{propGroupPoint,           propEditorPointGroup,        tr("Point", "Property name")},
		{propGroupPolyline,        propEditorGroup,             tr("Polyline", "Property name")},
		{propGroupRect,            propEditorRectGroup,         tr("Rect", "Property name")},
        // {c,    propEditorRectGroup,         tr("Bounding Rect", "Property name")},
		{propGroupSourcePoint,     propEditorPointGroup,        tr("Source Point", "Property name")},
		{propGroupSubject,         propEditorGroup,             tr("Subject", "Property name")},
		{propGroupSubjects,        propEditorGroup,             tr("Subjects", "Property name")},
		{propGroupTargetPoint,     propEditorPointGroup,        tr("Target Point", "Property name")},
		{propGroupTransition,      propEditorGroup,             tr("Transition", "Property name")},
		{propGuard,                propEditorString,            tr("Guard", "Property name")},
		{propID,                   propEditorString,            tr("ID", "Property name")},
		{propMarkup,               propEditorString,            tr("Markup", "Property name")},
		{propMetaString,           propEditorString,            ""},
		{propMetaEventPropagation, propEditorFlag,              tr(METAINFORMATION_EVENT_PROPAGATION, "Property name")},
		{propMetaStandardVersion,  propEditorString,            tr(METAINFORMATION_STANDARD_VERSION, "Property name")},
		{propMetaTransitionOrder,  propEditorFlag,              tr(METAINFORMATION_TRANSITION_ORDER, "Property name")},
		{propName,                 propEditorString,            tr("Name", "Property name")},
		{propSource,               propEditorSourceElementLink, tr("Source", "Property name")},
		{propSubjectType,          propEditorSubjectType,       tr("Subject Type", "Property name")},
		{propTarget,               propEditorTargetElementLink, tr("Target", "Property name")},
		{propTrigger,              propEditorString,            tr("Trigger", "Property name")},
		{propType,                 propEditorElementType,       tr("Type", "Property name")},
	};

	groupManager = new QtGroupPropertyManager(this);

	stringManager = new QtStringPropertyManager(this);
    connect(stringManager, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(slotPropertyChanged(QtProperty*)));
    lineEditFactory = new QtLineEditFactory(this);
    setFactoryForManager(stringManager, lineEditFactory);

	enumManager = new QtEnumPropertyManager(this);
    connect(enumManager, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(slotPropertyChanged(QtProperty*)));
    enumEditorFactory = new QtEnumEditorFactory(this);
    setFactoryForManager(enumManager, enumEditorFactory);

	pointManager = new QtPointFPropertyManager(this);
    // connect(pointManager, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(slotPropertyChanged(QtProperty*)));
    // lineEditFactory = new QtPoin(this);
    // setFactoryForManager(stringManager, lineEditFactory);
	rectManager = new QtRectFPropertyManager(this);
	dateManager = new QtDateTimePropertyManager(this);

	boolManager = new QtBoolPropertyManager(this);
    connect(boolManager, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(slotPropertyChanged(QtProperty*)));
    checkBoxFactory = new QtCheckBoxFactory(this);
    setFactoryForManager(boolManager, checkBoxFactory);

	QMap<Cyberiada::ActionType, QString> actionTypes = {
		{Cyberiada::actionTransition, tr("Transition", "Action type")},
		{Cyberiada::actionEntry,      tr("Entry", "Action type")},
		{Cyberiada::actionExit,       tr("Exit", "Action type")}
	};
	QMap<Cyberiada::ActionType, QIcon> actionTypeIcons = {
		{Cyberiada::actionTransition, QIcon(":/Icons/images/trans.png")},
		{Cyberiada::actionEntry,      QIcon(":/Icons/images/entry.png")},
		{Cyberiada::actionExit,       QIcon(":/Icons/images/exit.png")}
	};

	for (int i = 0; i < actionTypes.size(); i++) {
		Cyberiada::ActionType t = Cyberiada::ActionType(i);
		actionTypesEnumNames << actionTypes[t];
		actionTypesEnumIcons[t] = actionTypeIcons[t];
	}

	QMap<Cyberiada::CommentSubjectType, QString> subjectTypes = {
		{Cyberiada::commentSubjectElement, tr("Element", "Subject type")},
		{Cyberiada::commentSubjectName,    tr("Name", "Subject type")},
		{Cyberiada::commentSubjectData,    tr("Data", "Subject type")}
	};
	QMap<Cyberiada::CommentSubjectType, QIcon> subjectTypeIcons = {
		{Cyberiada::commentSubjectElement, QIcon(":/Icons/images/subject-element.png")},
		{Cyberiada::commentSubjectName,    QIcon(":/Icons/images/subject-name.png")},
		{Cyberiada::commentSubjectData,    QIcon(":/Icons/images/subject-data.png")}
	};

	for (int i = 0; i < subjectTypes.size(); i++) {
		Cyberiada::CommentSubjectType t = Cyberiada::CommentSubjectType(i);
		subjectTypesEnumNames << subjectTypes[t];
		subjectTypesEnumIcons[t] = subjectTypeIcons[t];
	}

	formatTypesEnumNames << tr("Cyberiada GraphML 1.0", "GraphML Format");
	formatTypesEnumNames << tr("Legacy YED", "GraphML Format");
	formatTypesEnumIcons[0] = QIcon(":/Icons/images/format-cyberiada.png");
	formatTypesEnumIcons[1] = QIcon(":/Icons/images/format-yed.png");
	
	setResizeMode(ResizeToContents);

    updating = false;
}

void CyberiadaSMPropertiesWidget::setModel(CyberiadaSMModel* model)
{
	MY_ASSERT(model);
	this->model = model;
	element = NULL;

	QMap<Cyberiada::ElementType, QString> types = {
		{Cyberiada::elementRoot,           tr("Document", "Element type")},
		{Cyberiada::elementSM,             tr("State Machine", "Element type")},
		{Cyberiada::elementSimpleState,    tr("Simple State", "Element type")},
		{Cyberiada::elementCompositeState, tr("Composite State", "Element type")},
		{Cyberiada::elementComment,        tr("Comment", "Element type")},
		{Cyberiada::elementFormalComment,  tr("Formal Comment", "Element type")},
		{Cyberiada::elementInitial,        tr("Initial", "Element type")},
		{Cyberiada::elementFinal,          tr("Final", "Element type")},
		{Cyberiada::elementChoice,         tr("Choice", "Element type")},
		{Cyberiada::elementTerminate,      tr("Terminate", "Element type")},
		{Cyberiada::elementTransition,     tr("Transition", "Element type")}
	};
	
	for (int i = 0; i < types.size(); i++) {
		Cyberiada::ElementType t = Cyberiada::ElementType(i);
		elementTypesEnumNames << types[t];
		elementTypesEnumIcons[t] = model->getElementIcon(t);
    }

    connect(model, &CyberiadaSMModel::dataChanged, this, &CyberiadaSMPropertiesWidget::slotModelDataChanged);
}

void CyberiadaSMPropertiesWidget::clearProperties()
{
	groupManager->clear();
	stringManager->clear();
	enumManager->clear();
	pointManager->clear();
	rectManager->clear();
	dateManager->clear();
	boolManager->clear();
}

void CyberiadaSMPropertiesWidget::slotElementSelected(const QModelIndex& index)
{
	clearProperties();
	if (model && index.isValid() && index != model->rootIndex()) {
		Cyberiada::Element* new_element = model->indexToElement(index);
		MY_ASSERT(new_element);
		newElement(new_element);
	} else {
		element = NULL;
	}
}

void CyberiadaSMPropertiesWidget::slotModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (model && topLeft.isValid() && topLeft != model->rootIndex()) {
        Cyberiada::Element* changed_element = model->indexToElement(topLeft);
        MY_ASSERT(changed_element);
        if (element == changed_element) {
            updateElement();
        }
    }
}

void CyberiadaSMPropertiesWidget::slotPropertyChanged(QtProperty* p)
{
    if (updating) return;

    if (!element) return;

    const CyberiadaProperty& cp = findProperty(p);
    Cyberiada::ElementType type = element->get_type();
    QModelIndex i = model->elementToIndex(element);

    if (type == Cyberiada::elementRoot) {
        const Cyberiada::LocalDocument* doc = model->rootDocument();
        MY_ASSERT(doc);

        if (cp.name == propFormat) {
            // TODO
        }
        // QtProperty* format_type_prop = constructProperty(propFormat);
        // enumManager->setValue(format_type_prop, doc->get_file_format());
        // doc_group_prop->addSubProperty(format_type_prop);

        // // QtProperty* bounding_group_prop = constructProperty(propGroupBoundingRect);
        // // Cyberiada::Rect r = doc->get_bound_rect();
        // // rectManager->setValue(bounding_group_prop, QRectF(r.x, r.y, r.width, r.height));
        // // doc_group_prop->addSubProperty(bounding_group_prop);

        if (cp.name == propMetaStandardVersion) {
            // TODO
        }
        // QtProperty* standard_version_prop = constructProperty(propMetaStandardVersion);
        // stringManager->setValue(standard_version_prop, QString(doc->meta().standard_version.c_str()));
        // meta_group_prop->addSubProperty(standard_version_prop);

        // for (std::vector<std::pair<Cyberiada::String, Cyberiada::String>>::const_iterator i = doc->meta().strings.begin();
        //      i != doc->meta().strings.end();
        //      i++) {
        //     QtProperty* platform_string_prop = constructProperty(propMetaString, i->first.c_str());
        //     stringManager->setValue(platform_string_prop, i->second.c_str());
        //     meta_group_prop->addSubProperty(platform_string_prop);
        // }

        if (cp.name == propMetaTransitionOrder) {
            // TODO
        }
//         QtProperty* transition_order_prop = constructProperty(propMetaTransitionOrder);
//         boolManager->setValue(transition_order_prop, doc->meta().transition_order_flag);
//         meta_group_prop->addSubProperty(transition_order_prop);

        if (cp.name == propMetaEventPropagation) {
            // TODO
        }
//         QtProperty* event_propagation_prop = constructProperty(propMetaEventPropagation);
//         boolManager->setValue(event_propagation_prop, doc->meta().event_propagation_flag);
//         meta_group_prop->addSubProperty(event_propagation_prop);

    } else {
        if (cp.name == propID) {
            model->updateID(i, stringManager->value(p));
        }

        if (type == Cyberiada::elementTransition) {
            const Cyberiada::Transition* trans = static_cast<const Cyberiada::Transition*>(element);
            MY_ASSERT(trans);

            if (cp.name == propSource) {
                const Cyberiada::Element* sourceElement = getElementByNumber(true, enumManager->value(p));
                MY_ASSERT(sourceElement);
                model->updateGeometry(i, sourceElement->get_id(), trans->target_element_id());
                Cyberiada::ElementType sourceType = sourceElement->get_type();
                Cyberiada::Point newSourcePoint = Cyberiada::Point(0, 0);
                if (sourceType == Cyberiada::elementSimpleState ||
                    sourceType == Cyberiada::elementCompositeState) {
                    const Cyberiada::ElementCollection* coll = static_cast<const Cyberiada::ElementCollection*>(sourceElement);
                    Cyberiada::Rect rect = coll->get_geometry_rect();
                    newSourcePoint = Cyberiada::Point(rect.width / 2, 0);
                }
                model->updateGeometry(i, newSourcePoint, trans->get_target_point());

            }

            if (cp.name == propTarget) {
                const Cyberiada::Element* targetElement = getElementByNumber(false, enumManager->value(p));
                MY_ASSERT(targetElement);
                model->updateGeometry(i, trans->source_element_id(), targetElement->get_id());
                Cyberiada::ElementType targetType = targetElement->get_type();
                Cyberiada::Point newTargetPoint = Cyberiada::Point(0, 0);
                if (targetType == Cyberiada::elementSimpleState ||
                    targetType == Cyberiada::elementCompositeState) {
                    const Cyberiada::ElementCollection* coll = static_cast<const Cyberiada::ElementCollection*>(targetElement);
                    Cyberiada::Rect rect = coll->get_geometry_rect();
                    newTargetPoint = Cyberiada::Point(rect.width / 2, 0);
                }
                model->updateGeometry(i, trans->get_source_point(), newTargetPoint);
            }

            const Cyberiada::Action& a = trans->get_action();

            if (cp.name == propTrigger) {
                if (stringManager->value(p).isEmpty()) {
                    model->deleteAction(i);
                } else {
                    model->updateAction(i, 0, stringManager->value(p), a.get_guard().c_str(), a.get_behavior().c_str());
                }
            }
            if (cp.name == propGuard) {
                model->updateAction(i, 0, a.get_trigger().c_str(), stringManager->value(p), a.get_behavior().c_str());
            }
            if (cp.name == propBehavior) {
                model->updateAction(i, 0, a.get_trigger().c_str(), a.get_guard().c_str(), stringManager->value(p));
            }

            if (trans->has_geometry()) {
                if (trans->has_geometry_source_point()) {
                    if (cp.name == propGroupSourcePoint) {
                        QPointF sp = pointManager->value(p);
                        model->updateGeometry(i, Cyberiada::Point(sp.x(), sp.y()), trans->get_target_point());
                    }
                }
                if (trans->has_geometry_target_point()) {
                    if (cp.name == propGroupTargetPoint) {
                        QPointF tp = pointManager->value(p);
                        model->updateGeometry(i, trans->get_source_point(), Cyberiada::Point(tp.x(), tp.y()));
                    }
                }
                if (trans->has_geometry_label_point()) {
                    if (cp.name == propGroupLabelPoint) {
                        QPointF lp = pointManager->value(p);
                        // TODO
                        // model->updateGeometry(i, trans->get_source_point(), Cyberiada::Point(tp.x(), tp.y()));
                    }
                }
                if (trans->has_polyline()) {
                    if (cp.name == propGroupPolyline) {
                        Cyberiada::Polyline pl = trans->get_geometry_polyline();
                        int point_index = getPropertyIndex(p);
                        QPointF new_point = pointManager->value(p);
                        pl.at(point_index) = Cyberiada::Point(new_point.x(), new_point.y());
                        model->updateGeometry(i, pl);
                    }
                }

//                 QtProperty* color_prop = constructProperty(propColor);
//                 stringManager->setValue(color_prop, QString(trans->get_color().c_str()));
//                 geom_group_prop->addSubProperty(color_prop);
            }

            } else {
                if (cp.name == propName) {
                    model->updateTitle(i, stringManager->value(p));
                }

            if (type == Cyberiada::elementSimpleState || type == Cyberiada::elementCompositeState) {
                const Cyberiada::State* state = static_cast<const Cyberiada::State*>(element);
                if (state->has_actions() && (cp.name == propActionType || cp.name == propTrigger ||
                                             cp.name == propGuard || cp.name == propBehavior)) {
                    const std::vector<Cyberiada::Action>& actions = state->get_actions();
                    QtProperty* action_property = getPropertyParent(p);
                    int action_index = getPropertyIndex(action_property);
                    const Cyberiada::Action& a = actions.at(action_index);

                    if (cp.name == propActionType) {
                        // TODO
                        // model->updateAction(i, action_index, a.get_trigger(), a.get_guard(), a.get_behavior());
                    }
                    if (a.get_type() == Cyberiada::actionTransition) {
                        if (cp.name == propTrigger) {
                            model->updateAction(i, action_index, stringManager->value(p), a.get_guard().c_str(), a.get_behavior().c_str());
                        }
                        if (cp.name == propGuard) {
                            model->updateAction(i, action_index, a.get_trigger().c_str(), stringManager->value(p), a.get_behavior().c_str());
                        }
                    }
                    if (cp.name == propBehavior) {
                        model->updateAction(i, action_index, a.get_trigger().c_str(), a.get_guard().c_str(), stringManager->value(p));
                    }
                }
            } else if (type == Cyberiada::elementComment || type == Cyberiada::elementFormalComment) {
                const Cyberiada::Comment* comment = static_cast<const Cyberiada::Comment*>(element);

                if (cp.name == propBody) {
                    model->updateCommentBody(i, stringManager->value(p));
                }

                if (cp.name == propMarkup) {
                    // TODO
                }

//                 QtProperty* markup_prop = constructProperty(propMarkup);
//                 stringManager->setValue(markup_prop, QString(comment->get_markup().c_str()));
//                 comment_group_prop->addSubProperty(markup_prop);

                if (comment->has_subjects()) {
                   // TODO
//                    const std::vector<Cyberiada::CommentSubject>& subjects = comment->get_subjects();
//                     for (std::vector<Cyberiada::CommentSubject>::const_iterator i = subjects.begin(); i != subjects.end(); i++) {
//                         const Cyberiada::CommentSubject& cs = *i;

//                         QtProperty* subject_prop = constructProperty(propGroupSubject);
//                         subjects_group_prop->addSubProperty(subject_prop);

//                         QtProperty* cs_type_prop = constructProperty(propSubjectType);
//                         enumManager->setValue(cs_type_prop, cs.get_type());
//                         subject_prop->addSubProperty(cs_type_prop);

//                         QtProperty* cs_target_prop = constructProperty(propTarget);
//                         enumManager->setValue(cs_target_prop, getElementNumber(false, cs.get_element()));
//                         subject_prop->addSubProperty(cs_target_prop);

//                         if (cs.get_type() != Cyberiada::commentSubjectElement) {
//                             QtProperty* fragment_prop = constructProperty(propFragment);
//                             stringManager->setValue(fragment_prop, cs.get_fragment().c_str());
//                             subject_prop->addSubProperty(fragment_prop);
//                         }

//                         if (cs.has_geometry()) {
//                             QtProperty* geom_group_prop = constructProperty(propGroupGeometry);
//                             subject_prop->addSubProperty(geom_group_prop);

//                             if (cs.has_geometry_source_point()) {
//                                 QtProperty* spoint_group_prop = constructProperty(propGroupSourcePoint);
//                                 geom_group_prop->addSubProperty(spoint_group_prop);
//                                 pointManager->setValue(spoint_group_prop, QPointF(cs.get_geometry_source_point().x,
//                                                                                   cs.get_geometry_source_point().y));
//                             }
//                             if (cs.has_geometry_target_point()) {
//                                 QtProperty* tpoint_group_prop = constructProperty(propGroupTargetPoint);
//                                 geom_group_prop->addSubProperty(tpoint_group_prop);
//                                 pointManager->setValue(tpoint_group_prop, QPointF(cs.get_geometry_target_point().x,
//                                                                                   cs.get_geometry_target_point().y));
//                             }
//                             if (cs.has_polyline()) {
//                                 QtProperty* poly_group_prop = constructProperty(propGroupPolyline);
//                                 geom_group_prop->addSubProperty(poly_group_prop);
//                                 const Cyberiada::Polyline& pl = cs.get_geometry_polyline();
//                                 for (Cyberiada::Polyline::const_iterator i = pl.begin(); i != pl.end(); i++) {
//                                     QtProperty* point_prop = constructProperty(propGroupPoint);
//                                     poly_group_prop->addSubProperty(point_prop);
//                                     pointManager->setValue(point_prop, QPointF(i->x, i->y));
//                                 }
//                             }
//                         }
//                     }
                }
            }

            if (element->has_geometry()) {

                if (type == Cyberiada::elementSM ||
                    type == Cyberiada::elementSimpleState || type == Cyberiada::elementCompositeState ||
                    type == Cyberiada::elementComment || type == Cyberiada::elementChoice) {

                    if (cp.name == propGroupRect) {
                        QRectF newRect = rectManager->value(p);
                        model->updateGeometry(i, Cyberiada::Rect(newRect.x(), newRect.y(), newRect.width(), newRect.height()));
                    }

                    if (cp.name == propColor) {
                        // TODO
                        // stringManager->value(p);
                        // model->
                    }
//                     QtProperty* color_prop = constructProperty(propColor);
//                     stringManager->setValue(color_prop, QString(col.c_str()));
//                     geom_group_prop->addSubProperty(color_prop);

                } else if (type == Cyberiada::elementInitial || type == Cyberiada::elementFinal) {
                    if (cp.name == propGroupPoint) {
                        QPointF newPoint = pointManager->value(p);
                        model->updateGeometry(i, Cyberiada::Point(newPoint.x(), newPoint.y()));
                    }
                }
            }
        }
    }
}

void CyberiadaSMPropertiesWidget::newElement(Cyberiada::Element* new_element)
{
	MY_ASSERT(new_element);
    element = new_element;

    updating = true;

	Cyberiada::ElementType type = element->get_type();

	QtProperty* element_group_prop = constructProperty(propGroupElement);
	addProperty(element_group_prop);

	QtProperty* element_type_prop = constructProperty(propType);
	enumManager->setValue(element_type_prop, type);
	element_group_prop->addSubProperty(element_type_prop);
	
	if (type == Cyberiada::elementRoot) {
		const Cyberiada::LocalDocument* doc = model->rootDocument();
		MY_ASSERT(doc);

		QtProperty* doc_group_prop = constructProperty(propGroupDocument);
		addProperty(doc_group_prop);
		
		QtProperty* format_type_prop = constructProperty(propFormat);
		enumManager->setValue(format_type_prop, doc->get_file_format());
		doc_group_prop->addSubProperty(format_type_prop);
		
		QtProperty* meta_group_prop = constructProperty(propGroupMeta);
		doc_group_prop->addSubProperty(meta_group_prop);

        // QtProperty* bounding_group_prop = constructProperty(propGroupBoundingRect);
        // Cyberiada::Rect r = doc->get_bound_rect();
        // rectManager->setValue(bounding_group_prop, QRectF(r.x, r.y, r.width, r.height));
        // doc_group_prop->addSubProperty(bounding_group_prop);

		QtProperty* standard_version_prop = constructProperty(propMetaStandardVersion);
		stringManager->setValue(standard_version_prop, QString(doc->meta().standard_version.c_str()));
		meta_group_prop->addSubProperty(standard_version_prop);

		for (std::vector<std::pair<Cyberiada::String, Cyberiada::String>>::const_iterator i = doc->meta().strings.begin();
			 i != doc->meta().strings.end();
			 i++) {
			QtProperty* platform_string_prop = constructProperty(propMetaString, i->first.c_str());
			stringManager->setValue(platform_string_prop, i->second.c_str());
			meta_group_prop->addSubProperty(platform_string_prop);
		}

		QtProperty* transition_order_prop = constructProperty(propMetaTransitionOrder);
		boolManager->setValue(transition_order_prop, doc->meta().transition_order_flag);
		meta_group_prop->addSubProperty(transition_order_prop);

		QtProperty* event_propagation_prop = constructProperty(propMetaEventPropagation);
		boolManager->setValue(event_propagation_prop, doc->meta().event_propagation_flag);
		meta_group_prop->addSubProperty(event_propagation_prop);
		
	} else {
		QtProperty* element_id_prop = constructProperty(propID);
		stringManager->setValue(element_id_prop, QString(element->get_id().c_str()));
		element_group_prop->addSubProperty(element_id_prop);
	
		if (type == Cyberiada::elementTransition) {
			const Cyberiada::Transition* trans = static_cast<const Cyberiada::Transition*>(element);
			MY_ASSERT(trans);
			QtProperty* trans_group_prop = constructProperty(propGroupTransition);
			addProperty(trans_group_prop);
			
			QtProperty* element_source_prop = constructProperty(propSource);
			enumManager->setValue(element_source_prop, getElementNumber(true,
																		model->idToElement(trans->source_element_id().c_str())));
			trans_group_prop->addSubProperty(element_source_prop);
			
			QtProperty* element_target_prop = constructProperty(propTarget);
			enumManager->setValue(element_target_prop, getElementNumber(false,
																		model->idToElement(trans->target_element_id().c_str())));
			trans_group_prop->addSubProperty(element_target_prop);
			
			QtProperty* action_group_prop = constructProperty(propGroupAction);
			trans_group_prop->addSubProperty(action_group_prop);
			
			QtProperty* trigger_prop = constructProperty(propTrigger);
			stringManager->setValue(trigger_prop, QString(trans->get_action().get_trigger().c_str()));
			action_group_prop->addSubProperty(trigger_prop);
			
			QtProperty* guard_prop = constructProperty(propGuard);
			stringManager->setValue(guard_prop, QString(trans->get_action().get_guard().c_str()));
			action_group_prop->addSubProperty(guard_prop);
			
			QtProperty* behavior_prop = constructProperty(propBehavior);
			stringManager->setValue(behavior_prop, QString(trans->get_action().get_behavior().c_str()));
			action_group_prop->addSubProperty(behavior_prop);

			if (trans->has_geometry()) {
				QtProperty* geom_group_prop = constructProperty(propGroupGeometry);
				addProperty(geom_group_prop);
				
				if (trans->has_geometry_source_point()) {
					QtProperty* spoint_group_prop = constructProperty(propGroupSourcePoint);
					geom_group_prop->addSubProperty(spoint_group_prop);
					pointManager->setValue(spoint_group_prop, QPointF(trans->get_source_point().x,
																	  trans->get_source_point().y));
				}
				if (trans->has_geometry_target_point()) {
					QtProperty* tpoint_group_prop = constructProperty(propGroupTargetPoint);
					geom_group_prop->addSubProperty(tpoint_group_prop);
					pointManager->setValue(tpoint_group_prop, QPointF(trans->get_target_point().x,
																	  trans->get_target_point().y));
				}
				if (trans->has_geometry_label_point()) {
					QtProperty* lpoint_group_prop = constructProperty(propGroupLabelPoint);
					geom_group_prop->addSubProperty(lpoint_group_prop);
					pointManager->setValue(lpoint_group_prop, QPointF(trans->get_label_point().x,
																	  trans->get_label_point().y));
				}
				if (trans->has_polyline()) {
					QtProperty* poly_group_prop = constructProperty(propGroupPolyline);
					geom_group_prop->addSubProperty(poly_group_prop);
					const Cyberiada::Polyline& pl = trans->get_geometry_polyline();
					for (Cyberiada::Polyline::const_iterator i = pl.begin(); i != pl.end(); i++) {
						QtProperty* point_prop = constructProperty(propGroupPoint);
						poly_group_prop->addSubProperty(point_prop);
						pointManager->setValue(point_prop, QPointF(i->x, i->y));
					}
				}

				QtProperty* color_prop = constructProperty(propColor);
				stringManager->setValue(color_prop, QString(trans->get_color().c_str()));
				geom_group_prop->addSubProperty(color_prop);
			}
			
		} else {
			QtProperty* element_name_prop = constructProperty(propName);
			stringManager->setValue(element_name_prop, QString(element->get_name().c_str()));    
			element_group_prop->addSubProperty(element_name_prop);
			
			if (type == Cyberiada::elementSimpleState || type == Cyberiada::elementCompositeState) {
				const Cyberiada::State* state = static_cast<const Cyberiada::State*>(element);
				if (state->has_actions()) {
					QtProperty* actions_group_prop = constructProperty(propGroupActions);
					addProperty(actions_group_prop);	
					const std::vector<Cyberiada::Action>& actions = state->get_actions();
					for (std::vector<Cyberiada::Action>::const_iterator i = actions.begin(); i != actions.end(); i++) {
						const Cyberiada::Action& a = *i;
						
						QtProperty* action_prop = constructProperty(propGroupAction);
						actions_group_prop->addSubProperty(action_prop);
						
						QtProperty* action_type_prop = constructProperty(propActionType);
						enumManager->setValue(action_type_prop, a.get_type());
						action_prop->addSubProperty(action_type_prop);
						
						if (a.get_type() == Cyberiada::actionTransition) {
							QtProperty* trigger_prop = constructProperty(propTrigger);
							stringManager->setValue(trigger_prop, QString(a.get_trigger().c_str()));
							action_prop->addSubProperty(trigger_prop);

							QtProperty* guard_prop = constructProperty(propGuard);
							stringManager->setValue(guard_prop, QString(a.get_guard().c_str()));
							action_prop->addSubProperty(guard_prop);					
						}
												
						QtProperty* behavior_prop = constructProperty(propBehavior);
						stringManager->setValue(behavior_prop, QString(a.get_behavior().c_str()));
						action_prop->addSubProperty(behavior_prop);
					}
				}
			} else if (type == Cyberiada::elementComment || type == Cyberiada::elementFormalComment) {
				const Cyberiada::Comment* comment = static_cast<const Cyberiada::Comment*>(element);
				
				QtProperty* comment_group_prop = constructProperty(propGroupComment);
				addProperty(comment_group_prop);

				QtProperty* body_prop = constructProperty(propBody);
				stringManager->setValue(body_prop, QString(comment->get_body().c_str()));    
				comment_group_prop->addSubProperty(body_prop);

				QtProperty* markup_prop = constructProperty(propMarkup);
				stringManager->setValue(markup_prop, QString(comment->get_markup().c_str()));    
				comment_group_prop->addSubProperty(markup_prop);

				if (comment->has_subjects()) {
					QtProperty* subjects_group_prop = constructProperty(propGroupSubjects);
					addProperty(subjects_group_prop);	
					const std::vector<Cyberiada::CommentSubject>& subjects = comment->get_subjects();
					for (std::vector<Cyberiada::CommentSubject>::const_iterator i = subjects.begin(); i != subjects.end(); i++) {
						const Cyberiada::CommentSubject& cs = *i;

						QtProperty* subject_prop = constructProperty(propGroupSubject);
						subjects_group_prop->addSubProperty(subject_prop);
						
						QtProperty* cs_type_prop = constructProperty(propSubjectType);
						enumManager->setValue(cs_type_prop, cs.get_type());
						subject_prop->addSubProperty(cs_type_prop);

						QtProperty* cs_target_prop = constructProperty(propTarget);
						enumManager->setValue(cs_target_prop, getElementNumber(false, cs.get_element()));
						subject_prop->addSubProperty(cs_target_prop);

						if (cs.get_type() != Cyberiada::commentSubjectElement) {
							QtProperty* fragment_prop = constructProperty(propFragment);
							stringManager->setValue(fragment_prop, cs.get_fragment().c_str());
							subject_prop->addSubProperty(fragment_prop);
						}

						if (cs.has_geometry()) {
							QtProperty* geom_group_prop = constructProperty(propGroupGeometry);
							subject_prop->addSubProperty(geom_group_prop);
				
							if (cs.has_geometry_source_point()) {
								QtProperty* spoint_group_prop = constructProperty(propGroupSourcePoint);
								geom_group_prop->addSubProperty(spoint_group_prop);
								pointManager->setValue(spoint_group_prop, QPointF(cs.get_geometry_source_point().x,
																				  cs.get_geometry_source_point().y));
							}
							if (cs.has_geometry_target_point()) {
								QtProperty* tpoint_group_prop = constructProperty(propGroupTargetPoint);
								geom_group_prop->addSubProperty(tpoint_group_prop);
								pointManager->setValue(tpoint_group_prop, QPointF(cs.get_geometry_target_point().x,
																				  cs.get_geometry_target_point().y));
							}
							if (cs.has_polyline()) {
								QtProperty* poly_group_prop = constructProperty(propGroupPolyline);
								geom_group_prop->addSubProperty(poly_group_prop);
								const Cyberiada::Polyline& pl = cs.get_geometry_polyline();
								for (Cyberiada::Polyline::const_iterator i = pl.begin(); i != pl.end(); i++) {
									QtProperty* point_prop = constructProperty(propGroupPoint);
									poly_group_prop->addSubProperty(point_prop);
									pointManager->setValue(point_prop, QPointF(i->x, i->y));
								}
							}
						}
					}
				}
			}

			if (element->has_geometry()) {
				QtProperty* geom_group_prop = constructProperty(propGroupGeometry);
				addProperty(geom_group_prop);
				
				if (type == Cyberiada::elementSM ||
					type == Cyberiada::elementSimpleState || type == Cyberiada::elementCompositeState ||
					type == Cyberiada::elementComment || type == Cyberiada::elementChoice) {

					Cyberiada::Color col;
					Cyberiada::Rect r;
					if (type == Cyberiada::elementChoice) {
						const Cyberiada::ChoicePseudostate* c = static_cast<const Cyberiada::ChoicePseudostate*>(element);
						r = c->get_geometry_rect();
						col = c->get_color();
					} else if (type == Cyberiada::elementComment) {
						const Cyberiada::Comment* c = static_cast<const Cyberiada::Comment*>(element);
						r = c->get_geometry_rect();
						col = c->get_color();
					} else {
						const Cyberiada::ElementCollection* c = static_cast<const Cyberiada::ElementCollection*>(element);
						r = c->get_geometry_rect();
						col = c->get_color();
					}

					QtProperty* rect_group_prop = constructProperty(propGroupRect);
					geom_group_prop->addSubProperty(rect_group_prop);
                    rectManager->setValue(rect_group_prop, QRectF(r.x, r.y, r.width, r.height));

					QtProperty* color_prop = constructProperty(propColor);
					stringManager->setValue(color_prop, QString(col.c_str()));
					geom_group_prop->addSubProperty(color_prop);
					
				} else if (type == Cyberiada::elementInitial || type == Cyberiada::elementFinal) {
					const Cyberiada::Vertex* v = static_cast<const Cyberiada::Vertex*>(element);
					QtProperty* point_group_prop = constructProperty(propGroupPoint);
					geom_group_prop->addSubProperty(point_group_prop);
					pointManager->setValue(point_group_prop, QPointF(v->get_geometry_point().x,
																	 v->get_geometry_point().y));
				}
			}
		}
	}

    updating = false;
}

void CyberiadaSMPropertiesWidget::updateElement()
{
    updating = true;

    Cyberiada::ElementType type = element->get_type();

    QtProperty* element_group_prop = findQtProperty(nullptr, findPropertyStruct(propGroupElement).propName);
    QtProperty* element_type_prop = findQtProperty(element_group_prop, findPropertyStruct(propType).propName);
    enumManager->setValue(element_type_prop, type);

    if (type == Cyberiada::elementRoot) {
        const Cyberiada::LocalDocument* doc = model->rootDocument();
        MY_ASSERT(doc);

        QtProperty* doc_group_prop = findQtProperty(nullptr, findPropertyStruct(propGroupDocument).propName);

        QtProperty* format_type_prop = findQtProperty(doc_group_prop, findPropertyStruct(propFormat).propName);
        enumManager->setValue(format_type_prop, doc->get_file_format());

        QtProperty* meta_group_prop = findQtProperty(doc_group_prop, findPropertyStruct(propGroupMeta).propName);
        QtProperty* standard_version_prop = findQtProperty(meta_group_prop, findPropertyStruct(propMetaStandardVersion).propName);
        stringManager->setValue(standard_version_prop, QString(doc->meta().standard_version.c_str()));

        // QtProperty* bounding_group_prop = constructProperty(propGroupBoundingRect);
        // Cyberiada::Rect r = doc->get_bound_rect();
        // rectManager->setValue(bounding_group_prop, QRectF(r.x, r.y, r.width, r.height));

        for (std::vector<std::pair<Cyberiada::String, Cyberiada::String>>::const_iterator i = doc->meta().strings.begin();
             i != doc->meta().strings.end();
             i++) {
            QtProperty* platform_string_prop = findQtProperty(meta_group_prop, i->first.c_str());
            stringManager->setValue(platform_string_prop, i->second.c_str());
        }

        QtProperty* transition_order_prop = findQtProperty(meta_group_prop, findPropertyStruct(propMetaTransitionOrder).propName);
        boolManager->setValue(transition_order_prop, doc->meta().transition_order_flag);
        QtProperty* event_propagation_prop = findQtProperty(meta_group_prop, findPropertyStruct(propMetaEventPropagation).propName);
        boolManager->setValue(event_propagation_prop, doc->meta().event_propagation_flag);

    } else {
        QtProperty* element_id_prop = findQtProperty(element_group_prop, findPropertyStruct(propID).propName);
        stringManager->setValue(element_id_prop, QString(element->get_id().c_str()));

        if (type == Cyberiada::elementTransition) {
            const Cyberiada::Transition* trans = static_cast<const Cyberiada::Transition*>(element);
            MY_ASSERT(trans);
            QtProperty* trans_group_prop = findQtProperty(nullptr, findPropertyStruct(propGroupTransition).propName);

            QtProperty* element_source_prop = findQtProperty(trans_group_prop, findPropertyStruct(propSource).propName);
            enumManager->setValue(element_source_prop, getElementNumber(true,
                                                                        model->idToElement(trans->source_element_id().c_str())));

            QtProperty* element_target_prop = findQtProperty(trans_group_prop, findPropertyStruct(propTarget).propName);
            enumManager->setValue(element_target_prop, getElementNumber(false,
                                                                        model->idToElement(trans->target_element_id().c_str())));

            QtProperty* action_group_prop = findQtProperty(trans_group_prop, findPropertyStruct(propGroupAction).propName);

            QtProperty* trigger_prop = findQtProperty(action_group_prop, findPropertyStruct(propTrigger).propName);
            stringManager->setValue(trigger_prop, QString(trans->get_action().get_trigger().c_str()));

            QtProperty* guard_prop = findQtProperty(action_group_prop, findPropertyStruct(propGuard).propName);
            stringManager->setValue(guard_prop, QString(trans->get_action().get_guard().c_str()));

            QtProperty* behavior_prop = findQtProperty(action_group_prop, findPropertyStruct(propBehavior).propName);
            stringManager->setValue(behavior_prop, QString(trans->get_action().get_behavior().c_str()));

            if (trans->has_geometry()) {
                QtProperty* geom_group_prop = findQtProperty(nullptr, findPropertyStruct(propGroupGeometry).propName);
                if (geom_group_prop == nullptr) {
                    geom_group_prop = constructProperty(propGroupGeometry);
                    addProperty(geom_group_prop);
                }

                if (trans->has_geometry_source_point()) {
                    QtProperty* spoint_group_prop = findQtProperty(geom_group_prop, findPropertyStruct(propGroupSourcePoint).propName);
                    if (spoint_group_prop = nullptr) {
                        spoint_group_prop = constructProperty(propGroupSourcePoint);
                        geom_group_prop->addSubProperty(spoint_group_prop);
                    }
                    // TODO always shows QPointF(0, 0)
                    // qDebug() << pointManager->value(spoint_group_prop);
                    pointManager->setValue(spoint_group_prop, QPointF(trans->get_source_point().x,
                                                                      trans->get_source_point().y));
                }
                if (trans->has_geometry_target_point()) {
                    QtProperty* tpoint_group_prop = findQtProperty(geom_group_prop, findPropertyStruct(propGroupTargetPoint).propName);
                    if (tpoint_group_prop == nullptr) {
                        tpoint_group_prop = constructProperty(propGroupTargetPoint);
                        geom_group_prop->addSubProperty(tpoint_group_prop);
                    }
                    // qDebug() << pointManager->value(tpoint_group_prop);
                    pointManager->setValue(tpoint_group_prop, QPointF(trans->get_target_point().x,
                                                                      trans->get_target_point().y));
                }
                if (trans->has_geometry_label_point()) {
                    QtProperty* lpoint_group_prop = findQtProperty(geom_group_prop, findPropertyStruct(propGroupLabelPoint).propName);
                    if (lpoint_group_prop == nullptr) {
                        lpoint_group_prop = constructProperty(propGroupLabelPoint);
                        geom_group_prop->addSubProperty(lpoint_group_prop);
                    }
                    pointManager->setValue(lpoint_group_prop, QPointF(trans->get_label_point().x,
                                                                      trans->get_label_point().y));
                }
                if (trans->has_polyline()) {
                    QtProperty* poly_group_prop = findQtProperty(geom_group_prop, findPropertyStruct(propGroupPolyline).propName);
                    if (poly_group_prop == nullptr) {
                        poly_group_prop = constructProperty(propGroupPolyline);
                        geom_group_prop->addSubProperty(poly_group_prop);
                    }
                    const Cyberiada::Polyline& pl = trans->get_geometry_polyline();
                    int index = 0;
                    for (Cyberiada::Polyline::const_iterator i = pl.begin(); i != pl.end(); i++) {
                        QtProperty* point_prop = findQtProperty(poly_group_prop, findPropertyStruct(propGroupPoint).propName, index);
                        if (point_prop == nullptr) {
                            point_prop = constructProperty(propGroupPoint);
                            poly_group_prop->addSubProperty(point_prop);
                        }
                        pointManager->setValue(point_prop, QPointF(i->x, i->y));
                        index ++;
                    }
                }

                QtProperty* color_prop = findQtProperty(geom_group_prop, findPropertyStruct(propColor).propName);
                if (color_prop == nullptr) {
                    color_prop = constructProperty(propColor);
                    geom_group_prop->addSubProperty(color_prop);
                }
                stringManager->setValue(color_prop, QString(trans->get_color().c_str()));
            }

        } else {
            QtProperty* element_name_prop = findQtProperty(element_group_prop, findPropertyStruct(propName).propName);
            stringManager->setValue(element_name_prop, QString(element->get_name().c_str()));

            if (type == Cyberiada::elementSimpleState || type == Cyberiada::elementCompositeState) {
                const Cyberiada::State* state = static_cast<const Cyberiada::State*>(element);
                if (state->has_actions()) {
                    QtProperty* actions_group_prop = findQtProperty(nullptr, findPropertyStruct(propGroupActions).propName);
                    if (actions_group_prop == nullptr) {
                        actions_group_prop = constructProperty(propGroupActions);
                        addProperty(actions_group_prop);
                    }
                    const std::vector<Cyberiada::Action>& actions = state->get_actions();
                    int index = 0;
                    for (std::vector<Cyberiada::Action>::const_iterator i = actions.begin(); i != actions.end(); i++) {
                        const Cyberiada::Action& a = *i;

                        QtProperty* action_prop = findQtProperty(actions_group_prop, findPropertyStruct(propGroupAction).propName, index);
                        if (action_prop == nullptr) {
                            action_prop = constructProperty(propGroupAction);
                            actions_group_prop->addSubProperty(action_prop);
                        }

                        QtProperty* action_type_prop = findQtProperty(action_prop, findPropertyStruct(propActionType).propName);
                        if (action_type_prop == nullptr) {
                            action_type_prop = constructProperty(propActionType);
                            action_prop->addSubProperty(action_type_prop);
                        }
                        enumManager->setValue(action_type_prop, a.get_type());

                        if (a.get_type() == Cyberiada::actionTransition) {
                            QtProperty* trigger_prop = findQtProperty(action_prop, findPropertyStruct(propTrigger).propName);
                            if (trigger_prop == nullptr) {
                                trigger_prop = constructProperty(propTrigger);
                                action_prop->addSubProperty(trigger_prop);
                            }
                            stringManager->setValue(trigger_prop, QString(a.get_trigger().c_str()));

                            QtProperty* guard_prop = findQtProperty(action_prop, findPropertyStruct(propGuard).propName);
                            if (guard_prop == nullptr) {
                                guard_prop = constructProperty(propGuard);
                                action_prop->addSubProperty(guard_prop);
                            }
                            stringManager->setValue(guard_prop, QString(a.get_guard().c_str()));
                        }

                        QtProperty* behavior_prop = findQtProperty(action_prop, findPropertyStruct(propBehavior).propName);
                        if (behavior_prop == nullptr) {
                            behavior_prop = constructProperty(propBehavior);
                            action_prop->addSubProperty(behavior_prop);
                        }
                        stringManager->setValue(behavior_prop, QString(a.get_behavior().c_str()));

                        index ++;
                    }
                }
            } else if (type == Cyberiada::elementComment || type == Cyberiada::elementFormalComment) {
                const Cyberiada::Comment* comment = static_cast<const Cyberiada::Comment*>(element);

                QtProperty* comment_group_prop = findQtProperty(nullptr, findPropertyStruct(propGroupComment).propName);

                QtProperty* body_prop = findQtProperty(comment_group_prop, findPropertyStruct(propBody).propName);
                stringManager->setValue(body_prop, QString(comment->get_body().c_str()));

                QtProperty* markup_prop = findQtProperty(comment_group_prop, findPropertyStruct(propMarkup).propName);
                stringManager->setValue(markup_prop, QString(comment->get_markup().c_str()));

                if (comment->has_subjects()) {
                    QtProperty* subjects_group_prop = findQtProperty(nullptr, findPropertyStruct(propGroupSubjects).propName);
                    if (subjects_group_prop == nullptr) {
                        subjects_group_prop = constructProperty(propGroupSubjects);
                        addProperty(subjects_group_prop);
                    }
                    const std::vector<Cyberiada::CommentSubject>& subjects = comment->get_subjects();
                    for (std::vector<Cyberiada::CommentSubject>::const_iterator i = subjects.begin(); i != subjects.end(); i++) {
                        const Cyberiada::CommentSubject& cs = *i;

                        QtProperty* subject_prop = findQtProperty(subjects_group_prop, findPropertyStruct(propGroupSubject).propName);
                        if (subject_prop == nullptr) {
                            subject_prop = constructProperty(propGroupSubject);
                            subjects_group_prop->addSubProperty(subject_prop);
                        }

                        QtProperty* cs_type_prop = findQtProperty(subject_prop, findPropertyStruct(propSubjectType).propName);
                        if (cs_type_prop == nullptr) {
                            cs_type_prop = constructProperty(propSubjectType);
                            subject_prop->addSubProperty(cs_type_prop);
                        }
                        enumManager->setValue(cs_type_prop, cs.get_type());

                        QtProperty* cs_target_prop = findQtProperty(subject_prop, findPropertyStruct(propTarget).propName);
                        if (cs_target_prop == nullptr) {
                            cs_target_prop = constructProperty(propTarget);
                            subject_prop->addSubProperty(cs_target_prop);
                        }
                        enumManager->setValue(cs_target_prop, getElementNumber(false, cs.get_element()));

                        if (cs.get_type() != Cyberiada::commentSubjectElement) {
                            QtProperty* fragment_prop = findQtProperty(subject_prop, findPropertyStruct(propFragment).propName);
                            if (fragment_prop == nullptr) {
                                fragment_prop = constructProperty(propFragment);
                                subject_prop->addSubProperty(fragment_prop);
                            }
                            stringManager->setValue(fragment_prop, cs.get_fragment().c_str());
                        }

                        if (cs.has_geometry()) {
                            QtProperty* geom_group_prop = findQtProperty(subject_prop, findPropertyStruct(propGroupGeometry).propName);
                            if (geom_group_prop == nullptr) {
                                geom_group_prop = constructProperty(propGroupGeometry);
                                subject_prop->addSubProperty(geom_group_prop);
                            }

                            if (cs.has_geometry_source_point()) {
                                QtProperty* spoint_group_prop = findQtProperty(geom_group_prop, findPropertyStruct(propGroupSourcePoint).propName);
                                if (spoint_group_prop == nullptr) {
                                    spoint_group_prop = constructProperty(propGroupSourcePoint);
                                    geom_group_prop->addSubProperty(spoint_group_prop);
                                }
                                pointManager->setValue(spoint_group_prop, QPointF(cs.get_geometry_source_point().x,
                                                                                  cs.get_geometry_source_point().y));
                            }
                            if (cs.has_geometry_target_point()) {
                                QtProperty* tpoint_group_prop = findQtProperty(geom_group_prop, findPropertyStruct(propGroupTargetPoint).propName);
                                if (tpoint_group_prop == nullptr) {
                                    tpoint_group_prop = constructProperty(propGroupTargetPoint);
                                    geom_group_prop->addSubProperty(tpoint_group_prop);
                                }
                                pointManager->setValue(tpoint_group_prop, QPointF(cs.get_geometry_target_point().x,
                                                                                  cs.get_geometry_target_point().y));
                            }
                            if (cs.has_polyline()) {
                                QtProperty* poly_group_prop = findQtProperty(geom_group_prop, findPropertyStruct(propGroupPolyline).propName);
                                if (poly_group_prop == nullptr) {
                                    poly_group_prop = constructProperty(propGroupPolyline);
                                    geom_group_prop->addSubProperty(poly_group_prop);
                                }
                                const Cyberiada::Polyline& pl = cs.get_geometry_polyline();
                                int index = 0;
                                for (Cyberiada::Polyline::const_iterator i = pl.begin(); i != pl.end(); i++) {
                                    QtProperty* point_prop = findQtProperty(poly_group_prop, findPropertyStruct(propGroupPoint).propName, index);
                                    if (point_prop == nullptr) {
                                        point_prop = constructProperty(propGroupPoint);
                                        poly_group_prop->addSubProperty(point_prop);
                                    }
                                    pointManager->setValue(point_prop, QPointF(i->x, i->y));
                                    index ++;
                                }
                            }
                        }
                    }
                }
            }

            if (element->has_geometry()) {
                QtProperty* geom_group_prop = findQtProperty(nullptr, findPropertyStruct(propGroupGeometry).propName);
                if (geom_group_prop == nullptr) {
                    geom_group_prop = constructProperty(propGroupGeometry);
                    addProperty(geom_group_prop);
                }

                if (type == Cyberiada::elementSM ||
                    type == Cyberiada::elementSimpleState || type == Cyberiada::elementCompositeState ||
                    type == Cyberiada::elementComment || type == Cyberiada::elementChoice) {

                    Cyberiada::Color col;
                    Cyberiada::Rect r;
                    if (type == Cyberiada::elementChoice) {
                        const Cyberiada::ChoicePseudostate* c = static_cast<const Cyberiada::ChoicePseudostate*>(element);
                        r = c->get_geometry_rect();
                        col = c->get_color();
                    } else if (type == Cyberiada::elementComment) {
                        const Cyberiada::Comment* c = static_cast<const Cyberiada::Comment*>(element);
                        r = c->get_geometry_rect();
                        col = c->get_color();
                    } else {
                        const Cyberiada::ElementCollection* c = static_cast<const Cyberiada::ElementCollection*>(element);
                        r = c->get_geometry_rect();
                        col = c->get_color();
                    }

                    QtProperty* rect_group_prop = findQtProperty(geom_group_prop, findPropertyStruct(propGroupRect).propName);
                    if (rect_group_prop == nullptr) {
                        rect_group_prop = constructProperty(propGroupRect);
                        geom_group_prop->addSubProperty(rect_group_prop);
                    }
                    rectManager->setValue(rect_group_prop, QRectF(r.x, r.y, r.width, r.height));

                    QtProperty* color_prop = findQtProperty(geom_group_prop, findPropertyStruct(propColor).propName);
                    if (color_prop == nullptr) {
                        color_prop = constructProperty(propColor);
                        geom_group_prop->addSubProperty(color_prop);
                    }
                    stringManager->setValue(color_prop, QString(col.c_str()));

                } else if (type == Cyberiada::elementInitial || type == Cyberiada::elementFinal) {
                    const Cyberiada::Vertex* v = static_cast<const Cyberiada::Vertex*>(element);
                    QtProperty* point_group_prop = findQtProperty(geom_group_prop, findPropertyStruct(propGroupPoint).propName);
                    if (point_group_prop == nullptr) {
                        point_group_prop = constructProperty(propGroupPoint);
                        geom_group_prop->addSubProperty(point_group_prop);
                    }
                    pointManager->setValue(point_group_prop, QPointF(v->get_geometry_point().x,
                                                                     v->get_geometry_point().y));
                }
            }
        }
    }

    updating = false;
}
	
QtProperty* CyberiadaSMPropertiesWidget::constructProperty(CyberiadaPropertyName prop, const QString& alt_name)
{
	MY_ASSERT(element);
	Cyberiada::ElementType type = element->get_type();
	
	const CyberiadaProperty& p = findPropertyStruct(prop);
	
	QtProperty* new_property = NULL;
	
	switch(p.editor) {
	case propEditorActionType:
		new_property = enumManager->addProperty(p.propName);
		enumManager->setEnumNames(new_property, actionTypesEnumNames);		
		enumManager->setEnumIcons(new_property, actionTypesEnumIcons);
		break;
	case propEditorColor:
		new_property = stringManager->addProperty(p.propName);
		break;
	case propEditorDate:
		new_property = dateManager->addProperty(p.propName);
		break;
	case propEditorElementType:
		new_property = enumManager->addProperty(p.propName);
		enumManager->setEnumNames(new_property, elementTypesEnumNames);		
		enumManager->setEnumIcons(new_property, elementTypesEnumIcons);
		break;
	case propEditorFlag:
		new_property = boolManager->addProperty(p.propName);
		break;
	case propEditorFormatType:
		new_property = enumManager->addProperty(p.propName);
		enumManager->setEnumNames(new_property, formatTypesEnumNames);		
		enumManager->setEnumIcons(new_property, formatTypesEnumIcons);		
		break;
	case propEditorGroup:
		new_property = groupManager->addProperty(p.propName);
		break;
	case propEditorPointGroup:
		new_property = pointManager->addProperty(p.propName);
		break;
	case propEditorRectGroup:
		new_property = rectManager->addProperty(p.propName);
		break;
	case propEditorSourceElementLink:
		new_property = enumManager->addProperty(p.propName);
		enumManager->setEnumNames(new_property, generateElementNames(true));		
		enumManager->setEnumIcons(new_property, generateElementIcons(true));
		break;
		break;
	case propEditorString:
		if (alt_name.isEmpty()) {
			new_property = stringManager->addProperty(p.propName);
		} else {
			new_property = stringManager->addProperty(alt_name);
		}
		break;
	case propEditorSubjectType:
		new_property = enumManager->addProperty(p.propName);
		enumManager->setEnumNames(new_property, subjectTypesEnumNames);		
		enumManager->setEnumIcons(new_property, subjectTypesEnumIcons);
		break;
	case propEditorTargetElementLink:
		new_property = enumManager->addProperty(p.propName);
		enumManager->setEnumNames(new_property, generateElementNames(false));		
		enumManager->setEnumIcons(new_property, generateElementIcons(false));
		break;
	default:
		MY_ASSERT(false);
	}

	return new_property;
}

CyberiadaSMPropertiesWidget::CyberiadaProperty& CyberiadaSMPropertiesWidget::findPropertyStruct(CyberiadaPropertyName prop)
{
    for(QVector<CyberiadaSMPropertiesWidget::CyberiadaProperty>::iterator i = cProperties.begin(); i != cProperties.end(); i++) {
		if (i->name == prop) {
			return *i;
		}
	}
    return cProperties.first();
}

CyberiadaSMPropertiesWidget::CyberiadaProperty& CyberiadaSMPropertiesWidget::findPropertyStruct(const QString& prop_name,
																								const QString& alt_name)
{
    for(QVector<CyberiadaSMPropertiesWidget::CyberiadaProperty>::iterator i = cProperties.begin(); i != cProperties.end(); i++) {
		if (alt_name.isEmpty() && i->propName == prop_name ||
			i->propName == alt_name) {
			return *i;
		}
	}
    return cProperties.first();
}

CyberiadaSMPropertiesWidget::CyberiadaProperty& CyberiadaSMPropertiesWidget::findProperty(const QtProperty* p)
	
{
    for(QVector<CyberiadaSMPropertiesWidget::CyberiadaProperty>::iterator i = cProperties.begin(); i != cProperties.end(); i++) {
		if (i->propName == p->propertyName()) {
			return *i;
		}
	}
    return cProperties.first();
}

QtProperty* CyberiadaSMPropertiesWidget::findQtProperty(QtProperty* root, const QString& prop_name,
                                                        int index) {
    if (prop_name.isEmpty())
        return nullptr;
    if (index < 0)
        return nullptr;

    int count = 0;
    if (root == nullptr) {
        for (QtProperty* prop : properties()) {
            if (prop->propertyName() == prop_name) {
                if (count == index) return prop;
                count ++;
            }
        }
        return nullptr;
    }

    for (QtProperty* child : root->subProperties()) {
        if (child->propertyName() == prop_name) {
            if (count == index) {
                return child;
            }
            count ++;
        }
    }
    return nullptr;
}

QtProperty *CyberiadaSMPropertiesWidget::getPropertyParent(QtProperty *property)
{
    if (!property)
        return nullptr;

    const auto browserItems = items(property);
    if (browserItems.isEmpty()) return nullptr;

    QtBrowserItem* item = browserItems.first();
    QtBrowserItem* parentItem = item->parent();

    if (parentItem) return parentItem->property();

    return nullptr;
}

int CyberiadaSMPropertiesWidget::getPropertyIndex(QtProperty* property) {

    QList<QtProperty*> siblings;
    QtProperty* parent = getPropertyParent(property);
    if (parent) {
        siblings = parent->subProperties();
    } else {
        siblings = properties();
    }

    QString name = property->propertyName();
    int counter = 0;

    for (QtProperty* p : siblings) {
        if (p->propertyName() == name) {
            if (p == property) {
                return counter;
            }
            counter++;
        }
    }

    return -1;
}

Cyberiada::ConstElementList CyberiadaSMPropertiesWidget::getAllElements(bool source) const
{
	MY_ASSERT(model);
	const Cyberiada::Document* doc = model->rootDocument();
	MY_ASSERT(doc);
	const Cyberiada::StateMachine* sm = doc->get_parent_sm(element);
	MY_ASSERT(sm);
	if (source) {
		return sm->find_elements_by_types({Cyberiada::elementSimpleState,
										   Cyberiada::elementCompositeState,
										   Cyberiada::elementInitial,
										   Cyberiada::elementChoice});
	} else {
		return sm->find_elements_by_types({Cyberiada::elementSimpleState,
										   Cyberiada::elementCompositeState,
										   Cyberiada::elementFinal,
										   Cyberiada::elementChoice,
										   Cyberiada::elementTerminate});
	}
}

QStringList CyberiadaSMPropertiesWidget::generateElementNames(bool source) const
{
	Cyberiada::ConstElementList elements = getAllElements(source);
	QStringList result;
	for (Cyberiada::ConstElementList::const_iterator i = elements.begin(); i != elements.end(); i++) {
		const Cyberiada::Element* e = *i;
		MY_ASSERT(e);
		QString name = e->get_name().c_str();
		if (name.isEmpty()) {
			name = QString("[") + e->get_id().c_str() + "]";
		}
		result << name;
	}
	return result;
}

QMap<int, QIcon> CyberiadaSMPropertiesWidget::generateElementIcons(bool source) const
{
	Cyberiada::ConstElementList elements = getAllElements(source);
	QMap<int, QIcon> result;
	int index = 0;
	for (Cyberiada::ConstElementList::const_iterator i = elements.begin(); i != elements.end(); i++, index++) {
		const Cyberiada::Element* e = *i;
		MY_ASSERT(e);
		result[index] = model->getElementIcon(e->get_type());
	}
	return result;
}

int CyberiadaSMPropertiesWidget::getElementNumber(bool source, const Cyberiada::Element* elem) const
{
	MY_ASSERT(elem);
	Cyberiada::ConstElementList elements = getAllElements(source);
	int index = 0;
	for (Cyberiada::ConstElementList::const_iterator i = elements.begin(); i != elements.end(); i++, index++) {
		const Cyberiada::Element* e = *i;
		MY_ASSERT(e);
		if (e == elem) return index;
	}
	MY_ASSERT(false);
	return -1;	
}

const Cyberiada::Element *CyberiadaSMPropertiesWidget::getElementByNumber(bool source, int index) const
{
    Cyberiada::ConstElementList elements = getAllElements(source);
    if (index < 0 || index >= elements.size()) return nullptr;
    return elements.at(index);
}
