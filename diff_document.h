/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The diff tab: two revisions of a diagram side by side, their differences marked
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

#ifndef CYBERIADA_DIFF_DOCUMENT
#define CYBERIADA_DIFF_DOCUMENT

#include "comparison_document.h"
#include "cyberiadasm_model.h"

class QLabel;
class QSplitter;
class CyberiadaSMEditorScene;
class CyberiadaSMGraphicsView;

// one difference: a row of the change list and of the == diff dump (EDIT-MODE-11)
struct DiffRow {
    QString machine;     // the state machine it belongs to; empty for the metainformation
    QString element;     // "state Idle", "transition Idle -> Busy", "meta platform"
    QString first;       // the id of the base side, empty when added
    QString second;      // the id of the target side, empty when removed
    QString change;      // "added", "removed", or what differs: "title, actions"
    Cyberiada::DiffFlags flags = 0;   // what differs, as the library tells it
};

// A diff tab (docs/MODES.md, The diff tab): the base and the target read as
// inspected, compared by the library's revision diff
class DiffDocument: public ComparisonDocument {
Q_OBJECT
public:
    DiffDocument(QObject* parent = nullptr);
    ~DiffDocument();

    QString          mode() const override { return MODE_DIFF; }
    QWidget*         page() const override;
    // Document > Geometry: the geometry, the ids and the view state compared too
    bool             geometry() const { return m_geometry; }
    void             setGeometry(bool on);

    const QList<DiffRow>& rows() const { return m_rows; }
    // a row per DiffRow, the machine rows the groups
    QList<ChangeEntry> changes() const override;
    int              added() const { return m_added; }
    int              removed() const { return m_removed; }
    int              changed() const { return m_changed; }
    // the library could not compare: the reason, else empty
    const QString&   error() const { return m_error; }

    CyberiadaSMModel*        baseModel() const { return m_baseModel; }
    CyberiadaSMModel*        targetModel() const { return m_targetModel; }
    CyberiadaSMEditorScene*  baseScene() const { return m_baseScene; }
    CyberiadaSMEditorScene*  targetScene() const { return m_targetScene; }
    // a row of the change list: the element selected and centred on both sides
    void             showElement(const QString& first, const QString& second);
    void             zoomIn();
    void             zoomOut();
    void             fit() override;

protected:
    void             loadSides() override;
    void             showChange(int index) override;

private:
    void             compare();
    void             mark();
    QString          label(const CyberiadaSMModel* model, const QString& id) const;

    bool                      m_geometry = false;
    QList<DiffRow>            m_rows;
    int                       m_added = 0;
    int                       m_removed = 0;
    int                       m_changed = 0;
    QString                   m_error;
    CyberiadaSMModel*         m_baseModel;
    CyberiadaSMModel*         m_targetModel;
    CyberiadaSMEditorScene*   m_baseScene;
    CyberiadaSMEditorScene*   m_targetScene;
    CyberiadaSMGraphicsView*  m_baseView;
    CyberiadaSMGraphicsView*  m_targetView;
    QLabel*                   m_baseLabel;
    QLabel*                   m_targetLabel;
    QSplitter*                m_page;
};

#endif
