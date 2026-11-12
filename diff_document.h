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

#include <QPointer>

#include "editor_document.h"
#include "cyberiadasm_model.h"
#include "plugin_values.h"

class QLabel;
class QSplitter;
class CyberiadaSMEditorScene;
class CyberiadaSMGraphicsView;

// where a side of a diff tab comes from, so Refresh takes it again (EDIT-MODE-12)
struct DiffOrigin {
    enum Kind {
        originTab = 0,      // the snapshot of an open diagram tab
        originFile,         // a file
        originSaved,        // the saved file of a tab
        originPlugin        // the output of a plugin run on a tab (EDIT-PLUG-58)
    };
    Kind                     kind = originTab;
    QPointer<EditorDocument> tab;
    QString                  path;
    // the name of the side in the title: a tab title, a file name, "saved", the
    // values of a plugin run
    QString                  label;
    // a plugin base: the action Refresh runs again with the same values
    QString                  pluginId;
    QString                  actionId;
    PluginValues             values;
};

// one difference: a row of the change list and of the == diff dump (EDIT-MODE-11)
struct DiffRow {
    QString machine;     // the state machine it belongs to; empty for the metainformation
    QString element;     // "state Idle", "transition Idle -> Busy", "meta platform"
    QString first;       // the id of the base side, empty when added
    QString second;      // the id of the target side, empty when removed
    QString change;      // "added", "removed", or what differs: "title, actions"
};

// A diff tab (docs/MODES.md, The diff tab): the base and the target read as
// inspected, compared by the library's revision diff; read-only always
class DiffDocument: public EditorDocument {
Q_OBJECT
public:
    DiffDocument(QObject* parent = nullptr);
    ~DiffDocument();

    QString          mode() const override { return MODE_DIFF; }
    // "<target> ↔ <base>"
    QString          title() const override;
    QString          filePath() const override { return QString(); }
    bool             isClean() const override { return true; }
    QWidget*         page() const override;
    QUndoStack*      undoStack() const override { return m_undo; }

    DiffOrigin&      base() { return m_base; }
    DiffOrigin&      target() { return m_target; }
    // the sides as buffers in any format the library reads, an empty one an empty
    // document; both are read as stored and compared
    void             setSides(const std::string& base, const std::string& target);
    const std::string& baseBuffer() const { return m_baseBuffer; }
    const std::string& targetBuffer() const { return m_targetBuffer; }
    // Document > Geometry: the geometry, the ids and the view state compared too
    bool             geometry() const { return m_geometry; }
    void             setGeometry(bool on);

    const QList<DiffRow>& rows() const { return m_rows; }
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
    void             fit();

signals:
    void             compared();

private:
    void             compare();
    void             mark();
    QString          label(const CyberiadaSMModel* model, const QString& id) const;

    DiffOrigin                m_base;
    DiffOrigin                m_target;
    std::string               m_baseBuffer;
    std::string               m_targetBuffer;
    bool                      m_geometry = false;
    QList<DiffRow>            m_rows;
    int                       m_added = 0;
    int                       m_removed = 0;
    int                       m_changed = 0;
    QString                   m_error;
    QUndoStack*               m_undo;
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
