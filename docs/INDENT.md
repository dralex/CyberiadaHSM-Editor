# Indentation — Architecture

**Document version:** 0.2 (2026-11-26), the Tab cycle of every language and the code blocks of
Markdown; 0.1: the indentation. The requirements are `EDIT-INDENT-1..10` in
`EDITOR-SPEC.md` §4.21. Read by the developers of the editor and by the authors of a language
file (`docs/PLUGIN-API.md`, Languages).

The source editors indent as Emacs does: Return puts the new line at its place (electric
indent), Tab moves a line there (`tab-always-indent`), the indent is written in spaces or tabs
of one width (`indent-tabs-mode`). Only spaces and tabs are ever inserted or removed: no
closing bracket, no comment star, no list marker, no quote mark.

```
 a key: Return, Tab, Shift+Tab, a typed character
     |
     v
 CodeEditor::keyPressEvent                    a text tab: the rules of its mode
 IndentFilter (an event filter)               the code dialogs: the rules of the document language
     |  Indenter::key(event): Return, Tab, Backtab, an electric character typed
     |                        -> one edit block, true when done
     |  rulesFor(text, line)  a Markdown code block: textIndentRulesAt; a dialog: its header
     v
 text_indent.*  (no widget)                    TextIndentRules <- CodeLanguage (syntax/*.json)
     |  textIndentColumn(text, line)    the column of a code line
     |  textIndentCycle(text, line)     the levels Tab goes through
     |  textIndentLevels(text, line)    the levels a Python line keeps in Indent Region
     |  textIndentPoints(text, line)    the indent-relative points of the line above
     |  textIndentParts(text)           the code blocks of a Markdown text <- markdownFencedBlocks
     |  textReturnIndent(text, at)      the indent a new line takes
     |  textReindent / textShift / textUntabify -> TextFillEdit (a minimal edit)
     v                                       ^
 the edit                                     | width, form, switch; indentChanged
                                         SettingsManager  <---  Preferences > Text, Edit menu
 TextDocument::indentLines, untabifyLines     Indent Region (textReindentParts in Markdown),
                                              Convert Tabs to Spaces, batch
```

## The keys

| Key | A language with rules | Markdown, Plain text, a language without rules |
|---|---|---|
| Return | the line breaks, the spaces and tabs around the cursor go, the new line at its column | the new line takes the whitespace of the fill's continuation prefix; a line left blank is emptied |
| Tab | the line to its column, wherever the cursor is; at its column or repeated, the next level of its cycle | in the leading whitespace: the line to the next indent point of the line above; after the text: whitespace at the cursor to the next point |
| Tab, a selection | each line re-indented | each line one width right |
| Shift+Tab | the line, or each selected line, one width left | the line back to the previous point; a selection one width left |
| a typed `}` `]` `)` `{` `#` first, a label's `:`, a dedenter's `:` | the line re-indented, in the step of the character | - |

With *Indent automatically* off, Return is a plain break, nothing is electric, Tab inserts the
whitespace to the next stop of the width at the cursor (a selection moves right), Shift+Tab,
Indent Region and Convert Tabs to Spaces stay.

## The rules

A scanner reads the text from its start, line by line, the comments and the strings masked by
the quotes, the comment marks and the escapes of the language as the highlighter reads them
(`CodeStyle::spans`), with the strings over the lines too: the triple quotes of Python, the raw
strings of C++ (`R"x(...)x"`) and the verbatim ones of C# (`@"..."`). The column of a line comes from the
state at its start; then its characters move the state, with the line as it now stands, so
Indent Region is one pass. A line starting in a block comment, a string or a preprocessor line
continued by `\` has no column: it stays (after Return there, the new line takes the indent of
the line above). The indents of the lines above are read as they are, not as they would be.

**The brackets** - all the code styles. Each open `(`, `[` and `{` is a frame: the column of the
opener, the indent of its line, the column of the code after it on its line (none when the line
ends). In a frame with code after its opener a line aligns with that code, its closing bracket
first with the opener; in a frame whose opener ends its line, a line is at the indent of the
opener's line plus the width, its closing bracket first at that indent. A line continuing an
argument lines up with the start of that argument (Emacs `c-lineup-argcont`), a keyword argument
`name=value` with its value; Python keeps to the opening delimiter (PEP 8), and a call cascaded
from the line above, `.arg()` or `->next()`, goes under the first argument. The commas of
template arguments (`QMap<A, B>`) part no arguments; the `;` of a `for` header does.

```
foo(a,            items = [           f(at, A |             add(x, help='one '
    b);               1, 2,                 B);                         'two')
                  ]
```

**`braces`** (C, C++, C#). A `{` whose opener ends its line is a block of statements; the
statement state is kept per block: in a statement or not, the indent of its first line, a
header waiting for its `(...)`, a substatement due.

| Line | Column |
|---|---|
| a statement | the base of the block plus the width; the base is the indent of the first line of the statement holding the `{`; `indentFlat` blocks (`namespace`) add no width |
| `}` first | the base |
| a label: a word of `indentLabels` first, its `:` (not `::`) on the line | the base: a width less than the statements |
| after `if (...)`, `for (...)`, `while (...)`, `switch (...)`, `catch (...)`, `else`, `do` | a substatement: the indent of the header's line plus the width, a `{` first at that indent |
| a statement continued: the line above it ended none of `;`, `{`, `}`, a label's `:` | the first that applies: a `:` under the last `?` (`c-lineup-ternary-bodies`), a `<<` or `>>` under the first one (`c-lineup-streamop`), a `.arg()` cascade the width in; under the code after the first `=` on its line (`c-lineup-assignments`) or after `return`; a declaration of the top level without `=` and `(` (a return type above its function, `template <...>`), and the one under a `template <...>` header in a class too, at its first line; else its first line plus the width. A `{` first: its first line |
| a macro alone on its line without `;` (`Q_OBJECT`, `Q_DECLARE_METATYPE(A)`) | a statement of its own |
| a comment alone on its line, under the comment after the code of the line above | stays there (`c-lineup-comment`) |
| a preprocessor line (`preprocessor: true`) | 0, the state not moved by it |

A `{` after `=`, `return`, `enum`, or inside a bracket after anything but `)` and `=>`, opens a
list (`int a[] = {`, an enum, an initializer): there `,` ends a statement too. Elsewhere a line
after `,` continues the statement (`int a = 1,`, a constructor's initializers).

```
class Window {            switch (state) {          if (a)
public:                   case STATE_INIT:              if (b)
    Window();                 set_state(STATE_DAY);         c();
};                            break;                d();
                          }
```

**`brackets`** (JSON): the brackets only; a line outside them is at 0.

**`offside`** (Python). Inside a bracket the brackets rule; otherwise the line above is the first
line of the statement above it, comment and blank lines skipped:

| Line | Column |
|---|---|
| after a line ending `:` | its indent plus the width |
| after `return`, `pass`, `break`, `continue`, `raise` | the indent of the nearest line above with a smaller indent (the block holding it) |
| `else`, `elif`, `except`, `finally` first | the nearest `if`, `elif`, `for`, `while`, `try` or `except` around it: the nearest line at each lower level above, when it is one of them |
| after a line ending `\` | the indent of the statement plus the width; two widths for the header of a block (`def`, `class`, `if`, `elif`, `while`, `for`, `with`, `except`), apart from its body (PEP 8) |
| in a bracket ending the header line of a block | two widths in (PEP 8) |
| else | the indent of the line above |

The levels (`textIndentLevels`): the column, then each smaller indent of the statements above,
nearest first, down to 0; the body of a block, after a `:`, has its column alone; a dedenter the
indents of the openers around it. As the indent of Python tells its blocks, Indent Region keeps
a line at one of its levels and moves another to the nearest level, the deeper of two (Emacs
`python-indent-region` keeps the block starts and the dedenters too).

**Relative** (a language without `indent`), **Markdown** and **Plain text** have no column. The
new line of Return takes the whitespace before the first other character of the fill's
continuation prefix (`textFillPrefix`, `EDIT-MD-18/23`): the paragraph's indent, the text column
of a list item, the whitespace before the marks of a quote; outside a unit, the indent of its
line. The indent points of Tab (Emacs `indent-relative`) are the indent and the start of each
word of the nearest non-blank line above; past the last one, the stops of the width.

## The Tab cycle

Every style that computes a column cycles (`textIndentCycle`, `Scanner::cycle`): the column,
then the indents of the code lines above smaller than it, nearest first, each smaller than the
one before, the left edge last; a Python dedenter the indents of the openers it may follow. The
code lines are those the scanner reads with code: not kept, not a preprocessor line. Emacs
python-mode takes the multiples of the width below the column; the indents written above keep a
file of another width and the aligned columns.

| The line | Tab |
|---|---|
| off its column | to its column |
| at its column, or right after a Tab on it (the line, the revision and the cursor as the Tab left them) | to the next level after its indent, from the last back to the column |
| a cycle of one level | stays, no edit |

```
void f() {               Tab on x(): 8 -> 4 -> 0 -> 8
    if (a) {
        x();
```

The first Tab on a misplaced line places it; on a placed line it moves it (the request of the
user: each press moves). Indent Region places without moving the placed lines.

## The code blocks of Markdown

```
 Markdown text ── markdownFencedBlocks(lines) ── {open, close, indent, tag}   (markdown_lines)
                        |  the fence as the highlighter reads it: markdownFence(line)
                        v
                 textIndentParts(text) ── CodeStyle::find(tag) computes? ── TextIndentPart
                        |                                     {first, last, rules}
     rules.codeStart = the start of the first code line   (the lines above: a header, masked)
     rules.margin    = the indent of the opening fence     (the root of the scan, not 0)
                        v
 Indenter: textIndentRulesAt(text, line)      TextDocument: textReindentParts(parts)
```

The rules of Markdown (`textIndentRules(textFillMarkdown)`) carry `blocks`. A key takes the
rules of the line it works on: of the part holding it, else of Markdown, so the fences stay
Markdown and Return on an opening fence gives its indent. A selection takes a part's rules when
its first and last lines are in the same one. The scan of a part is the scan of a code dialog:
the text before its code is a header the lexer masks, so the prose, its brackets and quotes
included, is not read; the margin puts the root frame, the top-level lines, the preprocessor
lines and the left edge of the cycle at the fence's indent. `textReindentParts` re-indents each
part within the range on the text as the parts before left it and gives one minimal edit of the
whole (`linesEdit`). The fence is the highlighter's (`^ {0,3}(`{3,}|~{3,})\s*([^\s`]*)\s*$`, a
closing one of the same character, as long at least, without a tag): the code is indented by the
language it is coloured by.

## The form

`textIndentString(column, tabs)`: *Spaces* - `column` spaces; *Tabs* - a tab per stop of
`EDITOR_TAB_SPACES` (4) from 0 and spaces for the rest. A line moved gets its leading
whitespace rewritten whole; one already at its column in the form is no edit. Whitespace
inserted after the text (Tab without rules) is the same: tabs to the stops inside the run,
spaces for the rest.

## The language file

| Key | Values | Meaning |
|---|---|---|
| `indent` | `braces`, `brackets`, `offside` | the style; without it a line takes the indent of the line above |
| `indentLabels` | words | `braces`: a line starting with one and ending its label with `:` goes a width left |
| `indentFlat` | words | `braces`: the block of a statement starting with one is not indented |

The built-in files: C `braces`, labels `case`, `default`, flat `extern`; C++ `braces`, labels
`case`, `default`, `public`, `protected`, `private`, `signals`, `slots`, `Q_SIGNALS`,
`Q_SLOTS`, flat `namespace`, `extern`; C# `braces`, labels `case`, `default`; JSON `brackets`;
Python `offside`.

## Undo

Each key is one edit block from the editor's cursor, so Undo puts the cursor back: Return with
its indent is a typing step; Tab, Shift+Tab, Indent Region and Convert Tabs to Spaces name
their steps (`indent`, `unindent`, `indent region`, `convert tabs`) by `editStepNamed`; an
electric character is typed by the indenter itself, with the re-indent of its line in the same
block (a block joined to the typing of `QPlainTextEdit` stays a step of its own). An edit that
changes nothing makes no block. Fill as You Type (`EDIT-MD-21`) runs after Return and finds
the line left by the position of its line feed.

## The dialogs

`IndentFilter` watches the `QPlainTextEdit` of `StateActionDialog` and of `MultilineTextDialog`
for a code role, with the rules of the document language and `codeStart`: the start of the code
after an `entry/` or `EVENT [guard] /` header (`codeBehaviourStart`, the highlighter's
segments). The text before it is no code; its lines stay.

## The storage

| Key | Value |
|---|---|
| `editor/indentWidth` | 1..8, 4 by default |
| `editor/indentWith` | `spaces` (default) or `tabs` |
| `editor/autoIndent` | `true` (default) or `false` |

A batch run takes 4 and spaces (`overrideIndentWidth`, `overrideIndentTabs`) unless its
`indent-width` and `indent-with` verbs set them.

## Testing

`l4-indent`: the columns of tables of texts per style; the forms and the minimal edits; the keys
in a C++, a Python, a JSON, a Markdown and a Plain text tab (Return, Tab anywhere, the Python
cycle of each style, Shift+Tab, a selection, the electric characters, the off switch, read-only,
one Undo each); the code blocks of a Markdown tab (the parts, the keys in a block and in a list
item, the prose untouched, Indent Region); the dialogs; the Preferences row, the Edit menu, the
storage, Reset. The batch cases `mode-indent-*` re-indent the fixtures of `tests/texts/indent-*`
(the course code of chapter 06, tabs and spaces mixed; a Markdown text with blocks of several
languages) and convert their tabs. The style was checked by Indent Region on copies
of written code (Nov 26 2026): the space-indented sources of the editor, 168 files and 38421
lines, 120 lines moved; 40 Python modules of the dispatchers and the agents, 9864 lines, 16; the
course programs and the language files, the tabs and the blank lines only; the Markdown chapters
of the course and `PLUGIN-API.md`, the lines of their `c`, `cpp` and `json` blocks only, their
whitespace (the JSON examples from 2 columns to the width). What moves is code
aligned by hand otherwise (the authors' habits differ: a continued assignment under its value
or a width in, a lambda argument a width in) or a misalignment.

## Stages

1. The indentation (`EDIT-INDENT-1..9`) — done.
2. The Tab cycle of every language, the code blocks of Markdown (`EDIT-INDENT-1/10`) — done.
