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
 EditableTextItem::keyPressEvent / inputMethodEvent      canvas texts, kind by role and markup
 CodeEditor::keyPressEvent / inputMethodEvent            text tabs, kind by the mode
     |  before(key): Backspace gives the pattern back
     |  typed(cursor, kind): after a key that changed the text
     v
 Autoreplacer (one per text)                             autoreplace.*
     |  autoreplaceMatch(text before the cursor, rules) -> rule
     |  autoreplaceAllowed(kind, text, from, to) -- markdownProseAt()
     v                                  ^
 one edit block: from -> to             | rules, enabled; autoreplaceChanged
                                   SettingsManager  <---  Preferences, Autoreplace page
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

`markdownProseAt()` reads the source up to the pattern typed, by its lines and characters:
the text being typed is mostly not closed yet, which a Markdown parser would read as plain
text. Nothing is replaced in:

| Range | Example |
|---|---|
| a code span, an unclosed one to the end of its line | `` `x--` ``, `` `a << `` |
| a fenced code block, an unclosed fence to the end of the text; a line indented by four spaces after an empty line, not under a list item | ` ``` ` |
| a formula, an unclosed `$` to the end of its line, `$$` to the end of the text; a `$` before a digit or a space opens none | `$a <= b$` |
| an HTML tag or comment, an unclosed `<!--` to the end of the text | `<!-- note -->` |
| the target of a link or an image, a bare URL | `[x](a...b)` |
| a line of `-`, `=`, `|`, `:` and spaces — a rule, the underline of a heading, a table border | `|---|:--|` |
| the `>` that open a quote | `>> quoted` |

## The moment

Only a typed character is replaced: a key press or an input method commit that inserted
text at the cursor, the text changed by it. A paste, a drop, an undo or redo, a plugin
result, a loaded or rendered text are never read. Of the batch verbs `type` is typing;
`edit-text` sets a text as written (`suspendAutoreplace()`).

The replacement is one edit block of its own:

| After a replacement | Effect |
|---|---|
| Undo | the typed characters are back, the cursor after them |
| Backspace, the cursor not moved since | the same |
| the next typed character | it is read against the rules again, so `–` and `-` make `—` |

A pattern restored by Undo or Backspace stays as typed: the `Autoreplacer` keeps the place
of its last replacement and replaces nothing over the pattern found there again. The Undo
of a text tab puts the cursor at the change, after the characters given back.

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
| a cell edited | the pattern or its replacement; the check box beside a pattern switches the rule |
| the last, empty row filled | a new rule; a row with an empty pattern is dropped |
| a pattern written twice | the later cell is emptied, the first row shown |
| *Replace as you type* off | the table greyed |
| Restore defaults | the default rules |
| Ok, Apply | the rules stored, the open texts follow at once |
| Cancel | nothing changes |

## Code layout

| File | Part |
|---|---|
| `autoreplace.*` | `AutoreplaceRule`, the default rules, `autoreplaceMatch()`, `markdownProseAt()`, `autoreplaceAllowed()`, `Autoreplacer` |
| `settings_manager.*` | `autoreplace/*`, `autoreplaceChanged` |
| `editable_text_item.*` | the hook of the canvas texts, `autoreplaceKind()` by the role and the markup |
| `code_editor.*`, `text_document.*` | the hook of a text tab, its kind by the mode; the cursor of Undo and Redo |
| `batch_script.cpp` | `edit-text` as written |
| `dialogs/multilinetextdialog.*` | planned: a prose or Markdown text |
| `cyberiadasm_properties_widget.*`, `cyberiadasm_view.*` | planned: the name rows and the tree rename |
| `dialogs/autoreplace_page.*` | `AutoreplacePage`: the switch, the table, the copy of the rules |
| `dialogs/preferences_dialog.*` | the page as a tab, its load and apply |
| `tests/l4/l4-autoreplace.cpp`, `tests/scripts/auto-type.script` | the cases `EDIT-AUTO-1..8` |
