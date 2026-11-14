# Color Schemes — Architecture

**Document version:** 0.1 (2026-11-14). The requirements are `EDIT-COLOR-1..8` in
`EDITOR-SPEC.md` §4.18.

The source editor of a text tab draws in the colors of a scheme: its background, text,
selection, line numbers and the highlighting. White and Dark are built in; the user edits
them and adds own schemes on the Text page of the Preferences. Everything else — the canvas,
the text dialogs, the exported images, the rendered Markdown and its PDF — keeps the White
colors as built in.

```
 :/Schemes/schemes/white.json, dark.json           QSettings textSchemes/*
          | builtinTextSchemes()                        | the changed colors, the user schemes
          v                                             v
 text_scheme.*  TextScheme  ------------------>  SettingsManager
 id, name, colors[role]                          getTextSchemes() / setTextSchemes()
 format(token kind)                              getTextScheme() / setTextScheme(id)
          |                                      currentTextScheme()
          | White as built in                         | textSchemeChanged
          v                                           v
 CodeStyle::format(kind)                     TextDocument::applyScheme()
 canvas, text dialogs, image export              |                      |
                                                 v                      v
                                      CodeEditor::setScheme    CodeHighlighter::setScheme
                                      palette: Base, Text,     its own colors, restyle()
                                      Highlight, the gutter

 MarkdownView, exportMarkdownPdf  ---  black on white, no scheme

 Preferences > Text: TextSchemePanel  --- a copy of the schemes --- apply() ---> SettingsManager
                     list, buttons, swatches, preview CodeEditor
```

## The colors

| Key | Draws | White | Dark |
|---|---|---|---|
| `background` | the page | `#ffffff` | `#000099` |
| `text` | the text, the cursor | `#000000` | `#ffff00` |
| `selection` | the selected text background | `#308cc6` | `#00aaaa` |
| `selectionText` | the selected text | `#ffffff` | `#000000` |
| `lineNumbersBackground` | the line number margin | `#f0f0f0` | `#000066` |
| `lineNumbers` | the line numbers | `#8c8c8c` | `#8080ff` |
| `keyword` | a keyword, a preprocessor word | `#0033b3` | `#ffffff` |
| `type` | a type | `#007f7f` | `#00ffff` |
| `string` | a string, a character | `#067d17` | `#55ff55` |
| `number` | a number | `#1750eb` | `#ff99ff` |
| `comment` | a comment, a Markdown comment | `#8c8c8c` | `#b0b0b0` |
| `heading` | a Markdown heading | `#871094` | `#ffffff` |
| `emphasis` | bold, italic, strike-through | `#9e5a00` | `#ffb000` |
| `code` | a code span, a block without a language | `#067d17` | `#55ff55` |
| `link` | a link, an image | `#1750eb` | `#00ffff` |
| `marker` | `#`, `>`, a bullet, a fence, a rule, `$` | `#8c8c8c` | `#b0b0b0` |
| `quote` | the text of a quote | `#5d6d7e` | `#c0c0c0` |
| `math` | a TeX formula | `#a31b5a` | `#ff99ff` |

The White highlighting colors are the ones the canvas always had. On `#000099` every Dark
color has a contrast of 6.6:1 or more, the line numbers 5.4:1. The find marks (`#ffeb3b`,
`#ff9800`, black glyphs) are the same in every scheme (`EDIT-FIND-4`).

## The schemes

A built-in scheme is a resource file:

```json
{ "name": "Dark", "colors": { "background": "#000099", "text": "#ffff00", ... } }
```

| Scheme | Id | Name | Copy, Rename, Delete, Restore |
|---|---|---|---|
| built in | the file name: `white`, `dark` | translated | Copy, Restore when changed |
| user | `user-<n>`, the smallest free `n` | as given, unique in any case | Copy, Rename, Delete |

## The storage

| Key | Value |
|---|---|
| `textSchemes/current` | the id of the current scheme, `white` by default; an unknown one is White |
| `textSchemes/list` | an array of `id`, `name` (a user scheme), and a key per color as `#rrggbb` |

A built-in scheme is in the list with its changed colors only, a user scheme with all of them;
the list is removed when nothing differs from the built-ins. A missing or malformed color is
the built-in one's, White's for a user scheme.

## The page

The group *Text editor colors* on the Text page of the Preferences, beside the font of the
diagrams; the dialog is wide enough for both.

| Event | Effect |
|---|---|
| a scheme chosen | its swatches and the preview |
| a swatch | a color dialog; the color changed in the copy, the preview follows |
| Copy... | a name asked; a user scheme with the colors shown, chosen |
| Rename... | a new name for a user scheme |
| Delete | the user scheme dropped, White chosen |
| Restore | the built-in colors of a built-in scheme |
| Ok, Apply | the schemes stored, the scheme shown current; the open text tabs follow at once |
| Cancel | nothing changes |
| Reset of the dialog | the built-in schemes, no user ones, White current |

## Code layout

| File | Part |
|---|---|
| `schemes/*.json`, `smeditor.qrc` | the built-in schemes |
| `text_scheme.*` | the color roles and keys, `TextScheme`, `builtinTextSchemes()`, the names |
| `settings_manager.*` | `textSchemes/*`, `currentTextScheme()`, `textSchemeChanged` |
| `code_highlighter.*` | `CodeStyle::format()` by White; `CodeHighlighter::setScheme()` |
| `code_editor.*` | `CodeEditor::setScheme()`: the palette and the margin |
| `text_document.*` | `applyScheme()` on a mode and on a change |
| `markdown_view.*` | the rendered page black on white |
| `dialogs/text_scheme_panel.*` | `TextSchemePanel`: the list, the buttons, the swatches, the preview |
| `dialogs/preferences_dialog.*` | the panel on the Text page, its load, apply and reset |
| `tests/l4/l4-colors.cpp` | the cases `EDIT-COLOR-1..8` |
