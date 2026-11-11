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

#include <QFileInfo>
#include <QScrollBar>

#include "cyberiadasm_editor_document.h"
#include "plugin_runner.h"

CyberiadaSMEditorDocument::CyberiadaSMEditorDocument(QObject* parent):
    EditorDocument(parent)
{
    m_model = new CyberiadaSMModel(this);
    m_scene = new CyberiadaSMEditorScene(m_model, this);
    m_view = new CyberiadaSMGraphicsView();
    m_view->setScene(m_scene);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &CyberiadaSMEditorDocument::refreshTitle);
    connect(m_model, &QAbstractItemModel::modelReset, this, &CyberiadaSMEditorDocument::refreshTitle);
    connect(m_model, &QAbstractItemModel::modelReset, this, &CyberiadaSMEditorDocument::dropPluginFacts);
    connect(m_model->undoStack(), &QUndoStack::indexChanged, this, &CyberiadaSMEditorDocument::dropPluginFacts);
    lastTitle = title();
}

// the view goes first (it paints the scene), the scene before the model
CyberiadaSMEditorDocument::~CyberiadaSMEditorDocument()
{
    delete m_view;
    delete m_scene;
    delete m_model;
}

QString CyberiadaSMEditorDocument::mode() const
{
    return m_model->inspected() ? MODE_INSPECTION : MODE_HSM;
}

bool CyberiadaSMEditorDocument::isEditable() const
{
    return EditorDocument::isEditable() && !m_model->isLocked();
}

QWidget* CyberiadaSMEditorDocument::page() const
{
    return m_view;
}

void CyberiadaSMEditorDocument::applyReadOnlyFlag(bool on)
{
    m_model->setReadOnlyFlag(on);
}

bool CyberiadaSMEditorDocument::load(const QString& path, QString* error,
                                     bool reconstruct, bool reconstruct_sm, bool strict,
                                     bool inspect)
{
    bool wasInspected = m_model->inspected();
    // the flag goes first: the load itself skips the recovery (EDIT-INSPECT-2)
    m_model->setInspected(inspect);
    if (!m_model->loadDocument(path, reconstruct && !inspect, reconstruct_sm && !inspect, strict)) {
        if (error) *error = m_model->loadError();
        m_model->setInspected(wasInspected);
        return false;
    }
    m_sourceTitle.clear();
    if (inspect != wasInspected) emit modeChanged();
    if (m_model->firstSMIndex().isValid()) {
        m_scene->loadScene();
    }
    pendingViewState = m_model->editorView();
    announceTitle();
    return true;
}

void CyberiadaSMEditorDocument::inspectSnapshot(const std::string& snapshot,
                                                const QString& sourceTitle,
                                                const QString& viewState)
{
    bool wasInspected = m_model->inspected();
    m_model->setInspected(true);
    m_model->restoreSnapshot(snapshot);
    m_model->undoStack()->clear();
    m_sourceTitle = sourceTitle;
    pendingViewState = viewState;
    if (!wasInspected) emit modeChanged();
    announceTitle();
}

void CyberiadaSMEditorDocument::leaveInspection()
{
    if (!m_model->inspected()) return;
    // the model reset reloads the scene in the editing layout, which adopts the regions
    m_model->leaveInspection();
    // an unsaved snapshot becomes an untitled document of its own
    m_sourceTitle.clear();
    emit modeChanged();
    announceTitle();
}

bool CyberiadaSMEditorDocument::save(QString* error)
{
    if (!hasFile()) return false;
    // the save refreshes the actions, which look for the .git directory anew
    m_gitValid = false;
    try {
        m_scene->migrateLabelsToRect();                 // point labels become rects
        m_model->setEditorView(m_view->viewState());   // persist the current view
        m_model->saveDocument();
    } catch (const Cyberiada::Exception& e) {
        if (error) *error = QString(e.str().c_str());
        return false;
    }
    return true;
}

bool CyberiadaSMEditorDocument::saveAs(const QString& path, Cyberiada::DocumentFormat format,
                                       bool round, bool skip_geometry, bool check_initial,
                                       bool strict_actions, bool skip_empty_behavior,
                                       QString* error)
{
    m_gitValid = false;
    try {
        m_scene->migrateLabelsToRect();
        m_model->setEditorView(m_view->viewState());
        m_model->saveAsDocument(path, format, round, skip_geometry, check_initial,
                                strict_actions, skip_empty_behavior);
    } catch (const Cyberiada::Exception& e) {
        if (error) *error = QString(e.str().c_str());
        return false;
    }
    announceTitle();
    return true;
}

void CyberiadaSMEditorDocument::clear()
{
    m_model->reset();
    m_model->undoStack()->clear();
    pendingViewState.clear();
    announceTitle();
}

QString CyberiadaSMEditorDocument::filePath() const
{
    return m_model->documentPath();
}

QString CyberiadaSMEditorDocument::title() const
{
    if (hasFile()) return QFileInfo(filePath()).fileName();
    if (!m_sourceTitle.isEmpty()) return m_sourceTitle;
    QString name = documentName();
    return name.isEmpty() ? tr("untitled") : "[" + name + "]";
}

QString CyberiadaSMEditorDocument::documentName() const
{
    const Cyberiada::LocalDocument* root = m_model->rootDocument();
    return root ? QString::fromStdString(root->get_name()) : QString();
}

void CyberiadaSMEditorDocument::announceTitle()
{
    lastTitle = title();
    emit titleChanged();
}

void CyberiadaSMEditorDocument::refreshTitle()
{
    if (title() != lastTitle) announceTitle();
}

PluginDocumentFacts CyberiadaSMEditorDocument::pluginFacts() const
{
    if (!m_pluginFactsValid) {
        PluginConditions::readDocument(m_model->rootDocument(), &m_pluginFacts);
        m_pluginFactsValid = true;
    }
    PluginDocumentFacts facts = m_pluginFacts;
    if (!m_gitValid || filePath() != m_gitPath) {
        m_gitPath = filePath();
        m_git = PluginConditions::readGit(m_gitPath);
        m_gitValid = true;
    }
    facts.git = m_git;
    facts.file = hasFile();
    facts.saved = hasFile() && isClean();
    facts.selection = !m_scene->selectedIds().isEmpty();
    return facts;
}

bool CyberiadaSMEditorDocument::isClean() const
{
    return m_model->undoStack()->isClean();
}

bool CyberiadaSMEditorDocument::isUntitled() const
{
    return !hasFile() && isClean() && mode() == MODE_HSM && !readOnlyFlag() &&
           m_sourceTitle.isEmpty();
}

bool CyberiadaSMEditorDocument::restoreView()
{
    if (pendingViewState.isEmpty()) return true;
    // the scrollbar-based pan restores against the final width only
    if (m_view->viewport()->width() <= 0) return false;
    m_view->applyViewState(pendingViewState);
    pendingViewState.clear();
    return true;
}

PluginRunner* CyberiadaSMEditorDocument::pluginRun() const
{
    return m_pluginRun.data();
}

void CyberiadaSMEditorDocument::setPluginRun(PluginRunner* run)
{
    m_pluginRun = run;
}
