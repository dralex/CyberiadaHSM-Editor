# The Markdown Rendering — Architecture

**Document version:** 0.11 (2026-11-14). The requirements are `EDIT-MODE-14`, `EDIT-MODE-17`,
`EDIT-MODE-18` and `EDIT-TEXT-14` in `EDITOR-SPEC.md`.

The editor renders Markdown itself: a read-only Markdown tab (`docs/MODES.md`, Text tabs) and,
when the preference is on, a Markdown comment body on the canvas. md4c reads the text, a TeX
formula is drawn by JKQTMathText, an SVG image by QtSvg after its markers are expanded; no
browser engine, no TeX installation and no network are involved.

```
 TextDocument::showPage()                    CyberiadaSMEditorCommentItem
   read-only Markdown tab                      body not edited + renderMarkdownComments
        | text, dir of the file                      | body, dir of the .graphml
        v                                            v
 MarkdownView : QTextBrowser               EditableTextItem::document()
        \                                           /
         +--------> importMarkdown(doc, text, baseDir) <------+       markdown_importer.*
                                  |
            readMarkdownMd4c: md4c callbacks -> QTextCursor       markdown_md4c_reader.cpp
            the formulas and the images inserted as objects
                                  |
                                  v
            QTextDocument: text, blocks, tables + objects
              MarkdownMathObject {tex, display}   MarkdownImageObject {path, source, alt}
                      |                                   |
     installMarkdownObjects(doc): the layout handlers          markdown_objects.*
                      v                                   v
        MathObjectHandler                      ImageObjectHandler
          MathRenderer -> JKQTMathText            SVG: expandSvgMarkers -> QSvgRenderer
          a QPicture per (tex, font, colour)      other: QImageReader; read once per file
          math_renderer.*                         svg_markers.*
```

## Reading

`readMarkdownMd4c` builds the document from the callbacks of md4c (`MD_DIALECT_GITHUB` with
the math spans) through a `QTextCursor`, on any Qt the editor builds with (`EDIT-MODE-17`). The
blocks take the formats of Qt's own reader (`QTextMarkdownImporter`): the heading sizes, the
margins, the lists and their markers, the quotes, the code font, the tables; the formulas and
the images are inserted as objects.

* A header cell takes the alignment of its column, as a body cell does.
* Raw HTML: the tags are counted and the HTML inserted once they close; inside them every text
  waits in its order (the code as `<code>`); a block inserts what is still open at its end.
  A comment `<!-- … -->` waits for its end over its lines and shows nothing, as a link
  definition to nothing (`[text]: #`, the Markdown comment) does.
  A `<br>` outside other tags is a line separator (Qt 5.12 drops a `<br/>` inserted alone),
  another void element closes itself (`<img ...>` -> `<img .../>`), a tag that is not text
  formatting (`<rundir>`) is shown as text.

The source highlighting (`EDIT-TEXT-11`) finds the formulas and the HTML comments with
`markdownSourceRanges`, the same md4c reading, so a formula is coloured exactly where it is
rendered and a comment where it is hidden. The same reading gives the runs of inline text of
the paragraphs, headings, list items and table cells: the lines a run crosses are matched as
one text, so an element broken by a line feed is coloured whole.

```
 markdownSourceRanges(text): math, comments, inlines (a run of text md4c reads in one block)
 markdownSpans: the lines one by one (fence, heading, rule, quote and list markers)
   > said *this        piece "said *this"    -+ the line feed lies in an inline run:
   > and that* ok      piece "and that* ok"  -+ joined "said *this\nand that* ok"
   the rules over the joined text, each match cut back to its pieces: "*this" and
   "and that*" emphasis, the second "> " a marker; a table row stays alone
 CodeHighlighter: an edit that changes the colours before the edited line (a closing *
   typed on a later line) re-highlights the lines from the first change
```

Qt's own `QTextDocument::setMarkdown` (Qt 5.15) is not used: it reads no formulas, drops an
image without an alt text, makes one image per piece of a formatted alt text and loses the
text after a `<br>` or an unknown tag.

## Layout

The spaces around the blocks are measured in text lines (the height of the default font) and
collapse as in CSS: of a bottom and the next top space the larger one counts.

| Block | Above | Below |
|---|---|---|
| heading | 1.5 (none for the first block) | 0.5 |
| paragraph, loose list item | 0.75 | 0.75 |
| tight list item | 0.15 | 0.15 |

A quote sets `MarkdownQuoteLevel` on its blocks (Qt's `BlockQuoteLevel` exists from Qt 5.14
only) and indents them by 1.25 lines a level. Its text and its formulas take
`markdownQuoteColor`: 55 % of the text colour, 45 % of the background (`setMarkdownColors`); a
link keeps its colour. `drawMarkdownQuotes` draws a bar of 0.2 lines (2 px at least) a quarter
into the indent of each level, on through the following blocks of the same quote:

```
 |  a quote with x^2           <- level 1: grey text, a bar
 |  on two lines
 |
 |  |  and a deeper one        <- level 2: the bar of level 1 goes on
```

`MarkdownView::paintEvent` draws the bars over its document, `EditableTextItem::paint` over a
rendered comment body (its colours: the text colour on white, the canvas keeps its colours).

## Formulas

* A span `$..$` is inline, `$$..$$` is display; a display formula alone in its paragraph
  centres the paragraph.
* `MathRenderer` parses the TeX with JKQTMathText (XITS fonts, embedded), at the size of the
  text around it, in its colour (black in a tab). `\colon` reads as `:`.
* A formula is recorded once into a `QPicture` with its width, ascent and descent and replayed
  on every paint, on the screen and in an export alike.
* The baseline. An `AlignMiddle` object of height `h` gets `(h + x/2) / 2` above the baseline
  and `(h - x/2) / 2` below it, `x` the x-height of the font, so the handler makes
  `h = max(2a - x/2, 2d + x/2)` for the formula's ascent `a` and descent `d`: the line holds the
  formula. Qt draws the object at that ascent above the baseline up to 5.15.0 (baseline
  `top + (h + x/2) / 2`) and in the middle of its line in the later 5.15 releases (baseline
  `top - (line height - h) / 2 + line ascent`, from the line of the object); a probe document
  tells once which this Qt does.
* A formula JKQTMathText reports errors for is drawn as its source in red monospace, the errors
  in its tooltip. Built with `CYBERIADA_WITH_MATH=OFF`, every formula is drawn so.

## Images

* Every image is an object. Its path is relative to the base directory — the directory of the
  tab's file or of the diagram; an untitled document has none, so only an absolute path is
  found; a URL that is not a file is not read.
* An image is drawn at its own size, scaled down to the text width; a missing or unreadable
  one as its alt text and path in a dashed frame.
* A file is read once and again when it changes. Qt 5 draws SVG Tiny 1.2, which has no
  markers, so `expandSvgMarkers` adds them before `QSvgRenderer` reads an SVG:

```
 <path d=".." style="..;stroke-width:w;marker-end:url(#m)" transform="T"/>
 -> the path as it is, then
 <g transform="T translate(end) rotate(angle) scale(w) translate(-refX,-refY)"> children of #m </g>
```

  * `marker-start` and `marker-end` of `path`, `line`, `polyline`, `polygon` (the elements
    SVG marks), set by an attribute, by `style` or inherited from a group;
  * `svgPathEnds` reads the path data (`M L H V C S Q T A Z`, absolute and relative, the
    packed numbers and arc flags): the end points and the directions of the nearest segments
    of non-zero length, an arc's from its centre form; `orient="auto"`, a number of degrees, or
    `auto-start-reverse`;
  * `markerUnits="strokeWidth"` (the default) scales by the stroke width,
    `userSpaceOnUse` does not; a `viewBox` with `markerWidth`/`markerHeight` scales to fit;
    the copies lose the ids of the marker;
  * `marker-mid` is not drawn, a file in another encoding than UTF-8 stays as it is.
* The fonts an SVG names must be installed; another font is taken otherwise, as a browser does.

## The view

`MarkdownView` is the page of a read-only Markdown tab: a `QTextBrowser` whose document is
filled by `importMarkdown`. A link to a file or a site opens outside the editor, a `#anchor`
scrolls to the heading of that GitHub anchor (`headingAnchor`: lower case, the spaces as `-`,
no punctuation, a repeated one numbered); Ctrl+wheel zooms. The page is black on white as
the PDF, whatever the colour scheme of the editor and the theme (`EDIT-COLOR-7`); a palette
change renders the text again for the link colour.

## PDF

File > Export in a Markdown tab (and `--export <file>.pdf` in batch mode) renders the current
text once more, into a document of its own laid out on pages (`EDIT-MODE-18`):

```
 slotFileExport / batch --export x.pdf
                     |
                     v
 TextDocument::exportPdf(path): the text, the dir and the name of the file
                     |
                     v
 exportMarkdownPdf(text, baseDir, title, path)                     markdown_pdf.*
   QPdfWriter: A4 portrait, margins 20 mm, the resolution of the screen
   QTextDocument: laid out for the writer, no margin of its own,
     setPageSize(the paint rect less a band for the number)
     setMarkdownColors(black, white) + importMarkdown(text, baseDir)
                     |
                     v
   page i of pageCount():  translate(0, -i * h), clip to page i
     layout()->draw(palette text black) + drawMarkdownQuotes
     i + 1 centred in the band; newPage()
```

* `QTextDocument::print()` would draw a clone without the object handlers and the quote bars:
  the pages are drawn here, from the same `importMarkdown` as the view.
* The writer takes the resolution of the screen, the one the formula pictures are recorded at,
  so the text, the formulas and the images keep their sizes of the rendered page; the PDF keeps
  the text as text and the SVG images and the formulas as vectors.
* The layout of a paged document moves a line that would cross the page end to the next page.
* The text and the formulas are black on white whatever the theme; a link keeps the link colour.

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
* A fragment link (`dChunk`) to a rendered body has no source offsets to underline: it ends on
  the border of the comment, as while the body is edited (`EDIT-EDGE-21`).
* The setting off draws the canvas exactly as before; the good files of the render tests stay.

## Build

| Part | Where | Licence |
|---|---|---|
| md4c 0.5.2 (`md4c.c`, `md4c.h`) | `3rdparty/md4c` | MIT |
| JKQTMathText of JKQtPlotter 5.0.0-beta2, the parts it needs of JKQTCommon | `3rdparty/jkqtmathtext` | LGPL-2.1+ |
| XITS fonts | `3rdparty/jkqtmathtext/jkqtmathtext/resources/xits` | OFL-1.1 |

* The option `CYBERIADA_WITH_MATH` (`ON`): `OFF` builds without JKQTMathText.
* The vendored code is compiled as it is, as C++11 like the editor, into the static libraries
  `md4c` and `CyberiadaMathText`; `3rdparty/README.md` lists the files and the versions.

## Testing

`l4-markdown` (`docs/TESTING.md`, L4); no glyph is compared, as everywhere in the tests:

* the formulas never in code, an escaped or a lone dollar, the tables; every image one
  object, without an alt text or with a formatted one; the HTML tags - the void and the
  foreign ones, the order inside the tags, an unclosed one ending with its paragraph;
* `svgPathEnds` and `expandSvgMarkers` on small examples, and the arrowhead of
  `texts/images/arrow.svg` drawn by QtSvg only after the expansion;
* the image paths and the base directory; the baseline of an inline formula lower and higher
  than its text; the heading anchors; the spaces of the blocks; the quotes - their levels,
  their grey text and formulas, their bars (shapes, no glyph);
* the structures of `texts/markdown.md` and `texts/notes.md` against
  `good/markdown-structure-output.txt` and `good/notes-structure-output.txt`;
* the read-only tab of `texts/markdown.md`: the formulas, the images found next to the file;
  the sizes of its objects - an SVG at its own size or the text width, a missing image as
  its label, a bad formula with its errors, the display formula centred;
* the comments of `diagrams/markdown-render-comments.graphml` with the preference on and off;
* the PDF of `texts/markdown.md`: a PDF file of A4 pages, a long text on more pages than a
  short one, its objects drawn on the pages.

`l2-render-markdown` runs the batch verb: the dump and the saved document stay as they are.
`mode-markdown-pdf` exports `texts/markdown.md` to PDF in batch mode; `mode-markdown-export-reject`
(a PNG of a Markdown text) and `mode-text-export-reject` (a PDF of a C++ text) are usage errors.
