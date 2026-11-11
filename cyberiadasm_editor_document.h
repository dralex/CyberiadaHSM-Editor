/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The editor document: the model, the scene and the canvas view of one file
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

#ifndef CYBERIADA_SM_EDITOR_DOCUMENT
#define CYBERIADA_SM_EDITOR_DOCUMENT

#include <QObject>
#include <QString>

#include <QMap>
#include <QPointer>

#include "cyberiadasm_model.h"
#include "cyberiadasm_editor_scene.h"
#include "cyberiadasm_editor_view.h"
#include "plugin_conditions.h"
#include "editor_document.h"

class PluginRunner;

// A diagram tab of the window (docs/WINDOW.md): the model owns the undo stack
// and the file identity, the scene draws it, the view is the page of the
// document stack. The window keeps the global parts. The modes are HSM and
// HSM inspection, a document read as stored (docs/MODES.md)
class CyberiadaSMEditorDocument: public EditorDocument {
Q_OBJECT
public:
    CyberiadaSMEditorDocument(QObject* parent = nullptr);
    ~CyberiadaSMEditorDocument();

    CyberiadaSMModel*        model() const { return m_model; }
    CyberiadaSMEditorScene*  scene() const { return m_scene; }
    CyberiadaSMGraphicsView* view() const { return m_view; }

    QString                  mode() const override;
    // and no plugin run locks the document (EDIT-PLUG-22/41)
    bool                     isEditable() const override;
    QWidget*                 page() const override;
    QUndoStack*              undoStack() const override { return m_model->undoStack(); }

    // load the file into the model and the scene; the saved viewport is kept
    // pending until the view has a laid-out size (restoreView); an inspected
    // document is read as stored, never reconstructed (EDIT-INSPECT-2/3)
    bool                     load(const QString& path, QString* error,
                                  bool reconstruct = false, bool reconstruct_sm = false,
                                  bool strict = false, bool inspect = false);
    // inspect the snapshot of an unsaved document, titled as its source (EDIT-MODE-6)
    void                     inspectSnapshot(const std::string& snapshot, const QString& sourceTitle,
                                             const QString& viewState);
    // HSM on an inspection tab: the editing layout and the recovery (EDIT-INSPECT-4)
    void                     leaveInspection();
    // write the document back to its file (false without one, or with the error)
    bool                     save(QString* error);
    bool                     saveAs(const QString& path, Cyberiada::DocumentFormat format,
                                    bool round, bool skip_geometry, bool check_initial,
                                    bool strict_actions, bool skip_empty_behavior,
                                    QString* error);
    // File > New: an empty document with a clean history
    void                     clear();

    // the absolute file path, empty for an untitled document
    QString                  filePath() const override;
    // the file basename, else the title of the source of an inspected snapshot,
    // else "[document name]", else "untitled" (EDIT-DOC-10)
    QString                  title() const override;
    // the meta name of the document, empty without one
    QString                  documentName() const;
    bool                     isClean() const override;
    // an empty editable tab that a new or an opened document may take (EDIT-DOC-2)
    bool                     isUntitled() const;

    // apply the pending viewport if the view is laid out; true when nothing is pending
    bool                     restoreView();
    bool                     hasPendingView() const { return !pendingViewState.isEmpty(); }

    // the plugin run of the document, at most one (EDIT-PLUG-21); nullptr when idle
    PluginRunner*            pluginRun() const;
    void                     setPluginRun(PluginRunner* run);
    // the plugin values of an untitled document last for the session (EDIT-PLUG-11),
    // by "<plugin>/<parameter>"
    QMap<QString, QString>&  sessionPluginValues() { return m_sessionPluginValues; }
    // what the when tokens test (EDIT-PLUG-52): the content is kept until the document
    // changes, the file, the save and the selection are read each time
    PluginDocumentFacts      pluginFacts() const;
    // the git fact is kept for the path until a save or a scan (EDIT-PLUG-55)
    void                     dropGitFact() { m_gitValid = false; }

protected:
    void                     applyReadOnlyFlag(bool on) override;

private slots:
    // the title of an unsaved document follows its name (a rename, an undo, a reset)
    void                     refreshTitle();
    void                     dropPluginFacts() { m_pluginFactsValid = false; }

private:
    void                     announceTitle();

    QString                  lastTitle;
    QString                  m_sourceTitle;
    CyberiadaSMModel*        m_model;
    CyberiadaSMEditorScene*  m_scene;
    CyberiadaSMGraphicsView* m_view;
    QString                  pendingViewState;
    QPointer<PluginRunner>   m_pluginRun;
    QMap<QString, QString>   m_sessionPluginValues;
    mutable PluginDocumentFacts m_pluginFacts;
    mutable bool             m_pluginFactsValid = false;
    mutable QString          m_gitPath;
    mutable bool             m_git = false;
    mutable bool             m_gitValid = false;
};

#endif
