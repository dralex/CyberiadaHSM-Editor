# Find — Architecture

**Document version:** 0.1 (2026-11-13). The requirements are `EDIT-FIND-1..9` in
`EDITOR-SPEC.md` §4.15.

One find bar under the document area searches the text the active tab shows: the source or
the rendered page of a text tab, the drawn texts of a diagram. The matches are marked over the
text, yellow and the current one orange; nothing is written into a document, its undo stack
or its exports.

```
 Edit > Find (Ctrl+F)  Find Next (F3)  Find Previous (Shift+F3)
                 |
                 v
 FindBar (global, under documentStack)          find_bar.*
   field, Aa / W / .*, up, down, "n of m", close
                 | queryChanged(FindQuery) / nextRequested / previousRequested / closed
                 v
 CyberiadaSMEditorWindow --- active EditorDocument ---------- editor_document.h
                 |   setFindQuery(q, navigate)  moveFind(+-1)  clearFind(keep)  findSeed()
                 |   <- findResultChanged(current, total, more)
       +---------+-------------------------------+
       v                                         v
 TextDocument                            CyberiadaSMEditorDocument --- DiagramFind
   the shown page:                         walk model->rootDocument()   diagram_find.*
   CodeEditor | MarkdownView               textItemsOf(scene item)     (the --dump-text order)
       | findRanges(toPlainText)                  | findRanges(text->toPlainText())
       v                                         v
   setExtraSelections                     EditableTextItem::setFindRanges(ranges, current)
   yellow, current orange                   paint(): boxes, then the glyphs
                                            EdgeGeometry::textRangeRects
                 \                               /
                  +--> FindQuery::expression() -> findRanges(text, re, limit)  find_query.*
```

## The query

`FindQuery` holds the field and the three options. `expression()` makes one
`QRegularExpression` of them, always with the Unicode properties, so that the case and the
word characters of the Cyrillic letters count:

| Options | Pattern |
|---|---|
| plain | `QRegularExpression::escape(text)` |
| Regular expression | `text` as written; an invalid one matches nothing |
| Whole words | `E(?:` ... `)E`, `E` = `(?:(?<!\w)\|(?!\w))`: an end never falls inside a word, so `/reset` matches `entry/reset()` |
| Match case off | `CaseInsensitiveOption` |

`findRanges()` lists the non-empty matches `[from, to)` of a text, at most the limit
(`FIND_MATCH_LIMIT`, 10000, for all the texts of a tab together), and says when more were left.
The positions are those of `QTextDocument`: `toPlainText()` keeps one character per block
break and per object.

## The current match

Every search keeps a *place* and makes current the first match at or after it, wrapping to
the first:

| Search | Place |
|---|---|
| field or option changed | the current match, else the text cursor / the first selected element |
| the text changed (`EDIT-FIND-8`) | the current match; nothing is navigated |
| tab switch | the text cursor / the first selected element; nothing is navigated |
| Find Next / Previous | the match after / before the current one |

A text place is a `QTextCursor` at the start of the current match, which the edits move with
the text. A diagram place is the triple (element number in the walk, text number in the
element, offset); an element gone keeps its old number. Only the field, an option and Next/Previous navigate:
a text tab moves its cursor to the match, a diagram selects the element and scrolls. A search
after an edit must not move the cursor or the selection, which would end the edit of a canvas
text.

## Text tabs

`TextDocument` searches the widget its stack shows (`showsRendered()`): the `CodeEditor`
(`QPlainTextEdit`) or the `MarkdownView` (`QTextBrowser`); the hidden one has its marks
cleared. Both documents' `contentsChanged` and `showPage()` search again, so an edit, an undo,
a plugin result, a mode switch and the read-only flag keep the marks right.

The marks are `QTextEdit::ExtraSelection`s, the background yellow or orange and the foreground
black. While the bar is open the cursor goes to the match without a selection: a selection is
drawn over the extra selections in the palette colour, so the orange would not show.
`clearFind(true)`, the bar closed, selects the current match.

## Diagrams

`DiagramFind` belongs to the `CyberiadaSMEditorDocument` and reads its model and scene:

```
 rootDocument --(element tree, the --dump-text order)--> scene->getMap()[id]
   --> textItemsOf(item)   the visible EditableTextItems, by font role, then top-down
     --> findRanges(text->toPlainText())          the drawn text, a rendered body included
       --> matches {QPointer<EditableTextItem>, element #, text #, from, to}
         --> item->setFindRanges(its ranges, the current one or -1)
```

`EditableTextItem::paint()` fills the boxes before `QGraphicsTextItem::paint()` draws the
glyphs; the rects come from the item's layout (`EdgeGeometry::textRangeRects`, the line walk of
`textRangeLines`), so a move, a resize or a wrap needs no new search. A cut text is clipped as
its glyphs are; a range not wholly in the visible part puts its box over the ellipsis.

The scene says when the drawn texts may have changed: `EditableTextItem` reports its
`contentsChanged`, its joining a scene and its visibility, the scene its removed rows and its
reset; the reports are joined by a zero timer into one `CyberiadaSMEditorScene::textsChanged()`,
which `DiagramFind` answers with a search while the query is not empty.

An export clears the marks for its rendering (`FindMarksGuard`, beside `RunMarksGuard`,
`cyberiadasm_render.cpp`); the scene flag `findMarksShown()` is read by the paint.

## The window

| Event | Effect |
|---|---|
| Find | the bar shown, the field filled by `findSeed()` (one selected line), focused and selected |
| field or option changed | `active->setFindQuery(q, true)`, the counter from `findResultChanged` |
| Find Next / Previous | the bar shown if hidden; `active->moveFind(+1 / -1)` |
| Esc, close | `active->clearFind(true)`, the bar hidden, the focus to the page |
| tab switch | `old->clearFind(false)`, `new->setFindQuery(q, false)` while the bar is shown |
| diff tab | `canFind()` false: the actions disabled, the bar greyed |

## Code layout

| File | Part |
|---|---|
| `find_query.*` | `FindQuery`, `FindRange`, `findRanges()` |
| `find_bar.*` | `FindBar`: the widget, its keys and the counter |
| `editor_document.h` | the find interface of a tab: `canFind`, `setFindQuery`, `moveFind`, `clearFind`, `findSeed`, `findResultChanged` |
| `text_document.*` | the text tab search and its extra selections |
| `diagram_find.*` | `DiagramFind`: the walk, the matches, the place, the navigation |
| `editable_text_item.*` | `setFindRanges`, the boxes in `paint()` |
| `edge_geometry.*` | `textRangeRects` |
| `cyberiadasm_editor_scene.*` | `textsChanged()`, `findMarksShown()` |
| `cyberiadasm_render.cpp` | `FindMarksGuard` |
| `smeditor_window.*` | the actions, the bar, the switch |
| `tests/l4/l4-find.cpp` | the cases `EDIT-FIND-1..9` |
