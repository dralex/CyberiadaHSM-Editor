# The Editor Window — Documents and Tabs

**Document version:** 0.1 (2026-10-04)

The window holds several documents at once. The menus, the toolbars, the tab line and the
right column are one set; each document brings its own model, scene and canvas view. The
requirements are `EDIT-DOC-1..9` in `EDITOR-SPEC.md` §4.11.

## Structure

```
CyberiadaSMEditorWindow (QMainWindow)                                   GLOBAL
 | menuBar  mainToolBar (top)  elementToolBar (left)  zoomOptionsToolBar  zoomCombo
 | clipboard (a detached element copy)   armed tool (toolGroup)   QUndoGroup
 | documents : QList<CyberiadaSMEditorDocument*>   current
 |
 | central widget (QVBoxLayout)
 |  +-------------------------------------------------------------------+
 |  | documentTabs : QTabBar   [ a.graphml ][ b.graphml* ]   hidden < 2 |
 |  +--------------------------------------+----------------------------+
 |  | documentStack : QStackedWidget       | hSplitter                  |
 |  |   page i = documents[i]->view()      |  SMView (structure tree)   |
 |  |   (CyberiadaSMGraphicsView)          |  propertiesWidget          |
 |  |                                      |  (re-targeted on switch)   |
 |  +--------------- vSplitter ------------+----------------------------+
 |
 +-- CyberiadaSMEditorDocument (QObject)                                 PER DOCUMENT
       model : CyberiadaSMModel         the document tree, the undo stack, the file path
       scene : CyberiadaSMEditorScene   the canvas items (parent: the document)
       view  : CyberiadaSMGraphicsView  the stacked page (zoom, pan)
       pendingViewState                 the saved viewport, applied on the first activation
       title()                          "untitled" or the file basename
```

## Switching the active document

```
documentTabs.currentChanged(i)  /  openFile  /  newDocument  /  closeTab
      |
      v
setCurrentDocument(doc)
  unbind the old document        SMView <-> scene selection links
  documentStack.setCurrentWidget(doc.view)
  SMView.setModel(doc.model) + setRootIndex   propertiesWidget.setModel/setScene(doc)
  undoGroup.setActiveStack(doc.model.undoStack())   -> undo/redo actions and texts
  bind the new document          scene.elementSelected -> SMView.select
                                 SMView.currentIndexActivated -> scene.slotElementSelected
  doc.scene.setCurrentTool(armed tool); doc.view.setCurrentTool(armed tool)
  zoom combo <- doc.view scale
  title, [*] marker, edit actions <- doc
  gesture log: end the session, start one on doc.model (when logging)
  restore the saved viewport once the page is laid out
```

## What is global and what follows the active document

| Part / action | Scope | Rule |
|---|---|---|
| Menus, toolbars, tab line | global | one set; the actions target the active document |
| New, Open, Open Recent, Exit, Preferences, About | global | New = a new tab; Open = replace a clean untitled tab, else a new tab; an open path activates its tab |
| Grid, Snap, Service objects, Transition text, Log session | global | settings; every scene listens to `SettingsManager` |
| Inspection mode | global | every document is read-only together (`EDIT-DOC-6`) |
| Armed tool | global | pushed to the active scene on a switch; a scene's own tool change is honoured only from the active scene |
| Clipboard | global | paste into the active document (`EDIT-DOC-5`) |
| Close (Ctrl+W), Save, Save As, Export | document | the active one; Save enabling as `EDIT-IO-7` |
| Undo, Redo | document | the active stack of the `QUndoGroup` |
| Cut, Copy, Delete, Reconstruct geometry | document | the active scene's selection |
| Zoom in/out, Fit, Zoom to SM, zoom combo | document | the active view |
| Edit > Language, code highlighting | document | the checked entry is the active document's `platformLanguage`; each tab is highlighted by its own (`EDIT-TEXT-9`); the list of unknown values met is global |
| Structure tree, properties panel | shared widget | content re-targeted on a switch |
| Window title and `[*]` | document | the active one; the tab text carries `*` when modified |
| Saved viewport (zoom/pan in the document) | document | restored on the tab's first activation |
| Gesture-log session | bound to one document | a switch ends it and starts a new one (`EDIT-DOC-8`) |
| Batch mode | the tab line | `open`, `new-document`, `switch`, `close`; dumps of the active tab (`EDIT-DOC-9`) |
| Exit | global | asks for every modified document in turn; a cancel keeps the editor open |
| Command-line files (GUI) | global | each opens a tab, the last one active (`EDIT-DOC-7`) |

## Lifetimes and traps

- A `QModelIndex` belongs to one model: the tree and the scene of a background document are never
  linked, and `setRootIndex` follows every `setModel` of the tree.
- The right column and the undo group are re-targeted before a document is deleted; the properties
  widget keeps raw model, scene and element pointers.
- A hidden stacked page keeps a stale size: the saved viewport is applied to the active document
  only, re-armed by a zero-delay timer on activation and on the window show.
- The scene fits the loaded diagram against its first view: a new tab is activated before its
  document loads.
- `QTabBar::currentChanged` fires on `addTab`/`removeTab`: the structural changes run with the tab
  bar signals blocked and set the active document explicitly.
