# The Editor Window — Documents and Tabs

**Document version:** 0.6 (2026-10-26)

The window holds several documents at once. The menus, the toolbars, the tab line and the
docks (the structure tree, the properties, the plugins) are one set; each document brings its
own model, scene and canvas view. The requirements are `EDIT-DOC-1..10` in `EDITOR-SPEC.md`
§4.11 and `EDIT-UI-6` for the docks; the plugin parts follow `docs/PLUGINS.md` (`EDIT-PLUG-n`,
§4.12), the plugin sessions its chapter Level 2 (`EDIT-PLUG-36..49`).

## Structure

```
CyberiadaSMEditorWindow (QMainWindow)                                   GLOBAL
 | menuBar  mainToolBar (top)  elementToolBar (left)  zoomOptionsToolBar  zoomCombo
 | Plugins menu + plugin toolbars (PluginsMenu, before zoomOptionsToolBar)
 | clipboard (a detached element copy)   armed tool (toolGroup)   QUndoGroup
 | documents : QList<CyberiadaSMEditorDocument*>   current
 |
 | central widget (QVBoxLayout)              | right dock area (EDIT-UI-6)
 |  +--------------------------------------+ | +---------------------------------+
 |  | documentTabs : QTabBar  hidden < 2   | | | treeDock "Structure tree" [_][x]|
 |  | [ a.graphml ][ b.graphml* ]          | | |   SMView (structure tree)       |
 |  +--------------------------------------+ | +---------------------------------+
 |  | documentStack : QStackedWidget       | | | propertiesDock "Properties"[_][x]|
 |  |   page i = documents[i]->view()      | | |   propertiesWidget              |
 |  |   (CyberiadaSMGraphicsView)          | | |   (re-targeted on switch)       |
 |  +--------------------------------------+ | +---------------------------------+
 |  View > Structure tree / Properties: the docks' toggle actions, kept in the settings
 +-------------------------------------------+-------------------------------------
 | pluginDock (QDockWidget, bottom, full width, hidden until the first run)   pluginRuns
 |
 +-- CyberiadaSMEditorDocument (QObject)                                 PER DOCUMENT
       model : CyberiadaSMModel         the document tree, the undo stack, the file path
       scene : CyberiadaSMEditorScene   the canvas items (parent: the document)
       view  : CyberiadaSMGraphicsView  the stacked page (zoom, pan)
       pendingViewState                 the saved viewport, applied on the first activation
       pluginRun()                      the running plugin (one); a replace run and a
                                        session lock; a session marks the scene (RunMark)
       title()                          the file basename, else "[document name]", else "untitled"
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
| New, Open, Open Recent, Exit, Preferences, About | global | New = a new tab; Open = replace a clean untitled tab, else a new tab; an open path activates its tab; the interface language of the Preferences applies at the next start (`EDIT-UI-1`) |
| Grid, Snap, Service objects, Transition text, Log session | global | settings; every scene listens to `SettingsManager` |
| Inspection mode | global | every document is read-only together (`EDIT-DOC-6`) |
| Armed tool | global | pushed to the active scene on a switch; a scene's own tool change is honoured only from the active scene |
| Clipboard | global | paste into the active document (`EDIT-DOC-5`) |
| Close (Ctrl+W), Save, Save As, Export | document | the active one; Save enabling as `EDIT-IO-7` |
| Undo, Redo | document | the active stack of the `QUndoGroup` |
| Cut, Copy, Delete, Reconstruct geometry | document | the active scene's selection |
| Zoom in/out, Fit, Zoom to SM, zoom combo | document | the active view |
| Edit > Language, code highlighting | document | the checked entry is the active document's `platformLanguage`; each tab is highlighted by its own (`EDIT-TEXT-9`); the list of unknown values met is global |
| Edit > Markup, comment markup | document | the checked entry is the active document's `markupLanguage` (`EDIT-TEXT-12`); the list of unknown values met is global |
| Structure tree, properties panel | shared docks | content re-targeted on a switch; shown or hidden by View, the choice kept (`EDIT-UI-6`) |
| Window title and `[*]` | document | the active one; the tab text carries `*` when modified; an unsaved document is `[document name]` or `untitled` (`EDIT-DOC-10`) |
| Saved viewport (zoom/pan in the document) | document | restored on the tab's first activation |
| Gesture-log session | bound to one document | a switch ends it and starts a new one (`EDIT-DOC-8`) |
| Batch mode | the tab line | `open`, `new-document`, `switch`, `close`; dumps of the active tab (`EDIT-DOC-9`) |
| Exit | global | asks for every modified document in turn; a cancel keeps the editor open; running plugins are cancelled and the sessions stopped after one question (`EDIT-PLUG-23/45`) |
| Plugins menu, plugin toolbars | global | an action runs on the active document; its state and tooltip follow it (`EDIT-PLUG-4..7`) |
| Plugin run | bound to one document | one at a time; a replace run locks the document; the result goes to it after any switch (`EDIT-PLUG-21/22`) |
| Plugin session | bound to one document | a run (one at a time); locks the document with the view interactive; marks the elements on the document's own scene; its tab "<document> session" in the plugin dock (`EDIT-PLUG-37/40/41/42`) |
| Plugins > Session, F10, F5, F6, Shift+F5 | document | act on the session of the active document; disabled without one (`EDIT-PLUG-46`) |
| Plugin dock | global | the findings link to the document of their run; one output tab per plugin, not per document, each run headed by its document; a session tab per session, named by its document (`EDIT-PLUG-15/17/32/35/42`) |
| Command-line files (GUI) | global | each opens a tab, the last one active (`EDIT-DOC-7`) |

## Lifetimes and traps

- A `QModelIndex` belongs to one model: the tree and the scene of a background document are never
  linked, and `setRootIndex` follows every `setModel` of the tree.
- The right docks and the undo group are re-targeted before a document is deleted; the properties
  widget keeps raw model, scene and element pointers. A hidden dock is re-targeted all the same.
- The dock toggle actions are unchecked by Qt whenever the docks hide, the main window's own hide
  included: the shown/hidden choice is stored only from the menu entry and the dock's close
  button, never from a visibility signal.
- A hidden stacked page keeps a stale size: the saved viewport is applied to the active document
  only, re-armed by a zero-delay timer on activation and on the window show.
- The scene fits the loaded diagram against its first view: a new tab is activated before its
  document loads.
- `QTabBar::currentChanged` fires on `addTab`/`removeTab`: the structural changes run with the tab
  bar signals blocked and set the active document explicitly.
