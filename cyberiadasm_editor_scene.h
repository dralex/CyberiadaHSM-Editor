/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Editor Scene
 *
 * Copyright (C) 2024 Alexey Fedoseev <aleksey@fedoseev.net>
 *
 * Based on the Qt Visual Graph Editor (QVGE)
 * Copyright (C) 2016-2021 Ars L. Masiuk's <ars.masiuk@gmail.com>
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

#ifndef CYBERIADA_SM_EDITOR_SCENE_HEADER
#define CYBERIADA_SM_EDITOR_SCENE_HEADER

#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QSet>
#include <QMenu>
#include <QByteArrayList>
#include <QList>
#include <QGraphicsItem>
#include <QMultiMap>
#include <QDebug>
#include <string>

#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_items.h"
#include "cyberiadasm_editor_state_item.h"
#include "cyberiadasm_editor_transition_item.h"
#include "cyberiada_constants.h"
#include "code_highlighter.h"

class CyberiadaSMEditorSubjectItem;
class EditableTextItem;

class CyberiadaSMEditorScene: public QGraphicsScene {
Q_OBJECT

public:
    CyberiadaSMEditorScene(CyberiadaSMModel* model, QObject *parent = NULL);
    virtual ~CyberiadaSMEditorScene();

    void  reset();
	
    // void  setGridSize(int newSize);
    // int   getGridSize() const { return gridSize; }

    // bool  isGridEnabled() const	{ return gridEnabled; }
    // bool  isGridSnapEnabled() const { return gridSnap; }

    void  setGridPen(const QPen& gridPen);
    const QPen& getGridPen() const { return gridPen; }

    // fit: the view follows the whole scene (a fresh document); a rebuild
    // after an undo keeps the view where it is
    void  loadScene(bool fit = true);

    QMap<Cyberiada::ID, QGraphicsItem*>& getMap() { return elementIdToItemMap; }
    // the drawn comment links of a comment in the subject order (EDIT-EDGE-6/17-20)
    QList<CyberiadaSMEditorSubjectItem*> subjectLinks(const Cyberiada::ID& comment) const;
    // the ids of the selected elements in the document order (${selection}, EDIT-PLUG-9)
    QStringList selectedIds() const;
    // the graphics items in the scene that no live owner tracks: a child item not in
    // its owner's ownedChildren(), or a stray top-level item (a handle left behind by a
    // rebuild). Empty when the scene graph is consistent (EDIT-UI-8). Read by the
    // polygon via the `== audit` dump section (--dump-audit)
    QStringList auditOrphans() const;

    // Comment the Selection (EDIT-TOOL-17): the fragment the selection of a text being
    // edited comments - the target, the kind and the text - when it lies in one field
    bool  selectionFragment(const EditableTextItem* text, Cyberiada::Element** target,
                            Cyberiada::CommentSubjectType* type, QString* fragment) const;
    // the drawn comments of the text's machine that may comment it
    QList<Cyberiada::Comment*> selectionComments(const EditableTextItem* text) const;
    // end the edit and link the comment, or a new one placed beside the target (an
    // empty id), to the selected fragment in one undo step
    bool  commentSelection(EditableTextItem* text, const Cyberiada::ID& comment);
    // the same once the selection is read: the menu reads it before the edit ends
    bool  commentFragment(Cyberiada::Element* target, Cyberiada::CommentSubjectType type,
                          const QString& fragment, const Cyberiada::ID& comment);

    // the union of the visible items only (unlike itemsBoundingRect, which
    // counts hidden dots and text)
    QRectF visibleItemsBoundingRect() const;
    // the visible diagram with the scene margin around it
    QRectF diagramRect() const;
    // keep the scene rect around the content (padded), so the view can reach and
    // frame the objects however far they sit from the origin
    void updateSceneRect();
    // convert any point labels to rect labels before an interactive save (edit mode)
    void migrateLabelsToRect();

    // true while a model reset takes the editing focus: the focus-out of a text
    // item discards its unfinished edit (the document is about to be freed)
    static bool discardsEdits(const QGraphicsItem* item);

    void  setCurrentTool(ToolType tool);
    ToolType getCurrentTool() { return currentTool; }
    // the document is inspected or locked by a plugin run (EDIT-PLUG-41)
    bool  readOnly() const { return model && model->readOnly(); }
    // the elements a plugin session marks, apart from the selection (EDIT-PLUG-40);
    // an id the scene does not show and a state machine are left out
    void  setRunMarks(const QStringList& active, const QStringList& fired);
    const QStringList& runActive() const { return runMarkedActive; }
    const QStringList& runFired() const { return runMarkedFired; }
    // a tool for one gesture: the select tool is back with the mouse release
    void  beginTransientTool(ToolType tool);

    void addSMItem(Cyberiada::ElementType type);
    CyberiadaSMEditorTransitionItem* addTransition(CyberiadaSMEditorAbstractItem* source, CyberiadaSMEditorAbstractItem* target);
    // draw a transition with a rubber-band line, creating it on release over a
    // valid target (used by sources that cannot seed a self-loop)
    void beginTransitionDraw(CyberiadaSMEditorAbstractItem* source, const QPointF& press);
    // re-route the bare self-loops of a state, or of all states (EDIT-EDGE-22)
    void refreshLoops(const Cyberiada::ID& stateId = Cyberiada::ID());

    // the scene rect of the state machine whose content changed most recently
    // (for the zoom-to-SM action); a null rect if none is known
    QRectF recentlyModifiedSMRect() const;

    // the platformLanguage of the document (EDIT-TEXT-7): the raw value and
    // its rules, null when empty or unknown
    QString codeLanguageName() const { return codeLanguageValue; }
    const CodeLanguage* codeLanguage() const { return codeLanguagePtr; }
    // the markupLanguage of the document (EDIT-TEXT-11): the raw value and its markup
    QString markupLanguageName() const { return markupLanguageValue; }
    CodeMarkup markupLanguage() const { return CodeStyle::findMarkup(markupLanguageValue); }

signals:
    void  toolChanged(ToolType tool);
    // the canvas selection, for the structure tree (the scene knows no window)
    void  elementSelected(const QModelIndex& index);
    void  codeLanguageChanged(const QString& value);
    void  markupLanguageChanged(const QString& value);

public slots:
	void  slotElementSelected(const QModelIndex& index);
    void  slotModelDataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight);
    void  slotRowsInserted(const QModelIndex& parent, int first, int last);
    void  slotRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void  slotRowsRemoved(const QModelIndex& parent, int first, int last);
    void  slotSMSizeChanged(CyberiadaSMEditorAbstractItem::CornerFlags side, qreal d);
	
    // void  enableGrid(bool on = true);
    // void  enableGridSnap(bool on = true);
    void  slotGridSettingsChanged();
    void  slotServiceObjectsChanged();
    // the CGML_COMPONENT comment items follow the visibility preference (EDIT-META-6)
    void  slotComponentCommentsChanged(bool show);
    void  slotSelectionChanged();
    void  slotModelAboutToBeReset();
    void  slotModelReset();
    // re-path the links: a geometry change or a change of a drawn text (a fragment
    // underline, EDIT-EDGE-21)
    void  updateSubjectLinks();

protected:
    void  drawBackground(QPainter *painter, const QRectF &);
    void  mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void  mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void  mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void  mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    // the highlighting follows the platformLanguage and markupLanguage parameters
    void updateTextLanguages();
    void  addItemsRecursively(QGraphicsItem* parent, Cyberiada::ElementCollection* element);
    void addSceneItem(QGraphicsItem* item);
    QGraphicsItem* addElementItem(Cyberiada::Element* element, QGraphicsItem* parent_item);
    QGraphicsItem* graphicsParentFor(const Cyberiada::Element* parent);
    QPointF freePlace(const Cyberiada::ElementCollection* parent, const QSizeF& size);
    // the centre of the attached view's viewport in scene coordinates, or the
    // scene-rect centre when no view is attached (batch/headless)
    QPointF viewportCentre() const;
    // a centre for a new state-machine border that overlaps no existing machine
    QPointF freeStateMachinePlace(const QSizeF& size);
    // grow a bordered state machine (symmetrically, keeping its centre) so a
    // newly added child fits inside it
    void extendStateMachineForChild(CyberiadaSMEditorAbstractItem* smItem, QGraphicsItem* child);
    void  removeItemsForElement(Cyberiada::Element* element);
    // show/hide the transition source boxes on all items when the tool changes
    void  refreshToolDecorations();
    // an item about to be deleted must not stay the mouse grabber or the focus
    // item: Qt keeps a raw pointer and would deliver the pending event to the
    // freed object (a use-after-free crash on a mid-gesture model reset)
    void  releaseInputFor(QGraphicsItem* item);
    void  updateItemsRecursively(CyberiadaSMEditorAbstractItem* parent, Cyberiada::ElementCollection* element);
    // the links are few: any structural change rebuilds them all after the element
    // items, a geometry change only re-paths them; the selected links stay selected
    void  rebuildSubjectLinks();
    void  clearSubjectLinks();
    void  addSubjectLinks(Cyberiada::Element* element);
    // the links of a comment no longer match its drawable subjects (a subject added,
    // removed or an end gone): a retarget or a point change keeps the items
    bool  subjectLinksStale(const Cyberiada::Comment* comment) const;


    CyberiadaSMModel*              model;
	Cyberiada::StateMachine*       currentSM;
    QString                        codeLanguageValue;
    const CodeLanguage*            codeLanguagePtr = nullptr;
    QString                        markupLanguageValue;
    QMap<Cyberiada::ID, QGraphicsItem*> elementIdToItemMap;
    QStringList                    runMarkedActive;
    QStringList                    runMarkedFired;
    QMultiMap<Cyberiada::ID, CyberiadaSMEditorSubjectItem*> subjectItems;
	
    // int                            gridSize;
    // bool                           gridEnabled;
    // bool                           gridSnap;
    QPen                           gridPen;

    ToolType currentTool = ToolType::Select;
    bool transientTool = false;
    // the id of the state machine whose content changed most recently
    Cyberiada::ID lastModifiedSM;
    void noteModified(const QModelIndex& index);

    // element creation by a drawing/placement tool
    bool             creating = false;
    QPointF          creationStart;
    QGraphicsRectItem* creationPreview = nullptr;
    // the transition rubber-band draw (from a pseudostate or the choice)
    QGraphicsLineItem* transitionDrawLine = nullptr;
    CyberiadaSMEditorAbstractItem* transitionSource = nullptr;
    QPointF transitionPress;
    void  finishTransitionDraw(const QPointF& scenePos, Qt::KeyboardModifiers mods);
    // the transition tool drawn from a comment links it to the node, the comment or
    // the transition line under the release, the ends stored (EDIT-TOOL-15, EDIT-EDGE-18)
    void  finishSubjectDraw(CyberiadaSMEditorAbstractItem* comment, const QPointF& scenePos);
    bool  handleCreationPress(QGraphicsSceneMouseEvent* event);
    void  handleCreationMove(QGraphicsSceneMouseEvent* event);
    void  handleCreationRelease(QGraphicsSceneMouseEvent* event);
    // build the element for the active creation tool from a drawn scene rect
    // (a click passes a null-size rect and a default size is used)
    void  createByTool(ToolType tool, const QRectF& sceneRect);
    // the selection to bring back after a snapshot restore
    QList<Cyberiada::ID> selectedBeforeReset;
    bool discardingEdits = false;

    // the session logger: a left-button gesture is recorded as press/drag/
    // release, its intermediate moves decimated to a few pixels
    bool    loggingPressed = false;
    QPointF loggingLastPoint;
};

#endif
