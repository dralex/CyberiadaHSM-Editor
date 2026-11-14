# Autoreplace — Architecture

**Document version:** 0.1 (2026-11-14). The requirements are `EDIT-AUTO-1..8` in
`EDITOR-SPEC.md` §4.17.

A typed pattern (`--`, `<<`, `(c)`) becomes its character (`–`, `«`, `©`) the moment it is
complete — in a text for people only, never in code. The rules are a table of the
Preferences.

```
 a typed key
     |
     v
 EditableTextItem::keyPressEvent / inputMethodEvent      canvas texts
 AutoreplaceWatcher (key filter + contentsChange)        CodeEditor, MultilineTextDialog
 AutoreplaceWatcher (key filter + textEdited)            name line edits
     |  text, cursor, kind
     v
 autoreplaceAllowed(kind, text, pos) --- markdownCodeRanges()   markdown_importer.*
     |  yes
     v
 autoreplaceMatch(text before the cursor, rules) -> from, to    autoreplace.*
     |                                  ^
     v                                  | rules, enabled; autoreplaceChanged
 one edit block: from -> to        SettingsManager  <---  Preferences, Autoreplace page
```

## The rules

`AutoreplaceRule` is `from`, `to` and `enabled`. `autoreplaceMatch()` takes the text before
the cursor and gives the enabled rule with the longest `from` that ends there, compared as
written. The default rules:

| Typed | Becomes | | Typed | Becomes |
|---|---|---|---|---|
| `--` | `–` | | `->` | `→` |
| `–-` (a third `-`) | `—` | | `<-` | `←` |
| `<<` | `«` | | `=>` | `⇒` |
| `>>` | `»` | | `+-` | `±` |
| `...` | `…` | | `!=` | `≠` |
| `(c)`, `(с)` | `©` | | `<=` | `≤` |
| `(r)`, `(р)` | `®` | | `>=` | `≥` |
| `(tm)`, `(тм)` | `™` | | | |

The Cyrillic `с`, `р`, `тм` are the same keys in the Russian layout.

## Where

The kind of a text is known where it is edited: the code role and the markup of a canvas
text (`getCodeRole()`, `getCodeMarkup()`), `CyberiadaSMModel::isMarkupBody()` for a comment
body, the mode of a text tab, the property of a panel row.

| Text | Kind | Autoreplace |
|---|---|---|
| the name of a state, a state machine, a pseudostate, a comment, a document | prose | yes |
| the body of an informal comment, plain | prose | yes |
| the body of a comment in Markdown, a Markdown tab | Markdown | yes, outside the exclusions |
| the `description` of the metainformation | prose | yes |
| an entry, exit or internal action, a transition label, a trigger, a guard, a behaviour | code | no |
| the body of a formal comment not in Markdown, a component comment | code | no |
| an id, a markup, a submachine, the other metainformation | value | no |
| a tab of a language, a Plain text tab | code, unknown | no |
| the find field, the fragment of a comment link, a plugin parameter, a session line | value | no |

A trigger is an event name: the model refuses one with a typographic character.

## Markdown exclusions

`markdownCodeRanges()` reads the source with md4c, as `markdownSourceRanges()` does, and
gives the ranges where nothing is replaced:

| Range | Example |
|---|---|
| a code span, an unclosed one to the end of its line | `` `x--` ``, `` `a << `` |
| a fenced or indented code block, an unclosed fence to the end of the text | ` ``` ` |
| a formula, an unclosed `$` to the end of its line | `$a <= b$` |
| an HTML tag or comment, an unclosed `<!--` to the end of the text | `<!-- note -->` |
| the target of a link or an image, a bare URL | `[x](a...b)` |
| a line of `-`, `|`, `:` and spaces — a rule, a table border | `|---|:--|` |
| the `>` that open a quote | `>> quoted` |

## The moment

Only a typed character is replaced: a key press or an input method commit that inserted
text at the cursor. A paste, a drop, an undo or redo, a plugin result, a loaded or rendered
text and the batch `set` verbs are never read. The batch `type` verb is typing.

The replacement is one edit block of its own:

| After a replacement | Effect |
|---|---|
| Undo | the typed characters are back, the cursor after them |
| Backspace, the cursor not moved since | the same |
| the next typed character | it is read against the rules again, so `–` and `-` make `—` |

A pattern restored by Undo or Backspace stays as typed.

## The storage

| Key | Value |
|---|---|
| `autoreplace/enabled` | the switch, on by default |
| `autoreplace/rules` | an array of `from`, `to`, `enabled`; absent — the default rules |

The rules are written when they differ from the defaults, removed when they equal them.

## The page

| Event | Effect |
|---|---|
| *Replace as you type* | the switch |
| a cell edited | the pattern or its replacement; the check box of a row switches the rule |
| the last, empty row filled | a new rule; a row with an empty pattern is dropped |
| a pattern written twice | the later row is refused, the first one shown |
| Restore defaults | the default rules |
| Ok, Apply | the rules stored, the open texts follow at once |
| Cancel | nothing changes |

## Code layout

| File | Part |
|---|---|
| `autoreplace.*` | `AutoreplaceRule`, the default rules, `autoreplaceMatch()`, `autoreplaceAllowed()`, `AutoreplaceWatcher` |
| `markdown_importer.*` | `markdownCodeRanges()` |
| `settings_manager.*` | `autoreplace/*`, `autoreplaceChanged` |
| `editable_text_item.*` | the hook of the canvas texts, the kind by the role and the markup |
| `text_document.*`, `code_editor.*` | the watcher of a Markdown tab |
| `dialogs/multilinetextdialog.*` | the watcher of a prose or Markdown text |
| `cyberiadasm_properties_widget.*`, `cyberiadasm_view.*` | the watcher of the name rows and of the tree rename |
| `dialogs/preferences_dialog.*` | the Autoreplace page |
| `tests/l4/l4-autoreplace.cpp`, `tests/scripts/auto-*` | the cases `EDIT-AUTO-1..8` |
