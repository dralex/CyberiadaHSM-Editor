# Indentation — Architecture

**Document version:** 0.1 (2026-11-26). The requirements are `EDIT-INDENT-1..9` in
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
     |  Indenter::key(event): Return, Tab, Backtab -> one edit block, true when done
     |  Indenter::typed(event): an electric character -> joined to the typing step
     v
 text_indent.*  (no widget)                    TextIndentRules <- CodeLanguage (syntax/*.json)
     |  textIndentColumn(text, line)    the column of a code line
     |  textIndentLevels(text, line)    the Python Tab cycle
     |  textIndentPoints(text, line)    the indent-relative points of the line above
     |  textReturnIndent(text, at)      the indent a new line takes
     |  textReindent / textShift / textUntabify -> TextFillEdit (a minimal edit)
     v                                       ^
 the edit                                     | width, form, switch; indentChanged
                                         SettingsManager  <---  Preferences > Text, Edit menu
 TextDocument::indentLines, untabifyLines     Indent Region, Convert Tabs to Spaces, batch
```

## The keys

| Key | A language with rules | Markdown, Plain text, a language without rules |
|---|---|---|
| Return | the line breaks, the spaces and tabs around the cursor go, the new line at its column | the new line takes the whitespace of the fill's continuation prefix; a line left blank is emptied |
| Tab | the line to its column, wherever the cursor is; Python: repeated, the next lower level | in the leading whitespace: the line to the next indent point of the line above; after the text: whitespace at the cursor to the next point |
| Tab, a selection | each line re-indented | each line one width right |
| Shift+Tab | the line, or each selected line, one width left | the line back to the previous point; a selection one width left |
| a typed `}` `]` `)` `{` `#` first, a label's `:`, a dedenter's `:` | the line re-indented, in the step of the character | - |

With *Indent automatically* off, Return is a plain break, nothing is electric, Tab inserts the
whitespace to the next stop of the width at the cursor (a selection moves right), Shift+Tab,
Indent Region and Convert Tabs to Spaces stay.

## The rules

A scanner reads the text from its start, line by line, the comments and the strings masked by
the quotes, the comment marks and the escapes of the language as the highlighter reads them
(`CodeStyle::spans`), with the triple quotes of Python too. The column of a line comes from the
state at its start; then its characters move the state, with the line as it now stands, so
Indent Region is one pass. A line starting in a block comment, a string or a preprocessor line
continued by `\` has no column: it stays (after Return there, the new line takes the indent of
the line above). The indents of the lines above are read as they are, not as they would be.

**The brackets** - all the code styles. Each open `(`, `[` and `{` is a frame: the column of the
opener, the indent of its line, the column of the code after it on its line (none when the line
ends). In a frame with code after its opener a line aligns with that code, its closing bracket
first with the opener; in a frame whose opener ends its line, a line is at the indent of the
opener's line plus the width, its closing bracket first at that indent.

```
foo(a,            items = [
    b);               1, 2,
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
| a statement continued: the line above it ended none of `;`, `{`, `}`, a label's `:` | its first line plus the width; a `{` first at its first line |
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
| `else`, `elif`, `except`, `finally` first | the nearest `if`, `elif`, `for`, `while`, `try` or `except` above at the computed column or less |
| after a line ending `\` | the indent of the statement plus the width |
| else | the indent of the line above |

The Tab cycle (`textIndentLevels`): the column, then each smaller indent of the code lines
above, nearest first, down to 0. A Tab right after a Tab on the same line, no edit or move
between, goes to the next level after the current one, from 0 back to the column.

**Relative** (a language without `indent`), **Markdown** and **Plain text** have no column. The
new line of Return takes the whitespace before the first other character of the fill's
continuation prefix (`textFillPrefix`, `EDIT-MD-18/23`): the paragraph's indent, the text column
of a list item, the whitespace before the marks of a quote; outside a unit, the indent of its
line. The indent points of Tab (Emacs `indent-relative`) are the indent and the start of each
word of the nearest non-blank line above; past the last one, the stops of the width.

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
electric re-indent joins the step of its character (`joinPreviousEditBlock`). An edit that
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
cycle, Shift+Tab, a selection, the electric characters, the off switch, read-only, one Undo
each); the dialogs; the Preferences row, the Edit menu, the storage, Reset. The batch cases
`mode-indent-*` re-indent the fixtures of `tests/texts/indent-*` and convert their tabs. The
style is checked on the editor's own sources, the course programs and the language files:
Indent Region leaves them as they are.

## Stages

1. The indentation (`EDIT-INDENT-1..9`) — in work.
