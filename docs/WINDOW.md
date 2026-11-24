# The Editor Window — Documents and Tabs

**Document version:** 0.28 (2026-11-23) — the modes, the read-only flag and the plugins in
the modes (`modes`, the text tabs, a plugin run as a diff base, the plugin languages) are
implemented, the git compare with them (stage 6 of `docs/MODES.md`); a switch keeps the dock
sizes; a Markdown tab exports its rendered PDF; the find bar (`docs/FIND.md`); the shortcuts
and the autoreplace (`docs/SHORTCUTS.md`, `docs/AUTOREPLACE.md`); the color schemes of the
text editors (`docs/COLOR-SCHEMES.md`) and their font; the text diff tab (stage 7 of `docs/MODES.md`);
0.21: the side docks follow the tab type, the change list a dock of its own.
0.22: Open Terminal.
0.23: the toolbars follow the tab — Main, Diagram, Elements, Text (`EDIT-MD-1`); the Markdown
tools (`docs/MARKDOWN.md`, Tools).
0.24: the Outline dock of a Markdown tab (`EDIT-MD-13..17`, `docs/MARKDOWN.md`, Outline).
0.25: Fill Paragraph, Unfill Paragraph and Fill as You Type (`EDIT-MD-18..23`).
0.26: View > Mode Line (`EDIT-MODE-31`).
0.27: Insert Formula (`EDIT-MD-24..27`).
0.28: the Git menu, the Repository dock (its Files and History tabs under its title, at the
right below the structure tree and the properties, as Inkscape stacks its dialogs), the disk
bar of a tab
(`EDIT-GIT-1..23`, `EDIT-DOC-13..18`, `docs/GIT.md`), specified.

The window holds several documents at once. The menus, the toolbars, the tab line and the
docks (the structure tree, the properties, the changes, the outline, the repository, the
history, the plugins) are one set; each document brings its
own page — a diagram its model, scene and canvas view — and its mode (`docs/MODES.md`). The
requirements are `EDIT-DOC-1..18` in `EDITOR-SPEC.md` §4.11, `EDIT-GIT-1..23` §4.20 for git, `EDIT-MODE-1..17` §4.14 and
`EDIT-UI-6` for the docks, `EDIT-FIND-1..9` §4.15 for the find bar; the plugin parts follow `docs/PLUGINS.md` (`EDIT-PLUG-n`, §4.12), the
plugin sessions its chapter Level 2 (`EDIT-PLUG-36..49`).

## Structure

```
CyberiadaSMEditorWindow (QMainWindow)                                   GLOBAL
 | menuBar (File, Edit, Document, View, Git, Plugins, Help)   ModeTraits (document_mode.h)
 | mainToolBar, diagramToolBar (top)  elementToolBar | textToolBar (left)
 | zoomOptionsToolBar  zoomCombo
 | Plugins menu + plugin toolbars (PluginsMenu, before zoomOptionsToolBar)
 | clipboard (a detached element copy)   armed tool (toolGroup)   QUndoGroup
 | documents : QList<EditorDocument*>   current
 |
 | RepositoryService (docs/GIT.md): one per work tree   the window watches the files of the tabs
 |
 | central widget (QVBoxLayout)              | right dock area (EDIT-UI-6)
 |  +--------------------------------------+ | +---------------------------------+
 |  | documentTabs : QTabBar  hidden < 2   | | | treeDock "Structure tree"  [_][x]|
 |  | [ a.graphml ][ a.graphml [FA]* ]     | | |   SMView                        |
 |  |                                      | | |                                 |
 |  +--------------------------------------+ | +---------------------------------+
 |  | documentStack : QStackedWidget       | | | propertiesDock "Properties"[_][x]|
 |  |   page i = documents[i]->page()      | | |   propertiesWidget              |
 |  |   a canvas, two canvases (diff), a   | | |   (re-targeted on switch)       |
 |  |   CodeEditor (text), two (text diff) | | |                                 |
 |  |   the disk bar above (EDIT-DOC-15)   | | +---------------------------------+
 |  +--------------------------------------+ | | changesDock "Changes"      [_][x]|
 |  | findBar : FindBar  hidden until Find | | |   changeList (a (text) diff tab)|
 |  +--------------------------------------+ | +---------------------------------+
 |                                           | | outlineDock "Outline"      [_][x]|
 |                                           | |   outlineView (a Markdown tab)  |
 |                                           | +---------------------------------+
 |                                           | | repositoryDock "Repository"[_][x]|
 |                                           | |   work tree + branch line,      |
 |                                           | |   tabs [Files|History]; every   |
 |                                           | |   tab, git in use               |
 |                                           | +---------------------------------+
 |  View > Structure tree / Properties / Changes / Outline / Repository: the docks'
 |  toggle actions, kept in the settings; applyDocks shows the docks of ModeTraits::docks as
 |  chosen, greys the others; the git docks in every tab while git is in use
 +-------------------------------------------+-------------------------------------
 | pluginDock (QDockWidget, bottom, full width, hidden until the first run)   pluginRuns
 |   + the "Git output" tab of the editor's git commands (EDIT-GIT-17)
 |
 +-- EditorDocument (QObject): mode, read-only flag, title marker, page    PER DOCUMENT
 |     DiffDocument, TextDocument: docs/MODES.md
 +-- CyberiadaSMEditorDocument (EditorDocument): HSM, FA, HSM inspection
       model : CyberiadaSMModel         the document tree, the undo stack, the file path
       scene : CyberiadaSMEditorScene   the canvas items (parent: the document)
       view  : CyberiadaSMGraphicsView  the stacked page (zoom, pan)
       pendingViewState                 the saved viewport, applied on the first activation
       disk()                           the size, time and digest last read or written;
                                        the disk bar (EDIT-DOC-13..18)
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
  documentStack.setCurrentWidget(doc.page)
  apply ModeTraits(doc.mode) and the read-only flag: tools, docks, Document menu, actions
  applyDocks: the docks of ModeTraits::docks shown as View chose, the others hidden and
                                 their View entries greyed; the settings stay
  SMView.setModel(doc.model) + setRootIndex   propertiesWidget.setModel/setScene(doc)
                                 (a diagram; a text tab empties them, a diff tab lists changes)
  outlineView.setModel(text.outline())  a Markdown tab, read first when out of date; nullptr
                                 for another tab; setOutlineShown on the old and the new tab
  expand the tree; widen its dock to the contents (grow-only, the docks keep their sizes)
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
| New, Open, Open Recent, Exit, Preferences, About | global | New > HSM diagram, FA diagram, Text file = a new tab in that mode; Open = replace a clean untitled tab, else a new tab, the mode by the file (`EDIT-MODE-5/13`), its dialog on the file type and in the folder of the active tab, the inspection box an inspection tab; an open path activates its tab; the interface language of the Preferences applies at the next start (`EDIT-UI-1`) |
| Grid, Snap, Service objects, Transition text, FA symbols, Log session | global | settings; every scene listens to `SettingsManager` |
| Document > Mode, Read-only | document | the mode and the flag of the active tab (`EDIT-DOC-6`, `EDIT-MODE-1..7`); HSM inspection and Compare with... open tabs of their own |
| Document > Check, Geometry, Refresh | document | Check on an FA and a Markdown tab (`EDIT-MD-8`), Geometry and Refresh on a diff tab (`EDIT-MODE-9..12`), Refresh on a text diff tab |
| Document > Next Change, Previous Change, Ignore Whitespace | document | the current row of the change list of a diff or text diff tab; Ignore Whitespace on a text diff tab (`EDIT-MODE-22/23`) |
| Armed tool | global | pushed to the active scene on a switch; a scene's own tool change is honoured only from the active scene |
| Clipboard | global | paste into the active document (`EDIT-DOC-5`) |
| Close (Ctrl+W), Save, Save As, Export | document | the active one; Save, Save As and Export enabling as `EDIT-IO-7`; Export of a Markdown tab saves its rendered PDF (`EDIT-MODE-18`) |
| Open Terminal | document | the system terminal in the folder of the active tab's file (`system_terminal.cpp`); disabled for a tab without a file (`EDIT-DOC-12`) |
| Undo, Redo | document | the active stack of the `QUndoGroup`; a text tab's stack proxies its `QTextDocument` |
| Cut, Copy, Delete | document | the active scene's selection, or the active text editor's |
| Find bar, Find, Find Next, Find Previous | global | one bar and query; the marks are the active tab's, dropped on a switch and searched again in the new tab; disabled in a diff tab, one side of a text diff tab (`EDIT-FIND-9`, `EDIT-MODE-24`, `docs/FIND.md`) |
| Go to Line | document | the cursor of the active text tab, also on the Text toolbar; disabled elsewhere (`EDIT-KEYS-7`) |
| Insert Image, Insert Diagram, Insert Link | document | the cursor of the active Markdown tab, one undo step; an untitled tab saved first (`EDIT-MD-2..5`, `EDIT-MD-9`) |
| Insert Formula | document | the cursor of the active Markdown tab, one undo step, an untitled tab too; the formula under the cursor edited, as by Ctrl+click on it (`EDIT-MD-24..27`) |
| Fill Paragraph, Unfill Paragraph | document | the unit of the cursor or the units of the selection in the active Markdown or Plain text tab, one undo step; Fill Paragraph also on the Text toolbar (`EDIT-MD-18..23`) |
| Edit > Fill as You Type | global | a setting; a Space or a Return typed past the text width breaks the line in every Markdown and Plain text tab (`EDIT-MD-21/22`) |
| Document > Update Diagram Images | document | the diagram images of the active Markdown tab, written beside their diagrams; the text unchanged (`EDIT-MD-7`) |
| View > Mode Line | global | a setting; every source editor of a text tab shows its mode line, the position of its cursor among the parts (`EDIT-MODE-31`) |
| View > Wrap Long Lines | global | a setting; every source editor but a text diff side wraps its long lines at the width (`EDIT-MODE-30`) |
| View > Preview | global | a setting; every writable Markdown tab shows its source and its rendered page side by side (`EDIT-MD-11`) |
| Shortcuts | global | one set of keys for the window, changed in the Preferences; the focused text takes its keys first (`EDIT-KEYS-1..12`, `docs/SHORTCUTS.md`) |
| Autoreplace | global | the switch and the rules are settings; each text follows them by its kind (`EDIT-AUTO-1..8`, `docs/AUTOREPLACE.md`) |
| Color scheme | global | one current scheme for the source editors of all text tabs, changed in the Preferences (`EDIT-COLOR-1..8`, `docs/COLOR-SCHEMES.md`) |
| Text editor font | global | one font for the source editors and the text diff sides, apart from the diagram font, changed in the Preferences (`EDIT-MODE-27`) |
| Document > Reconstruct geometry, Update geometry | document | the active diagram; disabled in a tab that is not editable (`EDIT-MODE-4`) |
| Zoom in/out, Fit, Zoom to SM, zoom combo | document | the active view; disabled in a text tab |
| Toolbars | global | Main always; Diagram in a diagram or a diff tab, Elements in a diagram tab, Text in a text tab, none of the three in a text diff tab; the entries of a hidden one greyed in the menus; one the user closed stays closed (`EDIT-MD-1`) |
| Document > Language, code highlighting | document | the checked entry is the active document's `platformLanguage`; each tab is highlighted by its own (`EDIT-TEXT-9`); the list of unknown values met is global |
| Document > Markup, comment markup | document | the checked entry is the active document's `markupLanguage` (`EDIT-TEXT-12`); the list of unknown values met is global |
| Structure tree, properties panel, changes, outline | shared docks | content re-targeted on a switch; shown in the tabs that have them as View chose, the choice kept (`EDIT-UI-6`): a diagram the tree and the properties, a (text) diff the changes, a Markdown tab the outline, another text tab none |
| Outline model | document | owned by a Markdown tab, read from its text at most once a second while the dock shows it, out of date otherwise (`EDIT-MD-14`) |
| Window title and `[*]` | document | the active one with its mode marker (`EDIT-MODE-2`); the tab text carries the marker and `*` when modified; an unsaved document is `[document name]` or `untitled` (`EDIT-DOC-10`) |
| Saved viewport (zoom/pan in the document) | document | restored on the tab's first activation |
| Gesture-log session | bound to one document | a switch ends it and starts a new one (`EDIT-DOC-8`) |
| Batch mode | the tab line | `open`, `new-document [<mode>]`, `switch`, `close`, `mode`, `read-only`, `compare`, `geometry`, `refresh`; dumps of the active tab (`EDIT-DOC-9`, `EDIT-MODE-16`) |
| Exit | global | asks for every modified document in turn; a cancel keeps the editor open; running plugins are cancelled and the sessions stopped after one question (`EDIT-PLUG-23/45`) |
| Plugins menu, plugin toolbars | global | an action runs on the active document; its state and tooltip follow it and its mode (`EDIT-PLUG-4..7`, `EDIT-PLUG-56`) |
| Plugin run | bound to one document | one at a time; a replace run locks the document; the result goes to it after any switch (`EDIT-PLUG-21/22`) |
| Plugin session | bound to one document | a run (one at a time); locks the document with the view interactive; marks the elements on the document's own scene; its tab "<document> session" in the plugin dock (`EDIT-PLUG-37/40/41/42`) |
| Plugins > Session, F10, F5, F6, Shift+F5 | document | act on the session of the active document; disabled without one (`EDIT-PLUG-46`) |
| Plugin dock | global | the findings link to the document of their run; one output tab per plugin, not per document, each run headed by its document; a session tab per session, named by its document (`EDIT-PLUG-15/17/32/35/42`) |
| Git menu, Repository dock | global | the repository of the active tab (a diff tab: of its target's file); a tab of another work tree rebuilds the dock, its view state restored; no work tree, a placeholder (`EDIT-GIT-2/3/16`) |
| RepositoryService | per work tree | one per root while the editor runs; its snapshot, its queue of git commands, its watcher; reads only while the Repository dock is shown (`EDIT-GIT-6`) |
| Commit dialog, branches | the repository | the index and the branches of the active tab's work tree; a switch saves the unsaved tabs of that work tree first (`EDIT-GIT-14/19`) |
| Disk state, disk bar | document | each tab with a file remembers its bytes; the watcher, the activation and a git verb check them; a clean tab reloads, an edited one shows its bar (`EDIT-DOC-13..17`) |
| Command-line files (GUI) | global | each opens a tab, the last one active (`EDIT-DOC-7`); `--inspect` makes them inspection tabs (`EDIT-MODE-6`) |

## Lifetimes and traps

- The git docks never hold a document: a closed tab only drops its open and unsaved marks;
  a service outlives the tabs of its work tree.
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
- The tree and the properties hold a diagram's model and scene: a text or a diff tab drops them
  before its page shows, so no index of a background model stays linked.
- The last column of the tree is stretched over its viewport: the dock widening measures the
  contents (`sizeHintForColumn`), the column width would grow the dock on every activation.
