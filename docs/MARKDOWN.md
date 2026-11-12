# The Markdown Rendering — Architecture

**Document version:** 0.1 (2026-11-12). The requirements are `EDIT-MODE-14`, `EDIT-MODE-17` and
`EDIT-TEXT-14` in `EDITOR-SPEC.md`.

The editor renders Markdown itself: a read-only Markdown tab (`docs/MODES.md`, Text tabs) and,
when the preference is on, a Markdown comment body on the canvas. A TeX formula is drawn by
JKQTMathText, an SVG image by QtSvg after its markers are expanded; no browser engine, no TeX
installation and no network are involved.

```
 TextDocument::showPage()                    CyberiadaSMEditorCommentItem
   read-only Markdown tab                      body not edited + renderMarkdownComments
        | text, dir of the file                      | body, dir of the .graphml
        v                                            v
 MarkdownView : QTextBrowser               EditableTextItem::document()
        \                                           /
         +--------> importMarkdown(doc, text, baseDir) <------+       markdown_importer.*
                     |                    |
     CYBERIADA_MD_IMPORTER=QT        CYBERIADA_MD_IMPORTER=MD4C
                     |                    |
   scanMath (md4c) -> $..$ to        md4c callbacks -> QTextCursor     markdown_md4c_reader.cpp
   ![](cyb-math:N); setMarkdown;     formulas and SVG images inserted
   images cyb-math:N / *.svg turned  as objects directly
   into objects
                     \                    /
                      v                  v
            QTextDocument: text, blocks, tables + objects
              MarkdownMathObject {tex, display}   MarkdownSvgObject {path, alt}
                      |                                   |
     installMarkdownObjects(doc): the layout handlers          markdown_objects.*
                      v                                   v
        MathObjectHandler                      SvgObjectHandler
          MathRenderer -> JKQTMathText            expandSvgMarkers -> QSvgRenderer
          a QPicture per (tex, font, colour)      a renderer per file (cached)
          math_renderer.*                         svg_markers.*
```

## Reading

`importMarkdown` fills a `QTextDocument` the same way for both readers (`EDIT-MODE-17`):

| Reader | Qt | How a formula and an SVG image get into the document |
|---|---|---|
| `QT` | 5.14+ | `scanMath` finds the spans with md4c (`MD_FLAG_LATEXMATHSPANS`; code is never math), each is replaced by the image `![](cyb-math:N)`, `QTextDocument::setMarkdown` (GitHub dialect) reads the text, and a pass over the fragments turns the `cyb-math:N` and `*.svg` images into objects |
| `MD4C` | 5.12+ | the callbacks of md4c (`MD_DIALECT_GITHUB`, the math spans) build the document with a `QTextCursor`, the formulas and the SVG images as objects |

`AUTO` takes `QT` when the Qt has `setMarkdown`. The `MD4C` reader gives the blocks the formats
Qt's reader gives (the heading sizes, the list indents, the code font), so the two documents
have the same structure; `l4-markdown` compares them.

## Formulas

* A span `$..$` is inline, `$$..$$` is display; a display formula alone in its paragraph
  centres the paragraph.
* `MathRenderer` parses the TeX with JKQTMathText (XITS fonts, embedded), at the size of the
  text around it, in its colour (the palette text colour in a tab). `\colon` reads as `:`.
* A formula is recorded once into a `QPicture` with its width, ascent and descent and replayed
  on every paint, on the screen and in an export alike.
* The baseline: Qt places an `AlignMiddle` object with the ascent `(h + x/2) / 2`, `x` the
  x-height of the font; the handler makes `h = max(2a - x/2, 2d + x/2)` for the formula's
  ascent `a` and descent `d` and draws its baseline at `(h + x/2) / 2`, the text's baseline.
* A formula JKQTMathText reports errors for is drawn as its source in red monospace, the errors
  in its tooltip. Built with `CYBERIADA_WITH_MATH=OFF`, every formula is drawn so.

## SVG images

* An image path is relative to the base directory — the directory of the tab's file or of the
  diagram; an untitled document has none, so only an absolute path is found.
* Qt 5 draws SVG Tiny 1.2, which has no markers, so `expandSvgMarkers` adds them before
  `QSvgRenderer` reads the file:

```
 <path d=".." style="..;stroke-width:w;marker-end:url(#m)" transform="T"/>
 -> the path as it is, then
 <g transform="T translate(end) rotate(angle) scale(w) translate(-refX,-refY)"> children of #m </g>
```

  * `marker-start` and `marker-end` of `path`, `line`, `polyline`, `polygon` (the elements
    SVG marks), set by an attribute, by `style` or inherited from a group;
  * the end point and the angle come from the path data (`M L H V C S Q T A Z`, absolute and
    relative) built into a `QPainterPath`; `orient="auto"`, a number of degrees, or
    `auto-start-reverse`;
  * `markerUnits="strokeWidth"` (the default) scales by the stroke width,
    `userSpaceOnUse` does not; a `viewBox` with `markerWidth`/`markerHeight` scales to fit;
  * `marker-mid` is not drawn.
* An image is drawn at its own size, scaled down to the text width; a missing or unreadable
  one as its alt text and path in a frame.
* An image that is not SVG stays a Qt image, found by the same base directory.

## The view

`MarkdownView` is the page of a read-only Markdown tab: a `QTextBrowser` whose document is
filled by `importMarkdown`. A link to a file or a site opens outside the editor, a `#anchor`
scrolls to the heading of that name; Ctrl+wheel zooms; a palette change renders the document
again for the new text colour.

## Comments on the canvas

With `renderMarkdownComments` (Preferences > Text, `--render-markdown`, the batch verb
`render-markdown on|off`) a comment body whose markup is Markdown is rendered into the
document of its `EditableTextItem`:

```
           render (setting on, Markdown body)            start of an edit
 rendered --------------------------------------> source -----------------> source + highlighter
    ^                                                                            |
    +---------------------------- editingFinished: write back, render ----------+
```

* The item keeps the body text: the write-back and the refresh compare it, never the rendered
  `toPlainText()`.
* A fragment link (`dChunk`) of a rendered body ends on the body box.
* The setting off draws the canvas exactly as before; the good files of the render tests stay.

## Build

| Part | Where | Licence |
|---|---|---|
| md4c 0.5.2 (`md4c.c`, `md4c.h`) | `3rdparty/md4c` | MIT |
| JKQTMathText of JKQtPlotter 5.0.0-beta2, the parts it needs of JKQTCommon | `3rdparty/jkqtmathtext` | LGPL-2.1+ |
| XITS fonts | `3rdparty/jkqtmathtext/jkqtmathtext/resources/xits` | OFL-1.1 |

* The options: `CYBERIADA_MD_IMPORTER` (`AUTO`, `QT`, `MD4C`; `QT` with a Qt older than 5.14
  stops the configuration) and `CYBERIADA_WITH_MATH` (`ON`).
* The vendored code is compiled as it is, as C++11 like the editor, into the static library
  `CyberiadaMathText`; `3rdparty/jkqtmathtext/README.md` lists the files and the version.

## Testing

* `l4-markdown`: `scanMath` (code spans and blocks, `\$`, `$$`, tables, the money text `$5
  and $6`), `expandSvgMarkers` (the XML of the examples), the two readers' structures, the
  baseline of an inline formula, the base directory.
* The render tests draw `tests/texts/*.md` and compare the images with the good files.
* The render test of `markdown-comments.graphml` with `--render-markdown`.
