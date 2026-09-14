/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 * 
 * The State Machine Model
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

#ifndef CYBERIADA_SM_MODEL_HEADER
#define CYBERIADA_SM_MODEL_HEADER

#include <QAbstractItemModel>
#include <QUndoStack>
#include <QIcon>
#include <QDateTime>
#include <cyberiada/cyberiadamlpp.h>

class CyberiadaSMModel: public QAbstractItemModel {
Q_OBJECT

public:
	CyberiadaSMModel(QObject *parent);
	~CyberiadaSMModel();

	// CORE FUNCTIONALITY
	void                                reset();
    // void                                createDocument();
	bool                                loadDocument(const QString& path, bool reconstruct = false,
	                                                 bool reconstruct_sm = false, bool strict = false);
	const QString&                      loadError() const { return lastLoadError; }
	void                                saveDocument(bool round = false);
	void                                saveAsDocument(const QString& path, Cyberiada::DocumentFormat f,
	                                                   bool round, bool skip_geometry,
	                                                   bool check_initial, bool strict_actions,
	                                                   bool skip_empty_behavior);
	// the inspected document is never modified
	bool                                readOnly() const;
	void                                saveAsDocument(const QString& path, Cyberiada::DocumentFormat f, bool round = false);

	// DATA REPRESENTATION
	QVariant                            data(const QModelIndex& index, int role) const;	
	Qt::ItemFlags                       flags(const QModelIndex& index) const;
	bool                                hasIndex(int row, int column, const QModelIndex& parent = QModelIndex()) const;
	QModelIndex                         index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
	QModelIndex                         parent(const QModelIndex& index) const;
	int                                 rowCount(const QModelIndex& parent = QModelIndex()) const;
	int                                 columnCount(const QModelIndex& parent = QModelIndex()) const;
	bool                                hasChildren(const QModelIndex& parent = QModelIndex()) const;
	QIcon                               getIndexIcon(const QModelIndex& index) const;
	QIcon                               getElementIcon(Cyberiada::ElementType type) const;
	
	// EDITING
	bool                                setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
	bool                                updateID(const QModelIndex& index, const QString& new_value);
	bool                                updateTitle(const QModelIndex& index, const QString& new_value);
	bool                                updateAction(const QModelIndex& index,
													 int action_index = -1,
                                                     const QString& new_trigger = QString(),
                                                     const QString& new_guard = QString(),
                                                     const QString& new_behaviour = QString());
	bool                                newAction(const QModelIndex& index,
                                                  Cyberiada::ActionType type,
												  const QString& trigger = QString(),
												  const QString& guard = QString(),
												  const QString& behaviour = QString());
	bool                                deleteAction(const QModelIndex& index, int action_index = -1);
	bool                                updateGeometry(const QModelIndex& index, const Cyberiada::Point& point, bool record = true);
	bool                                updateGeometry(const QModelIndex& index, const Cyberiada::Rect& rect, bool record = true);
	bool                                updateGeometry(const QModelIndex& index, const Cyberiada::Point& source, const Cyberiada::Point& target);
    bool                                updateGeometry(const QModelIndex& index, const Cyberiada::Polyline& pl);
    bool                                updateGeometry(const QModelIndex& index, const Cyberiada::ID& source, const Cyberiada::ID& target);
    // the transition label position; an invalid point resets it to auto-placement
    bool                                updateLabel(const QModelIndex& index, const Cyberiada::Point& label_point);
    bool                                updateParent(const QModelIndex& index, const Cyberiada::ID& new_parent_id);
    // grow the parent collection of a moved element so it contains all its rect
    // children (a programmatic move leaves a child outside, unlike an interactive
    // drag). directional: extend only the pushed edges and re-base the siblings to
    // hold their absolute place (paste/move); otherwise grow about the centre so
    // the existing children keep their stored positions (create).
    bool                                growToFitChildren(Cyberiada::Element* moved, bool directional = true);
    // the half-extent (from the collection centre, each axis) its children need;
    // the collection's own rect is excluded, so it is the bare content a border
    // must contain - the shared floor for resize clamps and auto-grow
    void                                childrenHalfExtent(const Cyberiada::ElementCollection* pc,
                                                           double& halfW, double& halfH) const;
    void                                childrenExtent(const Cyberiada::ElementCollection* pc,
                                                       double& left, double& right,
                                                       double& top, double& bottom) const;
    void                                rebaseChildren(Cyberiada::ElementCollection* pc, double dx, double dy);
	bool                                updateCommentBody(const QModelIndex& index, const QString& body);
    bool                                updateMetainformation(const QModelIndex& index, const QString& parameter, const QString& new_value);

    // a default name unique among the sibling states of the parent
    Cyberiada::Name                     uniqueStateName(const Cyberiada::ElementCollection* parent,
                                                        const Cyberiada::Name& base) const;
    Cyberiada::StateMachine*            newStateMachine(const Cyberiada::String& sm_name, const Cyberiada::Rect& r = Cyberiada::Rect());
    Cyberiada::State*                   newState(Cyberiada::ElementCollection* parent, const Cyberiada::String& state_name,
                                                 const Cyberiada::Action& a = Cyberiada::Action(), const Cyberiada::Rect& r = Cyberiada::Rect(),
                                                 const Cyberiada::Rect& region = Cyberiada::Rect(),
                                                 const Cyberiada::Color& color = Cyberiada::Color());
    Cyberiada::InitialPseudostate*      newInitial(Cyberiada::ElementCollection* parent, const Cyberiada::Point& p = Cyberiada::Point());
    Cyberiada::FinalState*              newFinal(Cyberiada::ElementCollection* parent, const Cyberiada::Point& p = Cyberiada::Point());
    Cyberiada::ChoicePseudostate*       newChoice(Cyberiada::ElementCollection* parent, const Cyberiada::Rect& r = Cyberiada::Rect(),
                                                  const Cyberiada::Color& color = Cyberiada::Color());
    Cyberiada::TerminatePseudostate*    newTerminate(Cyberiada::ElementCollection* parent, const Cyberiada::Point& p = Cyberiada::Point());
    Cyberiada::Transition*              newTransition(Cyberiada::StateMachine* sm, Cyberiada::TransitionType ttype,
                                                      Cyberiada::Element* source, Cyberiada::Element* target,
                                                      const Cyberiada::Action& action, const Cyberiada::Polyline& pl = Cyberiada::Polyline(),
                                                      const Cyberiada::Point& sp = Cyberiada::Point(),
                                                      const Cyberiada::Point& tp = Cyberiada::Point(),
                                                      const Cyberiada::Point& label_point = Cyberiada::Point(),
                                                      const Cyberiada::Rect& label_rect = Cyberiada::Rect(),
                                                      const Cyberiada::Color& color = Cyberiada::Color());
    Cyberiada::Comment*                 newComment(Cyberiada::ElementCollection* parent, const Cyberiada::String& body,
                                                   const Cyberiada::Rect& rect = Cyberiada::Rect(),
                                                   const Cyberiada::Color& color = Cyberiada::Color(),
                                                   const Cyberiada::String& markup = Cyberiada::String());
    Cyberiada::Comment*                 newFormalComment(Cyberiada::ElementCollection* parent, const Cyberiada::String& body,
                                                   const Cyberiada::Rect& rect = Cyberiada::Rect(),
                                                   const Cyberiada::Color& color = Cyberiada::Color(),
                                                   const Cyberiada::String& markup = Cyberiada::String());
    // deep-copy src into parent as a sibling: fresh ids, a unique name, remapped
    // internal transitions, and points shifted a little (for cut/copy/paste). The
    // returned element is the pasted copy, or NULL. A State Machine is never pasted.
    Cyberiada::Element*                 pasteElement(Cyberiada::ElementCollection* parent,
                                                     const Cyberiada::Element* src);

    bool                                newCommentSubject(const QModelIndex& index, Cyberiada::Element* target,
                                                          Cyberiada::CommentSubjectType type, const QString& fragment);
    bool                                deleteCommentSubject(const QModelIndex& index, int subject_index);

    bool                                deleteElement(const QModelIndex& index);

	// DRAG & DROP
	Qt::DropActions                     supportedDropActions() const;
	bool                                dropMimeData(const QMimeData *data,
													 Qt::DropAction action, int row, int column, const QModelIndex &parent);
	QMimeData*                          mimeData(const QModelIndexList &indexes) const;
	QStringList                         mimeTypes() const;

	// INDEXES
	QModelIndex                         rootIndex() const;
	QModelIndex                         documentIndex() const;
	QModelIndex                         firstSMIndex() const;
	QModelIndex                         elementToIndex(const Cyberiada::Element* element) const;
	bool                                isSMIndex(const QModelIndex& index) const;
	bool                                isInitialIndex(const QModelIndex& index) const;
	bool                                isStateIndex(const QModelIndex& index) const;
	bool                                isSimpleStateIndex(const QModelIndex& index) const;
	bool                                isCompositeStateIndex(const QModelIndex& index) const;
	bool                                isTransitionIndex(const QModelIndex& index) const;

	const Cyberiada::LocalDocument*     rootDocument() const;
	Cyberiada::LocalDocument*           rootDocument();

	// write the document to a file without touching its own file identity
	// (the session-log start snapshot); false if there is no document to write
	// or the encoding fails
	bool                                writeSnapshotFile(const QString& path) const;

	// the editor view saved in the document metainformation (cyberiadaEditorView);
	// setEditorView writes it quietly (no undo step) just before a save
	QString                             editorView() const;
	void                                setEditorView(const QString& value);

	// UNDO: one step per user gesture, the whole document snapshotted
	// around the mutations between the two calls (the calls nest; an
	// unbracketed mutation is a step of its own)
	QUndoStack*                         undoStack() { return undo; }
	void                                beginUndoStep(const QString& text);
	void                                endUndoStep();
	void                                restoreSnapshot(const std::string& snapshot);
	const Cyberiada::Element*           indexToElement(const QModelIndex& index) const;
	Cyberiada::Element*                 indexToElement(const QModelIndex& index);
	const Cyberiada::Element*           idToElement(const QString& id) const;
	Cyberiada::Element*                 idToElement(const QString& id);
	
private:
	// on failure returns "" and sets ok to false; an empty document (no root)
	// succeeds with an empty snapshot
	std::string                         snapshot(bool* ok = nullptr) const;
	void                                move(Cyberiada::Element* element, Cyberiada::ElementCollection* target_parent);
	void                                declareGeometry(Cyberiada::DocumentFormat f, bool skip_geometry);
	
	Cyberiada::LocalDocument*           root;
	QString                             lastLoadError;
	QUndoStack*                         undo;
	// suppresses the session-log delete verb for the cascade deletions (an
	// attached transition removed with its endpoint) so a replay never
	// deletes an element the parent delete already removed
	int                                 deleteDepth = 0;
	int                                 undoDepth;
	QString                             undoText;
	std::string                         undoBefore;
	bool                                undoBeforeOk;
	bool                                m_growing = false;   // growToFitChildren re-entrancy guard
	// the file identity, restored after a snapshot decode
	QString                             filePath;
	Cyberiada::DocumentFormat           fileFormat;
	QString							   	cyberiadaStateMimeType;
	QIcon                              	emptyIcon;
	QMap<Cyberiada::ElementType, QIcon> icons;
};

#endif
