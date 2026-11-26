# Color Schemes — Architecture

**Document version:** 0.9 (2026-11-26), the colors of the block cursor; 0.8: the steady block cursor; 0.7: the operators and the preprocessor lines, the Dark code colors; 0.6: the mode line; 0.5: the text width of the fill; 0.4: the cursor and the long lines of the text editors. The requirements are `EDIT-COLOR-1..8` in
`EDITOR-SPEC.md` §4.18; the editor font beside them is `EDIT-MODE-27`.

The source editor of a text tab draws in the colors of a scheme: its background, text,
selection, block cursor, line numbers and the highlighting. White and Dark are built in; the user edits
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
| `text` | the text, the line cursor | `#000000` | `#ffff00` |
| `selection` | the selected text background | `#308cc6` | `#00aaaa` |
| `selectionText` | the selected text | `#ffffff` | `#000000` |
| `blockCursor` | the block cursor (`EDIT-MODE-29`) | `#000000` | `#ffff00` |
| `blockCursorText` | the character under the block cursor | `#ffffff` | `#000099` |
| `lineNumbersBackground` | the line number margin | `#f0f0f0` | `#000066` |
| `lineNumbers` | the line numbers | `#8c8c8c` | `#8080ff` |
| `keyword` | a keyword | `#0033b3` | `#ffffff` |
| `type` | a type | `#007f7f` | `#ffffff` |
| `string` | a string, a character | `#067d17` | `#55ffff` |
| `number` | a number | `#1750eb` | `#55ff55` |
| `comment` | a comment, a Markdown comment | `#8c8c8c` | `#b0b0b0` |
| `operator` | an operator sign, a bracket, a separator | `#000000` | `#ffffff` |
| `preprocessor` | a preprocessor line but its strings, numbers and comments | `#0033b3` | `#55ff55` |
| `heading` | a Markdown heading | `#871094` | `#ffffff` |
| `emphasis` | bold, italic, strike-through | `#9e5a00` | `#ffb000` |
| `code` | a code span, a block without a language | `#067d17` | `#55ff55` |
| `link` | a link, an image | `#1750eb` | `#00ffff` |
| `marker` | `#`, `>`, a bullet, a fence, a rule, `$` | `#8c8c8c` | `#b0b0b0` |
| `quote` | the text of a quote | `#5d6d7e` | `#c0c0c0` |
| `math` | a TeX formula | `#a31b5a` | `#ff99ff` |

The White highlighting colors are the ones the canvas always had, a preprocessor line now in
the keyword blue to its end; outside a text editor an operator has no color of its own and keeps
the text color of the item or the dialog. On `#000099` every Dark
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
the built-in one's, White's for a user scheme, but a user scheme stored before 0.7, without
`operator` and `preprocessor`, takes its own `text` and `keyword` for them, as it drew them, and
one stored before 0.9, without `blockCursor` and `blockCursorText`, its own `text` and
`background`, as the inverted block drew a plain text.

## The page

The group *Text editors* on the Text page of the Preferences, beside the font of the
diagrams; the dialog is wide enough for both.

```
 Text editors
   Font    [Cyberiada Mono       v] [12 pt]
   Cursor  [Line  v]                       Line | Block
   Long lines [Scroll v]                   Scroll | Wrap
   Text width [ 90 ] characters [ ] Fill as you type     20..200 (EDIT-MD-22)
   Scheme  [White               v]
   [Copy...] [Rename...] [Delete] [Restore]
   swatches: the editor | the code | the Markdown colors, a column each
   preview: a Markdown text with a C++ block

 CodeEditor::applyCursor(): setCursorWidth(block ? 0 : 1) on editorCursorChanged and on every
   font change. The line is Qt's, blinking as the system sets; the block is the editor's own,
   steady (EDIT-MODE-29): Qt's cursor hidden (width 0, its blink then draws nothing), paintEvent
   fills the rect of a space at the cursor after the text in blockCursor and draws the layout
   of the cursor's block again clipped to it, the character at the cursor a FormatRange in
   blockCursorText on blockCursor, so the character keeps its font (bold, italic); setScheme
   gives the two colors, an editor without a scheme (a text diff side) takes the palette's Text
   and Base; drawn while the editor has the focus and takes
   the keys (writable or selectable by the keyboard), the old and the new rect repainted when the
   cursor moves, the focus comes or goes, the whole view when the text scrolls (the scroll moves
   the pixels of the old block, QPlainTextEdit scrolls by lines: scrollContentsBy and
   updateRequest with a dy) (not the overwrite mode: typing stays)
 CodeEditor::applyWrap(): WidgetWidth + WrapAnywhere, the right fringe (WrapFringe) beside the
   viewport, a hook arrow per row that continues (continuationRows: every layout line of a
   block but its last); a DiffTextPane never wraps (setWrapAllowed(false))
```

| Event | Effect |
|---|---|
| the font, its size | the text editor font (`EDIT-MODE-27`), the preview in it |
| the cursor | Line or Block (`EDIT-MODE-29`), the preview with it (it shows when clicked); the colors of the block are the swatches Block cursor and Text under the cursor of the scheme |
| the long lines | Scroll or Wrap (`EDIT-MODE-30`), the preview with it |
| the mode line (`EDIT-MODE-31`) | drawn in AlternateBase (the line-number background) with a rule and the dim parts in PlaceholderText (the line numbers), the text in Text: a scheme colors it as it colors the margin; the preview has none |
| the text width, Fill as you type | the width of the fill and its switch (`EDIT-MD-20..22`), on Apply; the preview stays |
| a scheme chosen | its swatches and the preview |
| a swatch | a color dialog; the color changed in the copy, the preview follows |
| Copy... | a name asked; a user scheme with the colors shown, chosen |
| Rename... | a new name for a user scheme |
| Delete | the user scheme dropped, White chosen |
| Restore | the built-in colors of a built-in scheme |
| Ok, Apply | the font, the cursor, the long lines and the schemes stored, the scheme shown current; the open text tabs follow at once |
| Cancel | nothing changes |
| Reset of the dialog | the bundled font at 12 pt, the line cursor, the long lines scrolled, the built-in schemes, no user ones, White current |

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
| `fontmanager.*` | `FontManager::editorFont()`: the stored family or the bundled one, the point size |
| `dialogs/text_scheme_panel.*` | `TextSchemePanel`: the font, the list, the buttons, the swatches, the preview |
| `dialogs/preferences_dialog.*` | the panel on the Text page, its load, apply and reset |
| `tests/l4/l4-colors.cpp` | the cases `EDIT-COLOR-1..8` |
