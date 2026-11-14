# Keyboard Shortcuts — Architecture

**Document version:** 0.1 (2026-11-14). The requirements are `EDIT-KEYS-1..12` in
`EDITOR-SPEC.md` §4.16.

Every built-in action of the window has a name, a list of default keys and, perhaps, the
keys the user gave it. The Preferences edit them; only the changes are stored. A text being
edited takes its keys first.

```
 smeditor_window.ui / .cpp                       dialogs/preferences_dialog.*
   actions with an objectName                      Shortcuts page: filter, tree by menu,
        | add(action, path)   once, initShortcuts()  key field, Assign / Add / Clear /
        v                                            Reset / Reset all
 ShortcutMap (owned by the window)   shortcut_map.*        | a copy of the keys,
   id -> action, path, default keys  <---------------------+ setKeys() on Ok / Apply
        |            ^
        | setShortcuts(keys)         | shortcuts/<id>, only the changes
        v            v
   QAction        SettingsManager (QSettings)      settings_manager.*
        |
        | changed()
        v
 CyberiadaSMEditorWindow -> CodeEditor::setWindowKeys(Undo keys + Redo keys)
```

## The actions

An action is registered under its `objectName`; the page shows it under its menu path
(`Edit > Tools > New State`), the toolbar-only ones under `Tools` and `View`. The actions
made in code get names: `actionSessionStep`, `...Run`, `...Pause`, `...Restart`, `...Stop`,
`...Post`, `...Load`, `...Edit`, `...Save`, `actionOutputPanel`, `actionPluginManager`,
`actionStructureTree`, `actionPropertiesPane`. The toolbar-only actions are added to the
window itself, so their keys act while the toolbar is hidden.

Not registered: the plugin actions and their toolbars' toggles, Open Recent, the text modes,
the Language and Markup entries — they come and go while the editor runs (`docs/PLUGINS.md`,
Deferred).

## The default keys

One table in `shortcut_map.cpp`; a name not in it has no key. A row is its written keys,
then the platform keys Qt names for it; a platform key some row writes is left to that row
(`Ctrl+G` is Grid, not the Find Next of a desktop). *new* marks the keys the editor did not
have before 0.62.

| Menu | Action | Keys | |
|---|---|---|---|
| File | New > HSM diagram | `Ctrl+N` | |
| | Open | `Ctrl+O` | |
| | Save, Save as | `Ctrl+S`, `Ctrl+Shift+S` | |
| | Export | `Ctrl+E` | |
| | Close, Exit | `Ctrl+W`, `Ctrl+Q` | |
| Edit | Undo | `Ctrl+Z` | |
| | Redo | `Ctrl+Shift+Z`, `Ctrl+Y` | *new*: `Ctrl+Y` |
| | Cut, Copy, Paste | `Ctrl+X`, `Ctrl+C`, `Ctrl+V` | |
| | Delete Element | `Del` | |
| | Find | the platform Find keys (`Ctrl+F`) | |
| | Find Next, Find Previous | `F3`, `Shift+F3` and the platform keys | |
| | Go to Line | `Alt+G` | *new* |
| Tools | Select | `Esc` | *new* |
| | Pan, Zoom | `H`, `Z` | *new* |
| | New State, Transition Tool | `S`, `T` | *new* |
| | New Comment, New Formal Comment | `C`, `Shift+C` | *new* |
| | New Initial, New Final, New Choice | `I`, `F`, `X` | *new* |
| View | Zoom In | `Ctrl++`, `Ctrl+=` | *new* |
| | Zoom Out, Fit Content | `Ctrl+-`, `Ctrl+0` | *new* |
| | Grid, Snap | `Ctrl+G`, `Ctrl+Shift+G` | *new* |
| | Next document, Previous document | the platform keys (`Ctrl+Tab`, `Ctrl+Shift+Tab`) | |
| | Preferences | `Ctrl+I` | |
| Document | Properties | `Alt+Return` | *new* |
| | Check | `F7` | *new* |
| Plugins > Session | Step, Run, Pause, Stop | `F10`, `F5`, `F6`, `Shift+F5` | |

Without a default key: the other element tools (terminate, the histories, the submachine
state, the entry and exit points, the state machine), Center Horizontally / Vertically, the
modes, Read-only, the geometry entries, Compare, Refresh, the other View toggles and docks,
Zoom to SM, the other session entries, Plugin manager, About.

## The keys of a text

A window key reaches its action only when the focused widget does not take it:

| Focus | Takes |
|---|---|
| a canvas text being edited | the typing and editing keys, the clipboard and its own Undo, `Esc` (abandons the edit, `EDIT-KEYS-11`) |
| a text tab | the typing and editing keys; the keys bound to Undo and Redo are left to the window (`EDIT-KEYS-12`) |
| the find field | the typing and editing keys, `Enter`, `Shift+Enter`, `Esc` (`EDIT-FIND-1/3`) |
| a line or a table of a dock or a dialog | the typing and editing keys |
| a focused point of a transition | `Del` removes the point (`EDIT-KEYS-9`) |

So a letter key picks its tool on the canvas and types in a text. The text items and the find
bar accept the `ShortcutOverride` of `Esc`; the text widgets of Qt do it for the typing keys.

## The storage

| Key | Value |
|---|---|
| `shortcuts/<id>` | the keys as portable text joined by `; `; empty — no key; absent — the default keys |

`load()` reads the group over the defaults; `setKeys()` writes the key or removes it when
the keys equal the defaults. A key left by a removed action is ignored. Reset of the
Preferences removes the group.

## The page

| Event | Effect |
|---|---|
| filter | the rows whose action, path or keys hold the text |
| a row chosen | its keys in the key field; a changed row is bold |
| Assign, Add | the key of the field replaces the keys of the row, or joins them |
| the key is another action's | the action is named; a confirmation takes the key from it |
| Clear | the row has no key |
| Reset, Reset all | the default keys of the row, of every row; a key they take back is taken from its new holder |
| Ok, Apply | `ShortcutMap::setKeys()` for the changed rows, the menus show the new keys at once |
| Cancel | nothing changes |

## Go to Line

Edit > Go to Line asks a line number from 1 to the number of lines, the current one
offered, and puts the cursor at its start, the line scrolled into view
(`TextDocument::goToLine`, shared with the line links of the plugin dock). Enabled in the
text tabs that show their source.

## The pending text

`CyberiadaSMEditorScene::commitTextEdit()` ends the edit of the focused text as a focus-out
does. The window calls it before New, Open, Save, Save as, Export, Close, Exit, a tab
switch, a mode change, Check, Compare, the geometry entries, a plugin run and a session
start, so that they see the text typed (`EDIT-KEYS-10`).

## Code layout

| File | Part |
|---|---|
| `shortcut_map.*` | `ShortcutMap`, the default table, the key list as text, the conflicts |
| `settings_manager.*` | `shortcuts/<id>`: read, write, remove, the group reset |
| `smeditor_window.*` | `initShortcuts()`, the action names, Go to Line, `commitPendingEdit()`, Del on a point |
| `dialogs/shortcuts_page.*` | `ShortcutsPage`: the tree, the key field, the copy of the keys |
| `dialogs/preferences_dialog.*` | the page as a tab, its load, apply and reset |
| `code_editor.*` | the window keys left to the window |
| `editable_text_item.*`, `cyberiadasm_editor_state_item.cpp` | `commitEditing()`, `cancelEditing()`, `Esc` |
| `cyberiadasm_editor_scene.*` | `commitTextEdit()` |
| `find_bar.*` | `Esc` ahead of the window |
| `text_document.*` | `goToLine()` |
| `tests/l4/l4-keys.cpp` | the cases `EDIT-KEYS-1..12` |
