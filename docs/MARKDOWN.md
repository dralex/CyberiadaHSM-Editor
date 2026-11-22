# The Markdown Rendering — Architecture

**Document version:** 0.16 (2026-11-22) — the authoring tools specified and implemented
(Tools, stages 1 to 6); 0.14: the outline specified (Outline); 0.15: the outline implemented
(Outline, stage 1); 0.16: the fill of the paragraphs (Fill).
The requirements are `EDIT-MODE-14`, `EDIT-MODE-17`, `EDIT-MODE-18`, `EDIT-TEXT-14`,
`EDIT-MD-1..23` and `EDIT-IO-8` in `EDITOR-SPEC.md`.

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
one text, so an element broken by a line feed is coloured whole. The link rule also gives the
target of a link (`markdownSpans`, `targets`): the destination of `[text](dest)` and
`![alt](dest)` without its `<>`, the address of `<url>`, a bare URL; a text tab underlines it
and opens it by Ctrl+click (`EDIT-MODE-28`, `docs/MODES.md`, Text tabs).

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

## Tools

A Markdown tab writes a course text: its images and diagrams live in the folders next to it,
its links point to its headings and to the other texts (`EDIT-MD-1..12`).

```
 Text toolbar, Edit > Insert ..., a drop, a paste       Document > Update Diagram Images, Check
        |                  |                  |                         |
        v                  v                  v                         |
 InsertImageDialog  InsertDiagramDialog  InsertLinkDialog               |     dialogs/
        \                  |                  /                         |
         +-----> markdown_tools.*: the paths and the copies, the written line,
                 markdownImages, markdownDiagramImages, updateDiagramImages,
                 renderDiagramFile, markdownHeadings, markdownCheck
                       |                                  |
       TextDocument::insertBlock: one undo step    renderScene(..., source) + svg_source.*
       (the cursor, or line n in batch mode)       an off-tab model and scene, as a diff
                                                   side has (cyberiadasm_render.*)
```

A course text and its files:

```
 02-traffic-light-sm.md
   ![Wiring](images/semaphore-arduino.png)                         Insert Image
   [![Traffic light](diagrams/2/semaphore.svg)](diagrams/2/semaphore.graphml)   Insert Diagram
   see [the HSM language](04-hsm-language.md#42-composite-states)  Insert Link
 images/semaphore-arduino.png            copied, perhaps cropped
 diagrams/2/semaphore.graphml            copied when it lay outside
 diagrams/2/semaphore.svg                <metadata>: dc:source semaphore.graphml, the digest
```

### Paths and copies

| Source | Written |
|---|---|
| a file inside the text's directory, not edited | linked where it is |
| a file outside, or an edited image | saved into the folder: `images/`, `diagrams/` by default, the last one remembered |
| a pasted image | `image-<n>.png`, the first free n |
| a target with the same content | reused, nothing asked |
| a target with another content | Replace, Keep both (`<name>-<n>.<ext>`), Cancel; batch keeps both |

* A path is relative to the directory of the text, with `/`; `<path>` when it has a space or a
  parenthesis; `\\`, `\[`, `\]` in an alt or link text. The inserted line is a paragraph of its own.
* An untitled text has no directory: Save and Insert first; the source image is never written.

### Insert Image

```
 Insert Image ─────────────────────────────────────────────────────────
 Source: /home/u/Pictures/scr-2026.png                        [Browse]
 ┌──────────────────────────────────────────────┐  Crop  x 12  y 40
 │   ┌────────────────────────┐                 │        w 800 h 450
 │   │  crop frame, 8 handles │                 │  [Rotate ⟲] [Rotate ⟳]
 │   └────────────────────────┘                 │  Width ≤ [ 800 ] px
 └──────────────────────────────────────────────┘  [Reset]
 Alt text: [Traffic light wiring                          ]
 Save as:  [images/] [scr-2026.png        ]   (relative to the text)
 Inserts:  ![Traffic light wiring](images/scr-2026.png)
                                                    [Insert] [Cancel]
```

* The edits are for a raster image (`QImageReader`): crop, rotate (`QTransform`), scale down
  (`Qt::SmoothTransformation`); written by `QImageWriter` in the source format, a JPEG at
  quality 90. An SVG is copied as it is.

### Insert Diagram and the diagram images

```
 Insert Diagram ───────────────────────────────────────────────────────
 Diagram:  ( ) open tab [ semaphore.graphml        v]   (tabs with a file)
           (•) file     [~/work/semaphore.graphml ] [Browse]
 Copy to:  [diagrams/2/]                     (when it lies outside the text's directory)
 Font:     [Cyberiada Mono  v]                (the export font)
 Alt text: [Traffic light state machine                   ]
 Writes:   diagrams/2/semaphore.graphml, diagrams/2/semaphore.svg
 Inserts:  [![Traffic light state machine](diagrams/2/semaphore.svg)](diagrams/2/semaphore.graphml)
                                                    [Insert] [Cancel]
```

The text links the image to its diagram; the SVG records where it was drawn from:

```
 [![alt](diagrams/2/x.svg)](diagrams/2/x.graphml)        the text
        |                          |
        | the image                | Ctrl+click (source), a click (rendered page): its tab
        v                          v
   x.svg  <metadata>  ── dc:source, cyb:sourceDigest ──>  x.graphml
```

* `markdownImages(text)` finds the images line by line, the code fences and the code spans
  skipped, an image that is the whole text of a link with that link's target;
  `markdownDiagramImages(text, baseDir)` — the diagram of an image (`EDIT-MD-6`), in this order:
  the `.graphml` the image is linked to; the `dc:source` of the SVG's metadata; the `.graphml`
  beside the SVG with its base name (every diagram image of the course texts today).
* The provenance belongs to the image, not to the texts using it, so it is stored where
  SVG 1.1 §21 puts metadata: an RDF description with Dublin Core `dc:source` ("a related
  resource from which the described resource is derived") and two terms of the editor's own
  namespace `https://cyberiada.dev/ns/svg#` (`EDIT-IO-8`):

```xml
<svg ... xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
         xmlns:dc="http://purl.org/dc/elements/1.1/"
         xmlns:cyb="https://cyberiada.dev/ns/svg#">
<title>Traffic light</title>
<desc>Generated by CyberiadaPro from semaphore.graphml</desc>
<metadata><rdf:RDF><rdf:Description rdf:about="">
  <dc:source>semaphore.graphml</dc:source>              relative to the SVG
  <dc:format>image/svg+xml</dc:format>
  <cyb:sourceDigest>sha256:9f2c…</cyb:sourceDigest>      the diagram file drawn
  <cyb:font>Courier New</cyb:font>                       the family the text is drawn in
</rdf:Description></rdf:RDF></metadata>
```

* Out of date = the digest is not the SHA-256 of the diagram file, or there is none: a git
  checkout gives every file the time of the checkout, so the times tell nothing.
* `QSvgGenerator` writes its own `<title>`, `<desc>` and no `<metadata>`: with the metadata on,
  `renderScene` generates into a `QBuffer`, sets the title and the description, inserts the
  namespaces into `<svg>` and the block after `<desc>`, then writes the file. QtSvg,
  `expandSvgMarkers` and the browsers draw it as before. Off, the bytes are today's.
* Update Diagram Images keeps the look of the SVG it replaces: the `cyb:font`, else the first
  `font-family` of the old SVG (the course SVGs are Courier New), else the export font. The
  diagram is read from its saved file into an off-tab model and scene, drawn, and dropped.

### Check

```
 Diagnostics ─ 02-traffic-light-sm.md ─────────────────────────────────
 ▲ 92:1   image not found: images/lego-cooker.png
 ▲ 141:1  diagram image out of date: diagrams/2/microwave.graphml changed
 ▲ 208:1  diagram image without a recorded source: Update Diagram Images records
          diagrams/2/semaphore-hierarchy.graphml
 ▲ 210:15 no heading for #states-and-events
 ▲ 233:4  no heading for 04-hsm-language.md#composite
 ▲ 240:1  linked to diagrams/2/day.graphml, its SVG names diagrams/2/night.graphml
```

```
 markdownLinks(text)      [t](target)  ![a](target)  [label]: target   (code skipped)
 markdownHeadings(text)   # ATX and setext, anchors -1, -2 as the rendered view
        \                     /
         markdownCheck(text, baseDir, openText) ── markdownDiagramImages
                 |                                 (no source, out of date, mismatch)
                 v
 window checkMarkdown: PluginDiagnostic warning, line + column ──> Diagnostics
                                         a click: goToLine(line, column) in the source
```

* A target with a scheme (`https:`, `mailto:`) is never fetched: the check needs no network;
  a bare `#` is the Markdown comment. An untitled text checks its `#anchor`s only.
* `file.md#anchor` takes the headings of the file's open tab, its unsaved ones too, else of
  the file. `TextDocument::goToHeading` reads the same `markdownHeadings`, so Ctrl+click and
  the check agree on every anchor.
* An image without a recorded digest (every course SVG before its first Update) is told apart
  from one whose diagram changed.

### Insert Link

```
 Insert Link ──────────────────────────────────────────────────────────
 Text:   [the HSM language             ]   (the selection, if any)
 Target: (•) heading  Text:    [04-hsm-language.md            v] [Browse]
                               this text | the .md files of the directory
                      Heading: [  4.2 Composite states         v]  (indented by level)
         ( ) file     [programs/semaphore/semaphore.ino      ] [Browse]
         ( ) address  [https://                              ]
 Inserts: [the HSM language](04-hsm-language.md#42-composite-states)
                                                    [Insert] [Cancel]
```

```
 InsertLinkDialog(textPath, its text, the selection, openText)
   Text combo: this text + markdownFilesUnder(dir), Browse adds one outside (../x.md)
   Heading combo: markdownHeadings of the chosen text (its open tab's text, else the file)
   file: relative to the text, a missing one refused;  address: as typed
        | markdownLinkLine(text, target)
        v
 TextDocument::insertInline: the selection replaced, one undo step
```

* `markdownHeadings(text)` → {level, title, anchor, line}: one reading of the headings (not
  in a code fence) for the dialog, the check and `TextDocument::goToHeading`, numbering a
  repeated anchor as `MarkdownView::scrollToHeading` does (`-1`, `-2`); a Cyrillic heading
  keeps its letters, as GitHub does: `## 4.2 Составные состояния` is `#42-составные-состояния`.
* The link text is the selection, else the heading title, the file name or the address, until
  it is typed; a keystroke is never overwritten by a proposal (`textChanged` comes before
  `textEdited`).
* Opening `file.md#anchor`: `openFileRequested(path, fragment)` (Ctrl+click in the source, a
  click on a rendered page); `openLinkedFile` opens or activates the tab, then its
  `showHeading`: the heading's line in the source, or the rendered page scrolled to it; an
  anchor no heading has only opens the tab.
* Batch `insert-link <target> [line <n>] [<text>]`: a paragraph of its own; the target written
  as given, relative to the text; a missing file or an unknown anchor refused.

### The preview

```
 ┌ 02-traffic-light-sm.md [Markdown] ───────────────────────────────────┐
 │ CodeEditor (source)              ┃ MarkdownView (rendered)          │
 │ ## 3.2 States and events         ┃  3.2 States and events            │
 │ ![Traffic](diagrams/2/s.svg)     ┃  [diagram]                        │
 └──────────────────────────────────┸───────────────────────────────────┘
   contentsChange ─ 300 ms single-shot timer ─> importMarkdown(text, baseDir), the scroll kept
   cursor to another block ─> the rendered block of that source line scrolled into view

 TextDocument page (QStackedWidget)
   preview off:  [CodeEditor] [MarkdownView: read-only page]
   preview on:   [QSplitter: CodeEditor | MarkdownView preview] [read-only page]
                  the editor moves into the splitter and back (showPage)
```

* View > Preview splits each writable Markdown tab (`display/markdownPreview`, enabled in a
  Markdown tab); the read-only flag still shows the rendered page alone. With the preview off
  the page holds the editor as before, so nothing else sees a change.
* The reader gives every block it inserts the source line of its first text
  (`MarkdownSourceLine`): md4c hands the texts as pointers into the bytes it reads, their offsets
  give the lines; a block not inserted yet (an image alone in its paragraph) takes the line
  when it is; a list item drops the line of the item whose format it copies.
  `MarkdownView::scrollToSourceLine` takes the last block beginning at or before a line and
  brings it a line below the top when it is out of view; one in view stays.
* `setMarkdownText` puts the view's cursor back at the start: the import inserts at the cursor
  and would push it, and the scroll with it, to the end of the page.
* The image and formula caches keep a re-render cheap.

### Stages

1. The toolbars — done: `mainToolBar` split into Main and `diagramToolBar`
   (`smeditor_window.ui`), `textToolBar` with Go to Line (`images/go-to-line.svg`) where the
   element tools stand, `showModeBar` hiding a bar for the tab's kind and showing it back unless
   the user closed it (`modeHiddenBars`); Log session greyed in a text tab with the other
   canvas switches. Each later stage adds its action to the Text toolbar.
2. Insert Image — done: `markdown_tools.*` (the paths, `proposeImageTarget`, `imageInsertData`,
   `imageTargetClashes`, `writeImageInsert`), `TextDocument::insertBlock`,
   `dialogs/insert_image_dialog.*` (`ImageCropView`), `CodeEditor::insertFromMimeData` offering
   a dropped or pasted image (queued, after the event), Edit > Insert Image (`Ctrl+Shift+I`, the
   text toolbar), `markdown/imageFolder`, `insert-image`.
3. Insert Diagram, Update Diagram Images and the SVG metadata — done: `svg_source.*`
   (`recordSvgSource`, `readSvgSource`, `fileDigest`), `renderScene(..., sourceFile, title)`, the
   Export checkbox (`export/svgMetadata`), `--svg-metadata`, `markdownImages`,
   `markdownDiagramImages`, `renderDiagramFile` (an off-tab `CyberiadaSMEditorDocument`),
   `updateDiagramImages`, `dialogs/insert_diagram_dialog.*` (`markdown/diagramFolder`),
   `MarkdownView::linkActivated` through `TextDocument::openLink`, `insert-diagram`,
   `update-diagram-images` (`== update`).
4. Check — done: `markdownLinks`, `markdownHeadings` (also `TextDocument::goToHeading`),
   `markdownCheck`, the window's `checkMarkdown` (F7 routed by the tab), the column of a
   `PluginDiagnostic` (`line:column`, a click goes there), `findTextTab`, `check` (`== check`).
5. Insert Link — done: `markdownFilesUnder`, `markdownLinkLine`, `dialogs/insert_link_dialog.*`,
   `TextDocument::insertInline` and `showHeading`, `openFileRequested(path, fragment)` and the
   window's `openLinkedFile`, Edit > Insert Link (`Ctrl+K`, the text toolbar), `insert-link`.
6. The preview — done: `MarkdownSourceLine` from the md4c reader,
   `MarkdownView::scrollToSourceLine`, the splitter page of `TextDocument` (`showsPreview`,
   `renderPreview`, `followCursor`), `display/markdownPreview`, View > Preview, `preview`, the
   `preview: yes` dump line.

## Outline

The Outline dock shows the headings of a Markdown tab as a tree and its images as leaves
(`EDIT-MD-13..17`). The text is the source: the model is read from it, the tree only navigates,
nothing goes back.

```
              data: one way only
 text ──(read ≤ 1/s, only while the Outline is shown)──▶ model ──Qt signals──▶ tree
   ▲                                                                           │
   └──────────── navigation only: goToLine / showHeading ◀─────────────────────┘
```

|                   | Structure tree (a diagram)        | Outline (a Markdown text)                     |
|-------------------|-----------------------------------|-----------------------------------------------|
| data              | the element tree in the model     | the text; the model derives its nodes         |
| node identity     | `Element*`                        | `OutlineNode*`, matched across the readings   |
| a change          | the model's mutators              | a reading of the text                         |
| undo and redo     | a snapshot, the model reset       | a text change, the next reading               |
| editing in a tree | rename, drag, delete              | none: `flags()` enabled and selectable only   |

### The parts

```
 TextDocument (Markdown mode)                         CyberiadaSMEditorWindow
   QTextDocument ─contentsChanged─▶ dirty              outlineDock "Outline" [_][x]
        │                             │ shown + active   MarkdownOutlineView : QTreeView
        │                     1 s single-shot timer      (setModel on a tab switch,
        │                     (not restarted)             nullptr for another tab)
        ▼                             ▼                         ▲   │ current row, Enter,
   text() ──▶ markdownOutline(text, baseDir) ──▶ MarkdownOutlineModel  │ double click
              markdown_outline.*                  refresh: reconcile ───┘   ▼
              markdownHeadings, markdownImages,   indexAtLine, entry   goToLine / showHeading
              markdownDiagramImages (cached)      markdown_outline_model.*  openFileRequested
   CodeEditor ─cursorPositionChanged─▶ (not dirty) indexAtLine ─▶ the view's current row
```

* `markdown_outline.*`: `OutlineEntry` (heading, image or diagram; the level, title, anchor,
  line, column, target, link, diagram state) and `markdownOutline`, pure functions over the
  scanners of Check, so the anchors are the ones of Ctrl+click, Check and Insert Link.
* `markdown_outline_model.*`: `MarkdownOutlineModel : QAbstractItemModel`, read-only, the
  `internalPointer` an `OutlineNode*`; the diagram state is cached by the path, the time and the
  size of the files, so a reading hashes a diagram again only when it changed.
* `markdown_outline_view.*`: `MarkdownOutlineView : QTreeView`, no drag, no edit triggers; not
  `CyberiadaSMView`, which casts its model to `CyberiadaSMModel` to drag.
* `TextDocument` owns the model in the Markdown mode, the dirty flag, the timer and
  `setOutlineShown`; the window binds the view as it binds the structure tree and the changes.

### The reading

A reading builds the new nodes and reconciles them with the old ones parent by parent: the
child lists are compared by `TextDiff::diffLines` (`text_diff.*`) on the keys `#<title>` and
`!<target>`; a changed pair is kept when its titles or targets themselves are similar, so
`# A` and `# B` are not one row by the `#` they share. The level is not in the key: a heading
of another level under the same parent keeps its row.

```
old: [h1 Intro] [img board] [h2 Blink]
new: [h1 Introduction] [img board] [h2 Blink] [h2 Fade]
      changed → kept    equal → kept  equal      inserted
```

| Row of the comparison | The node |
|---|---|
| equal | kept; `dataChanged` when its line moved |
| changed pair, the names similar (≥ 0.5, `MARKDOWN_OUTLINE_SIMILARITY`) | kept, its data replaced; `dataChanged` |
| removed | `beginRemoveRows`, the rows of a parent from the last |
| added | `beginInsertRows`, in the order of the text |

The kept nodes are compared again below; a heading whose new level moves it under another
parent is removed and added, only its subtree loses the expanded state. The model is never
reset: a load is a change of the text, another mode drops the model and Markdown makes a new
one. The expanded state of a row lives in its node, so a tab shown again opens its tree as it
was left.

### A row on an out-of-date outline

```
click / arrow / Enter on the row "2.3" (removed from the text 0.4 s ago)
 1. the view keeps a QPersistentModelIndex of the row, its kind and its title (or its file)
 2. queued after the event: model->refresh(text)
      "2.3" not matched → its rows removed → the persistent index invalid
 3. valid          → the node's current line
    else the only node of that kind and title the reading added → it (a level, a move)
    else           → nowhere; QToolTip at the tree: The heading "2.3. …" is no longer in the
                     text; the row at the cursor current
```

The window has no status bar: the message is a tooltip at the tree, as the source shows the
link targets (`CodeEditor`).

### Stages

1. The navigator — done: `markdown_outline.*` (`markdownOutline`), `DiagramFileCache` and
   `diagramOfImage` in `markdown_tools.*` (`markdownDiagramImages` on them), the model
   (`markdown_outline_model.*`: the reading, `indexAt`, `addedNamesake`, the expanded state),
   the view (`markdown_outline_view.*`), `TextDocument` (`outline`, `setOutlineShown`,
   `readOutline`, `showOutlineRow`, `MARKDOWN_OUTLINE_INTERVAL_MS`), `outlineDock` with View >
   Outline (`display/showOutline`, `modeDockOutline`), the window's `bindOutline`,
   `followOutlineCursor` and `showOutlineRow`, `--dump-outline` (`dumpOutline`).
2. *(optional)* On a rendered page the current row follows the scroll: the `MarkdownSourceLine`
   of the top block.

## Fill

The source of a Markdown or a Plain text tab keeps its lines within a text width, as Emacs'
`fill-paragraph` and `auto-fill-mode` do (`EDIT-MD-18..23`). The width is of the source; the
display wraps apart (`EDIT-MODE-30`).

```
 Edit > Fill Paragraph (Alt+Q), Unfill Paragraph (Alt+Shift+Q),   a Space or a Return typed,
 the text toolbar, batch fill-paragraph / unfill-paragraph         Fill as You Type on
        |                                                                  |
        v                                                                  v
 TextDocument::fill(unfill, from, to)                        CodeEditor::keyPressEvent
   textFill(text, from, to, width, kind, unfill)               QPlainTextEdit types the key,
   -> {from, to, replacement, cursor}: one undo step           Autoreplacer::typed, autoFill():
        |                                                     textAutoFillBreak(text, cursor,
        |                                                       width, kind) -> {at, prefix}:
        |                                                       an edit block, a step of its own
        +------------------------+-----------------------------------+
                                 v
 text_fill.*: the units, the prefixes, the break points, the refill
        | fenceOf, indented, listItem, codeBlockLine, inlineCodeAt
        v
 markdown_lines.*  (shared with autoreplace.*, EDIT-AUTO-4)

 SettingsManager: editor/textWidth (90), editor/fillAsYouType (off), textFillChanged()
   <- Preferences > Text > Text editors (Text width, Fill as you type), Edit > Fill as You Type
```

### The units

A unit is what one Fill makes into one run of lines. `textFill` reads the lines of the text
once: the fences and the indented code (`codeBlockLine`), then each block by its first line.

| Block | Unit |
|---|---|
| a paragraph | its lines, to a blank line, to a line opening another block, through a hard break |
| a list item | its marker line and its continuation lines, lazy ones too; the next item is another unit |
| a quote | the lines of its paragraph, the quote marks taken off and put back |
| a heading, code, a table, an HTML block, a rule, a reference definition, a `$$` formula | none: never filled |
| Plain text | the lines between blank lines |

The prefix of the first line stays as written; the next lines take the quote marks of the
first one, then the marker width of a list item (`- ` two, `10. ` four), else the indent of
the second line, of the first one when the unit has one line.

### The breaks

The words of a unit are its runs of non-spaces, where a code span, a formula, a tag, a link
target or a URL counts as one word with its spaces (`inlineCodeAt`). The refill puts words on a
line while it holds the width (a tab to the next multiple of `EDITOR_TAB_SPACES` columns), a
longer word alone, and moves a word that would open a block at the start of a line (`-`, `*`,
`+`, `1.`, `#`, `>`, a fence, a tag, `=` or `-` alone) back to the line before it. A hard break
ends the unit: its line keeps its two spaces or its `\`.

Fill as you type breaks only the line of the cursor, after the key: the last break point
within the width, else the first one after it before the key typed; the rest goes to a new
line with the prefix. The edit block makes it a step of its own, so Undo right after joins the
line and the key typed stays; Unfill and Fill Paragraph act on the whole unit.

### Stages

1. The fill — done: `markdown_lines.*`, `text_fill.*` (`textFill`, `textAutoFillBreak`),
   `TextDocument::fill`, `CodeEditor::autoFill` with `setFillKind`, the settings
   (`editor/textWidth`, `editor/fillAsYouType`, `TEXT_WIDTH_*`), Edit > Fill Paragraph,
   Unfill Paragraph and Fill as You Type, the text toolbar group, the Preferences row, the
   batch `fill-paragraph`, `unfill-paragraph` and `text-width`.

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

The tools: `l4-markdown-tools` — the paths and the copies, a name clash, the crop, rotation and
scale sizes, the inserted line and its single undo step, the Insert Image dialog, the crop drag,
the pasted images; the image scan, the diagram of an image (the link, the metadata, the
sibling), the digest, the update and the font it keeps, the Insert Diagram dialog, a rendered
link opening its tab; `md-insert-image`, `md-insert-diagram` and their rejects;
`export-svg-metadata` — the parts of the recorded source, the refusals of a modified diagram
and of a PNG, the plain `l3-svg-*` goods unchanged; `l4-dialog test_export_svg_source` — the
checkbox; the link scan, the headings (ATX, setext, repeats, Cyrillic, a link in a heading),
the check with an open tab's unsaved heading, the diagram findings, F7 to the cursor;
`mode-markdown-check` (`texts/markdown-broken.md`) and its reject; the link helpers, the
inline insert, the Insert Link dialog (the texts below the folder, the headings of another
text and of an open tab, a text outside, a keystroke kept), `file.md#anchor` opening at the
heading in the source and on a rendered page; `md-insert-link` and its reject; the source
lines of the rendered blocks, the scroll to a source line, the preview page (the splitter, the
re-render after the edits, read-only and other modes, off), the page following the cursor
with its scroll kept, View > Preview; `mode-markdown-preview` and its reject.

The outline: `l4-markdown-outline` — `markdownOutline` on `texts/*.md` (the levels, a skipped
level, setext, the fences, images on one line, the diagram leaves); the model under
`QAbstractItemModelTester` through the typed edits, undo and redo, a heading retyped, a level
changed and a mode switch; the expanded rows and the selection kept by a reading; nothing read
while the dock is hidden or the tab is in the background; the tree and the source following
each other; every row of the out-of-date table of `EDIT-MD-16`, a removed heading with an
older namesake too; a diagram leaf up to date and out of date; a rendered page scrolled; a
double click opening a diagram; the dock shown by default; `md-outline` (`texts/outline.md`),
`md-outline-edited` (the outline after an Insert Image) and `md-outline-none` (another tab).

The fill: `l4-text-fill` — `textFill` on a table of texts (a paragraph, a list item, a nested
quote and list, the blocks never filled, a hard break, a code span and a URL kept whole, a long
word, no break before a block marker, the cursor kept, Unfill, Plain text indents) and
`textAutoFillBreak`; typing past the width (a Space, a Return, off by default, in code, Undo
joining, after an autoreplace); the actions by the tab kind, their keys and the toolbar
group, greyed in a read-only tab; the Preferences row, Apply and Reset; the stored keys;
`md-fill-paragraph`, `md-unfill-paragraph`, `md-fill-text-width` and `md-fill-plain` on
`texts/markdown-fill.md` and `texts/plain-fill.txt`, `md-fill-reject` (a line the text has not),
`md-fill-text-width-reject` and `mode-text-fill-reject` (a C++ text).
