/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
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

CyberiadaSMEditorDocument::CyberiadaSMEditorDocument(QObject* parent):
    QObject(parent)
{
    m_model = new CyberiadaSMModel(this);
    m_scene = new CyberiadaSMEditorScene(m_model, this);
    m_view = new CyberiadaSMGraphicsView();
    m_view->setScene(m_scene);
}

// the view goes first (it paints the scene), the scene before the model
CyberiadaSMEditorDocument::~CyberiadaSMEditorDocument()
{
    delete m_view;
    delete m_scene;
    delete m_model;
}

bool CyberiadaSMEditorDocument::load(const QString& path, QString* error,
                                     bool reconstruct, bool reconstruct_sm, bool strict)
{
    if (!m_model->loadDocument(path, reconstruct, reconstruct_sm, strict)) {
        if (error) *error = m_model->loadError();
        return false;
    }
    if (m_model->firstSMIndex().isValid()) {
        m_scene->loadScene();
    }
    pendingViewState = m_model->editorView();
    emit titleChanged();
    return true;
}

bool CyberiadaSMEditorDocument::save(QString* error)
{
    if (!hasFile()) return false;
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
    try {
        m_scene->migrateLabelsToRect();
        m_model->setEditorView(m_view->viewState());
        m_model->saveAsDocument(path, format, round, skip_geometry, check_initial,
                                strict_actions, skip_empty_behavior);
    } catch (const Cyberiada::Exception& e) {
        if (error) *error = QString(e.str().c_str());
        return false;
    }
    emit titleChanged();
    return true;
}

void CyberiadaSMEditorDocument::clear()
{
    m_model->reset();
    m_model->undoStack()->clear();
    pendingViewState.clear();
    emit titleChanged();
}

QString CyberiadaSMEditorDocument::filePath() const
{
    return m_model->documentPath();
}

QString CyberiadaSMEditorDocument::title() const
{
    return hasFile() ? QFileInfo(filePath()).fileName() : tr("untitled");
}

bool CyberiadaSMEditorDocument::isClean() const
{
    return m_model->undoStack()->isClean();
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
