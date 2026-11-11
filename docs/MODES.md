# The Document Modes — Architecture

**Document version:** 0.4 (2026-11-11) — stages 1 and 2 implemented (see Stages). The requirements are
`EDIT-MODE-1..16` in `EDITOR-SPEC.md` §4.14, the plugin parts `EDIT-PLUG-56..60`.

A tab holds one document in one **mode**: the format edited, the access, and the tools, the
docks and the plugins that come with them — as an image mode of GIMP (Image > Mode). The mode
shows in the tab and window titles and is chosen in Document > Mode. Beside the mode a tab has a
**read-only flag**, off by default. A diagram is drawn on the canvas; a text replaces the canvas
with a text editor as the page of its tab.

```
CyberiadaSMEditorWindow                                                          GLOBAL
 | File > New > HSM diagram | FA diagram | Text file > Plain text, Markdown, C, ... Python
 | Document menu ------------------------------------------- acts on the active tab
 | ModeTraits[mode]: the tools, the docks, the actions, the plugin id   (document_mode.h)
 | documents : QList<EditorDocument*>          documentStack page i = documents[i]->page()
 | QUndoGroup <- doc->undoStack()              updateActions() <- traits(doc), readOnly()
 |
 +-- EditorDocument (abstract QObject)                                          PER TAB
 |     mode() setMode()  readOnly() setReadOnly()          modeChanged, readOnlyChanged
 |     title() modeMarker() isModified() filePath()        load() save()
 |     page() : QWidget*  undoStack() : QUndoStack*        diagram() : nullptr for text, diff
 |
 +-- CyberiadaSMEditorDocument           hsm | fa          rw (default) | ro
 |     model + scene + view (as now)     inspection        ro always
 |     model.readOnly() = locked || inspected || the flag
 +-- DiffDocument                        diff              ro always
 |     base, target: CyberiadaSMModel read as inspected; Document::diff
 +-- TextDocument                        text | markdown | <language>    rw (default) | ro
       QTextDocument; page: CodeEditor, a rendered view for a read-only Markdown
```

## The modes

| Mode | Id | Page | Element tools | Tree, properties | Marker |
|---|---|---|---|---|---|
| HSM diagram | `hsm` | canvas | all | yes | — |
| HSM inspection | `inspection` | canvas, the stored geometry | Select, Pan, Zoom | yes, read-only | `[inspection]` |
| FA diagram | `fa` | canvas | Select, Pan, Zoom, State, Initial, Final, Transition, Comment, Formal comment | yes | `[FA]` |
| HSM diff | `diff` | two canvases | Select, Pan, Zoom | the change list | `[diff]` |
| Plain text | `text` | CodeEditor | the toolbar hidden | empty | `[text]` |
| Markdown | `markdown` | CodeEditor; rendered when read-only | the toolbar hidden | empty | `[Markdown]` |
| a language | `c`, `cpp`, `csharp`, `python`, a plugin's (`lua`) | CodeEditor | the toolbar hidden | empty | `[C++]` ... |

* A language mode is a language file: a built-in one of `syntax/`, or one a plugin brings in
  its `languages` (`EDIT-PLUG-60`, `PLUGINS.md` Discovery) — a new language needs no rebuild.
  Its stem is the id, its `name` the marker; the file gains `extensions`, the ones Open
  recognises (`cpp`: `.cpp .cxx .cc .h .hpp .hh .ino`).
* The read-only flag adds `ro` to the marker: the tab `a.graphml [FA, ro]*`, the window
  `a.graphml [FA, read-only][*]`; an HSM tab `a.graphml [ro]`.
* The traits decide which actions and docks a tab gets; the read-only flag, a plugin lock and
  the inspection then disable the editing ones as today.

## Mode transitions

```
     diagram tabs                      tabs of their own (read-only)          text tabs
 +-------+  set type/ fa  +------+  HSM inspection  +----------------+   +------+ +----------+
 |  HSM  |--------------->|  FA  |----------------->| HSM inspection |   | text | | markdown |
 |       |<---------------|      |<-----------------| read from the  |   +------+ +----------+
 +-------+  drop type/ fa +------+  HSM / FA:       | file without   |   | c cpp csharp      |
     |                       |      in place        | the recovery   |   | python            |
     |   Compare with...     |                      +----------------+   +-------------------+
     +-----------------------+--------------------->| HSM diff       |    any <-> any: the
     |   Plugins > ... (result: diff)               | base vs target |    highlighting only
     +--------------------------------------------->|                |
                                                    +----------------+
```

* **HSM <-> FA** edits the document: the meta `type/ fa` set or removed as one undo step. The mode
  follows the meta, so an undo, a properties edit or a plugin result moves the tab with it, and
  a file with `type/ fa` opens in FA.
* **HSM inspection** needs the document read without the recovery (the endpoint snap of
  `EDIT-EDGE-1` happens at the load), so it opens a new tab: the file when the source tab is
  saved, its snapshot otherwise. HSM or FA on an inspection tab converts it in place, as leaving
  the inspection did (`EDIT-INSPECT-4`).
* **HSM diff** is never reached by a switch: Compare with... or a plugin opens it.
* **The text modes** switch freely: the text stays, the highlighting and the marker change.

## Inspection and read-only

```
                        loaded with      drawn from          edits     recovery, [U] laws
 HSM, FA                recovery         the editing layout  yes       kept
 HSM, FA + read-only    recovery         the editing layout  refused   kept
 HSM inspection         no recovery      the stored geometry refused   suspended
 HSM diff (both sides)  no recovery      the stored geometry refused   suspended
```

* The model holds `inspected` (set before the load, which then skips the recovery) and the
  flag of the tab; `readOnly() = locked || inspected || the flag`. The items follow its
  `inspectedChanged`, the properties panel its `readOnlyChanged`; nothing is global.
* The Open dialog's inspection box and `--inspect` open inspection tabs.
* Leaving an inspection tab (`leaveInspection`) is one model reset: the recovery of the
  endpoints, then the scene reloaded in the editing layout adopts the regions; no undo step.

## The FA mode

* The traits remove the creation tools EXT-FA has no element for: choice, terminate, the
  histories, the submachine state, the entry and exit points, a second state machine.
* Nothing else is filtered: a paste, a plugin result or a loaded file may bring any element. An
  FA draft is a valid document (EXT-FA 5); Document > Check lists what it breaks.
* Check runs `Document::check_fa()` and shows the findings in Diagnostics, linked to their
  elements, as a plugin's findings (`EDIT-PLUG-15`).
* View > FA symbols (off by default) draws a state's entry action as its output symbol alone,
  without `entry/` (EXT-FA 3.1); the document is not changed.

## The diff tab

```
 Compare with...: a tab | a file | the saved file          a plugin action, result: diff
            |                                                   | ${output} = the base
            v                                                   v
 +---------------------------------------------------------------------------------+
 | DiffDocument                                                                    |
 |  target <- the snapshot of the source tab        (origin: the tab)              |
 |  base   <- the origin: a tab, a file, the saved file, a plugin run              |
 |  both read as inspected: any format the library reads, an empty base = empty    |
 |  DocumentDiff = base.diff(target, DiffOptions{geometry})                        |
 +---------------------------------------------------------------------------------+
      | pairs + flags                       | marks                     ^ Refresh:
      v                                     v                           | both sides
 +------------------------+    +------------------------------------+   | again from
 | the change list (tree  |    |  base view       |   target view   |   | their origins;
 | dock): machine >       |--->|  removed,        |   added,        |   | a plugin base
 | element: what differs  |    |  changed         |   changed       |   | re-runs it
 +------------------------+    +------------------------------------+---+
   a row selects and centres the element in both views; the marks reuse RunMark
```

* The pairs come from the library, never from the editor: the same `Document::diff` the git
  plugin prints. Without Document > Geometry the ids, the geometry and the
  `cyberiadaEditorView` metainformation are not reported, as `hsm-git diff` does.
* The title is `<target> ↔ <base>`; the base is named by its tab title, its file name, `saved`,
  or, for a plugin run, the values of the action's string parameters (`HEAD`), else the action
  label.
* A closed source tab leaves the target as it was taken; Refresh keeps it then.

The git plugin hands the revision of a diagram to a diff tab:

```
 Plugins > Git > Compare with a Revision...      (modes: hsm, fa, inspection; when: git)
   editor -- ${input}: the tab's unsaved copy ------> hsm-git revision DOC --revision=REV
                                                        rev-parse, cat-file blob, a rename
   editor <- ${output}: the revision file ----------- (empty: not in the revision)
   editor: a diff tab "light.graphml ↔ HEAD", base = ${output}, target = ${input}
```

## Text tabs

```
 File > New > Text file > <mode>       Open: the extension -> the mode (else text)
            |                                      |
            v                                      v
 +--------------------------------------------------------------------------+
 | TextDocument: QTextDocument (UTF-8), mode, file path                     |
 |  QTextDocument::undoCommandAdded -> a proxy step on its QUndoStack       |
 +--------------------------------------------------------------------------+
            | rw                                     | read-only Markdown
            v                                        v
 +----------------------------------+     +-------------------------------+
 | CodeEditor: QPlainTextEdit, the  |     | QTextBrowser::setMarkdown:    |
 | line numbers, CodeHighlighter by |     | the rendered document         |
 | the language or the Markdown     |     |                               |
 +----------------------------------+     +-------------------------------+
```

* The proxy step lets the window's Undo and Redo, bound to the `QUndoGroup`, drive the text's own
  undo, and the stack's clean state gives the `*` of the title.
* Save writes the text as it is, in UTF-8; Save As proposes the mode's first extension.

## The menus

```
File                              Document
  New            > HSM diagram       Mode         > (*) HSM diagram
                   FA diagram                       ( ) HSM inspection
                   Text file   >                    ( ) FA diagram
  Open...            Plain text                     ( ) HSM diff
  ...                Markdown                       ---------------
                     C, C++,                        ( ) Plain text
                     C#, Python                     ( ) Markdown
                                                    Code > ( ) C ( ) C++ ( ) C# ( ) Python
                                     [ ] Read-only
                                     Language >  Markup >
                                     Properties...
                                     Check
                                     Reconstruct geometry   Update geometry
                                     Compare with...   [ ] Geometry   Refresh
```

* Document stands between Edit and View. Edit keeps Cut, Copy, Paste, Delete, the centring,
  Undo, Redo and Tools; Language, Markup and the two geometry actions move to Document; Edit >
  Inspector mode is gone.
* A Mode entry the tab cannot reach is disabled; Read-only is checked and disabled in an
  inspection or a diff tab; Geometry and Refresh act on a diff tab only; Check on an FA tab only.
* Properties... selects the Document element in the structure tree and the properties.

## Plugins

* An action's `modes` lists the modes it works in, `["hsm", "fa"]` by default, so the plugins
  written before the modes keep their place. In another tab the action is greyed out with the
  modes in the reason (`EDIT-PLUG-56`).
* In a text tab `${input}` and `${output}` are `input.<ext>`, `output.<ext>` holding the text
  (`EDIT-PLUG-57`); a `files` result opens a listed text file in a tab on a click.
* `result: "diff"` opens a diff tab with `${output}` as the base (`EDIT-PLUG-58`).
* An unknown `result` greys out that action only, as an unknown `when` token does
  (`EDIT-PLUG-59`).
* A plugin's `languages` add language modes and highlighting at the scan; the built-ins come
  first, a clash is a warning (`EDIT-PLUG-60`). The language menus are rebuilt on Rescan.

## Batch mode

| Verb, option | Does |
|---|---|
| `new-document [<mode>]` | a new untitled tab in the mode (`hsm` by default) |
| `mode <id>` | Document > Mode; `inspection` opens the inspection tab |
| `read-only on\|off` | Document > Read-only |
| `compare <index\|title\|file>` | a diff tab: the base given, the active tab the target |
| `geometry on\|off`, `refresh` | Document > Geometry, Refresh of the active diff tab |
| `--inspect` | the command-line files open in inspection tabs |
| `--dump-tabs` | `i <title>[*] <mode>[ ro] [active]` |
| `--dump` | a text tab: `== text`; a diff tab: `== diff` (the pairs and their flags) |

## Code layout

| File | Holds |
|---|---|
| `editor_document.h/.cpp` | `EditorDocument`: the mode, the flag, the title marker |
| `document_mode.h/.cpp` | the mode ids, `ModeTraits`, the extensions of the text modes |
| `cyberiadasm_editor_document.*` | the diagram tab, now an `EditorDocument` |
| `diff_document.h/.cpp` | `DiffDocument`, the base origins, the change list model |
| `text_document.h/.cpp`, `code_editor.h/.cpp` | `TextDocument`, `CodeEditor` |
| `smeditor_window.*`, `smeditor_window.ui` | the Document menu, File > New, the traits applied |

## Stages

1. `EditorDocument`, inspection tabs, the read-only flag, the Document menu — done: the
   menu has Mode (HSM diagram, HSM inspection), Read-only, Language, Markup, Properties...
   and the geometry actions; File > New, Check, Compare with..., Geometry and Refresh come
   with their stages.
2. The FA mode — done: Mode > FA diagram writes `type/ fa` as one undo step
   (`CyberiadaSMModel::setFA`), the tab follows the type, File > New > FA diagram marks the
   empty document until its first machine, the FA tools (`modeHasTool`, also refused by the
   batch `tool` gesture), Document > Check into Diagnostics, View > FA symbols.
3. The text tabs (Java and JavaScript leave `syntax/`).
4. The diff tabs.
5. The plugin `modes`, the text input, `result: "diff"`, the plugin `languages`.
6. The git plugin: Compare with a Revision....

## Decisions of 0.1

* The mode and the flag belong to a tab; inspection stops being an application mode.
* Inspection is a mode of its own, not a read-only HSM: it changes how the document is read.
* The inspection and the diff open tabs of their own; the source tab is never changed.
* FA is the meta `type/ fa`, not a view: the file says what it is.
* A language is a mode; the language files are the one list of the languages.
* The diff is computed by the editor; a plugin only supplies the base, so the git plugin and
  `Compare with...` show the same pairs.

## Decisions of 0.2

* A new language comes with a plugin (`languages`), not with a rebuild; the built-in ones stay
  compiled in and are never replaced.
