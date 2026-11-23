/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The formulas of a Markdown text: found, written and picked from the palette
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see https://www.gnu.org/licenses/
 *
 * ----------------------------------------------------------------------------- */

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

#include "markdown_formula.h"
#include "markdown_importer.h"
#include "markdown_lines.h"

namespace {

const QChar DOLLAR('$');
const QChar BACKSLASH('\\');
const QChar NEWLINE('\n');
const char* const DISPLAY_DOLLARS = "$$";

// a dollar md4c would take for a mark written \$
QString escapedDollars(const QString& line)
{
    QString result;
    for (int i = 0; i < line.length(); i++) {
        if (line[i] == DOLLAR && (i == 0 || line[i - 1] != BACKSLASH)) result += BACKSLASH;
        result += line[i];
    }
    return result;
}

// md4c opens a formula only after, and closes it only before, a space, a punctuation or a
// symbol character (Unicode P and S); a dollar beside would join the formula's dollars
bool needsSpace(QChar c)
{
    if (c.isNull() || c.isSpace()) return false;
    if (c == DOLLAR) return true;
    return !c.isPunct() && !c.isSymbol();
}

QString firstWord(const QString& line)
{
    static const QRegularExpression SPACES("\\s+");
    return line.trimmed().section(SPACES, 0, 0);
}

// the face drawn in a slot
const char* const SLOT_FACE = "\\square";

// an entry of the tables: its face given only when the slots alone would not show it
struct PaletteSource {
    const char* snippet;
    const char* face;
};

const PaletteSource GREEK[] = {
    {"\\alpha", nullptr}, {"\\beta", nullptr}, {"\\gamma", nullptr}, {"\\delta", nullptr},
    {"\\epsilon", nullptr}, {"\\varepsilon", nullptr}, {"\\zeta", nullptr}, {"\\eta", nullptr},
    {"\\theta", nullptr}, {"\\vartheta", nullptr}, {"\\iota", nullptr}, {"\\kappa", nullptr},
    {"\\lambda", nullptr}, {"\\mu", nullptr}, {"\\nu", nullptr}, {"\\xi", nullptr},
    {"\\pi", nullptr}, {"\\varpi", nullptr}, {"\\rho", nullptr}, {"\\varrho", nullptr},
    {"\\sigma", nullptr}, {"\\varsigma", nullptr}, {"\\tau", nullptr}, {"\\upsilon", nullptr},
    {"\\phi", nullptr}, {"\\varphi", nullptr}, {"\\chi", nullptr}, {"\\psi", nullptr},
    {"\\omega", nullptr}, {"\\Gamma", nullptr}, {"\\Delta", nullptr}, {"\\Theta", nullptr},
    {"\\Lambda", nullptr}, {"\\Xi", nullptr}, {"\\Pi", nullptr}, {"\\Sigma", nullptr},
    {"\\Upsilon", nullptr}, {"\\Phi", nullptr}, {"\\Psi", nullptr}, {"\\Omega", nullptr},
};

const PaletteSource OPERATORS[] = {
    {"\\pm", nullptr}, {"\\mp", nullptr}, {"\\times", nullptr}, {"\\div", nullptr},
    {"\\cdot", nullptr}, {"\\ast", nullptr}, {"\\star", nullptr}, {"\\circ", nullptr},
    {"\\bullet", nullptr}, {"\\cap", nullptr}, {"\\cup", nullptr}, {"\\setminus", nullptr},
    {"\\wedge", nullptr}, {"\\vee", nullptr}, {"\\neg", nullptr}, {"\\oplus", nullptr},
    {"\\ominus", nullptr}, {"\\otimes", nullptr}, {"\\odot", nullptr}, {"\\oslash", nullptr},
    {"\\sqcap", nullptr}, {"\\sqcup", nullptr}, {"\\dagger", nullptr}, {"\\ddagger", nullptr},
};

const PaletteSource RELATIONS[] = {
    {"\\neq", nullptr}, {"\\leq", nullptr}, {"\\geq", nullptr}, {"\\ll", nullptr},
    {"\\gg", nullptr}, {"\\approx", nullptr}, {"\\equiv", nullptr}, {"\\sim", nullptr},
    {"\\simeq", nullptr}, {"\\cong", nullptr}, {"\\propto", nullptr}, {"\\prec", nullptr},
    {"\\succ", nullptr}, {"\\in", nullptr}, {"\\notin", nullptr}, {"\\ni", nullptr},
    {"\\subset", nullptr}, {"\\supset", nullptr}, {"\\subseteq", nullptr}, {"\\supseteq", nullptr},
    {"\\perp", nullptr}, {"\\parallel", nullptr}, {"\\mid", nullptr}, {"\\models", nullptr},
    {"\\vdash", nullptr}, {"\\dashv", nullptr},
};

const PaletteSource ARROWS[] = {
    {"\\to", nullptr}, {"\\leftarrow", nullptr}, {"\\leftrightarrow", nullptr}, {"\\Rightarrow", nullptr},
    {"\\Leftarrow", nullptr}, {"\\Leftrightarrow", nullptr}, {"\\longrightarrow", nullptr},
    {"\\longleftarrow", nullptr}, {"\\longleftrightarrow", nullptr}, {"\\Longrightarrow", nullptr},
    {"\\Longleftarrow", nullptr}, {"\\Longleftrightarrow", nullptr}, {"\\mapsto", nullptr},
    {"\\iff", nullptr}, {"\\uparrow", nullptr}, {"\\downarrow", nullptr}, {"\\updownarrow", nullptr},
    {"\\Uparrow", nullptr}, {"\\Downarrow", nullptr}, {"\\Updownarrow", nullptr},
    {"\\nearrow", nullptr}, {"\\searrow", nullptr}, {"\\swarrow", nullptr}, {"\\nwarrow", nullptr},
    {"\\leftharpoonup", nullptr}, {"\\rightharpoonup", nullptr}, {"\\leftharpoondown", nullptr},
    {"\\rightharpoondown", nullptr},
};

const PaletteSource STRUCTURES[] = {
    {"^{#}", "x^{\\square}"}, {"_{#}", "x_{\\square}"}, {"_{#}^{#}", "x_{\\square}^{\\square}"},
    {"\\frac{#}{#}", nullptr}, {"\\dfrac{#}{#}", nullptr}, {"\\sqrt{#}", nullptr},
    {"\\sqrt[#]{#}", nullptr}, {"\\binom{#}{#}", nullptr}, {"\\sum_{#}^{#}", nullptr},
    {"\\prod_{#}^{#}", nullptr}, {"\\coprod_{#}^{#}", nullptr}, {"\\int_{#}^{#}", nullptr},
    {"\\iint_{#}", nullptr}, {"\\iiint_{#}", nullptr}, {"\\oint_{#}", nullptr},
    {"\\lim_{# \\to #}", nullptr}, {"\\bigcup_{#}^{#}", nullptr}, {"\\bigcap_{#}^{#}", nullptr},
    {"\\bigoplus_{#}", nullptr}, {"\\bigotimes_{#}", nullptr}, {"\\overline{#}", nullptr},
    {"\\underline{#}", nullptr}, {"\\vec{#}", nullptr}, {"\\hat{#}", nullptr},
    {"\\widehat{#}", nullptr}, {"\\bar{#}", nullptr}, {"\\dot{#}", nullptr}, {"\\ddot{#}", nullptr},
    {"\\tilde{#}", nullptr}, {"\\widetilde{#}", nullptr}, {"\\overset{#}{#}", nullptr},
    {"\\underset{#}{#}", nullptr},
    {"\\stackrel{#}{#}", nullptr}, {"\\boxed{#}", nullptr}, {"\\text{#}", "\\text{abc}"},
    {"\\mathrm{#}", "\\mathrm{d}"}, {"\\mathbf{#}", "\\mathbf{x}"}, {"\\mathit{#}", "\\mathit{x}"},
    {"\\mathbb{#}", "\\mathbb{R}"}, {"\\mathcal{#}", "\\mathcal{A}"}, {"\\mathfrak{#}", "\\mathfrak{g}"},
    {"\\operatorname{#}", "\\operatorname{rot}"},
};

const PaletteSource BRACKETS[] = {
    {"\\left( # \\right)", nullptr}, {"\\left[ # \\right]", nullptr}, {"\\left\\{ # \\right\\}", nullptr},
    {"\\left| # \\right|", nullptr}, {"\\left\\| # \\right\\|", nullptr},
    {"\\left\\langle # \\right\\rangle", nullptr}, {"\\left\\lfloor # \\right\\rfloor", nullptr},
    {"\\left\\lceil # \\right\\rceil", nullptr}, {"\\{", nullptr}, {"\\}", nullptr},
    {"\\langle", nullptr}, {"\\rangle", nullptr}, {"\\lfloor", nullptr}, {"\\rfloor", nullptr},
    {"\\lceil", nullptr}, {"\\rceil", nullptr}, {"\\|", nullptr},
};

const PaletteSource MATRICES[] = {
    {"\\begin{matrix} # & # \\\\ # & # \\end{matrix}", nullptr},
    {"\\begin{pmatrix} # & # \\\\ # & # \\end{pmatrix}", nullptr},
    {"\\begin{bmatrix} # & # \\\\ # & # \\end{bmatrix}", nullptr},
    {"\\begin{Bmatrix} # & # \\\\ # & # \\end{Bmatrix}", nullptr},
    {"\\begin{vmatrix} # & # \\\\ # & # \\end{vmatrix}", nullptr},
    {"\\begin{Vmatrix} # & # \\\\ # & # \\end{Vmatrix}", nullptr},
    {"\\begin{pmatrix} # & # & # \\\\ # & # & # \\\\ # & # & # \\end{pmatrix}", nullptr},
    {"\\begin{pmatrix} # \\\\ # \\end{pmatrix}", nullptr},
    {"\\begin{cases} # & \\text{if } # \\\\ # & \\text{otherwise} \\end{cases}", nullptr},
    {"\\begin{aligned} # &= # \\\\ &= # \\end{aligned}", nullptr},
    {"\\begin{array}{cc} # & # \\\\ # & # \\end{array}", nullptr},
};

const PaletteSource FUNCTIONS[] = {
    {"\\sin", nullptr}, {"\\cos", nullptr}, {"\\tan", nullptr}, {"\\cot", nullptr},
    {"\\sec", nullptr}, {"\\arcsin", nullptr}, {"\\arccos", nullptr}, {"\\arctan", nullptr},
    {"\\sinh", nullptr}, {"\\cosh", nullptr}, {"\\tanh", nullptr}, {"\\coth", nullptr},
    {"\\exp", nullptr}, {"\\log", nullptr}, {"\\ln", nullptr}, {"\\lim", nullptr},
    {"\\limsup", nullptr}, {"\\liminf", nullptr}, {"\\min", nullptr}, {"\\max", nullptr},
    {"\\det", nullptr}, {"\\dim", nullptr}, {"\\ker", nullptr}, {"\\deg", nullptr},
    {"\\gcd", nullptr}, {"\\arg", nullptr}, {"\\Pr", nullptr}, {"\\hom", nullptr},
};

const PaletteSource OTHER[] = {
    {"\\infty", nullptr}, {"\\partial", nullptr}, {"\\nabla", nullptr}, {"\\forall", nullptr},
    {"\\exists", nullptr}, {"\\nexists", nullptr}, {"\\emptyset", nullptr}, {"\\varnothing", nullptr},
    {"\\hbar", nullptr}, {"\\ell", nullptr}, {"\\Re", nullptr}, {"\\Im", nullptr},
    {"\\aleph", nullptr}, {"\\angle", nullptr}, {"\\triangle", nullptr}, {"\\therefore", nullptr},
    {"\\because", nullptr}, {"\\ldots", nullptr}, {"\\cdots", nullptr}, {"\\vdots", nullptr},
    {"\\ddots", nullptr}, {"\\prime", nullptr}, {"\\top", nullptr}, {"\\bot", nullptr},
    {"\\square", nullptr}, {"\\checkmark", nullptr}, {"\\quad", "a \\quad b"}, {"\\qquad", "a \\qquad b"},
};

template <size_t N>
FormulaTab paletteTab(const char* title, const PaletteSource (&sources)[N])
{
    FormulaTab tab;
    tab.title = title;
    for (size_t i = 0; i < N; i++) {
        tab.entries.append(FormulaEntry{QString::fromLatin1(sources[i].snippet),
                                        sources[i].face ? QString::fromLatin1(sources[i].face) : QString()});
    }
    return tab;
}

} // namespace

const QChar FORMULA_SLOT('#');

const QVector<FormulaTab>& formulaPalette()
{
    static const QVector<FormulaTab> tabs = {
        paletteTab(QT_TRANSLATE_NOOP("FormulaPalette", "Greek"), GREEK),
        paletteTab(QT_TRANSLATE_NOOP("FormulaPalette", "Operators"), OPERATORS),
        paletteTab(QT_TRANSLATE_NOOP("FormulaPalette", "Relations"), RELATIONS),
        paletteTab(QT_TRANSLATE_NOOP("FormulaPalette", "Arrows"), ARROWS),
        paletteTab(QT_TRANSLATE_NOOP("FormulaPalette", "Structures"), STRUCTURES),
        paletteTab(QT_TRANSLATE_NOOP("FormulaPalette", "Brackets"), BRACKETS),
        paletteTab(QT_TRANSLATE_NOOP("FormulaPalette", "Matrices"), MATRICES),
        paletteTab(QT_TRANSLATE_NOOP("FormulaPalette", "Functions"), FUNCTIONS),
        paletteTab(QT_TRANSLATE_NOOP("FormulaPalette", "Other"), OTHER),
    };
    return tabs;
}

QString formulaSnippetTex(const QString& snippet)
{
    return QString(snippet).remove(FORMULA_SLOT);
}

QString formulaEntryFace(const FormulaEntry& entry)
{
    return entry.face.isEmpty() ? QString(entry.snippet).replace(FORMULA_SLOT, SLOT_FACE) : entry.face;
}

FormulaEdit applyFormulaSnippet(const QString& text, int selectionStart, int selectionEnd, const QString& snippet)
{
    static const QRegularExpression COMMAND_END("\\\\[A-Za-z]+$");
    int start = qBound(0, qMin(selectionStart, selectionEnd), text.length());
    int end = qBound(start, qMax(selectionStart, selectionEnd), text.length());
    QString selected = text.mid(start, end - start);
    QString inserted;
    int cursor = -1;
    bool first = true;
    for (QChar c : snippet) {
        if (c != FORMULA_SLOT) {
            inserted += c;
        } else {
            if (first && !selected.isEmpty()) inserted += selected;
            else if (cursor < 0) cursor = inserted.length();
            first = false;
        }
    }
    // \alpha before b would read as \alphab
    if (end < text.length() && text[end].isLetter() && COMMAND_END.match(inserted).hasMatch()) inserted += ' ';
    FormulaEdit edit;
    edit.text = text.left(start) + inserted + text.mid(end);
    edit.cursor = start + (cursor < 0 ? inserted.length() : cursor);
    return edit;
}

MarkdownFormula markdownFormulaAt(const QString& text, int position)
{
    MarkdownFormula formula;
    if (position < 0 || position >= text.length()) return formula;
    for (const MarkdownMathRange& m : markdownSourceRanges(text).math) {
        if (m.begin <= position && position < m.end) {
            formula.begin = m.begin;
            formula.end = m.end;
            formula.tex = text.mid(m.texBegin, m.texEnd - m.texBegin);
            formula.display = text.midRef(m.begin, 2) == QLatin1String(DISPLAY_DOLLARS);
            break;
        }
    }
    return formula;
}

MarkdownFormula markdownFormulaAround(const QString& text, int cursor)
{
    MarkdownFormula formula = markdownFormulaAt(text, cursor);
    return formula.isNull() || formula.begin >= cursor ? MarkdownFormula() : formula;
}

QChar markdownCharBefore(const QString& text, int position)
{
    return position > 0 && position <= text.length() && text[position - 1] != NEWLINE ? text[position - 1] : QChar();
}

QChar markdownCharAfter(const QString& text, int position)
{
    return position >= 0 && position < text.length() && text[position] != NEWLINE ? text[position] : QChar();
}

QString markdownFormulaTex(const QString& tex)
{
    QString source = tex;
    source.replace("\r\n", "\n").replace(QChar::ParagraphSeparator, NEWLINE).replace('\r', NEWLINE);
    // a blank line would end the paragraph
    QStringList lines;
    for (QString line : source.split(NEWLINE)) {
        int n = line.length();
        while (n > 0 && line[n - 1].isSpace()) n--;
        line.truncate(n);
        if (!line.trimmed().isEmpty()) lines << escapedDollars(line);
    }
    if (lines.isEmpty()) return QString();
    lines.first() = lines.first().trimmed();
    return lines.join(NEWLINE);
}

QString markdownFormulaText(const QString& tex, bool display, QChar before, QChar after)
{
    QString normalized = markdownFormulaTex(tex);
    if (normalized.isEmpty()) return QString();
    QStringList lines = normalized.split(NEWLINE);
    QString result;
    if (!display || lines.size() == 1) {
        for (QString& line : lines) line = line.trimmed();
        const char* dollars = display ? DISPLAY_DOLLARS : "$";
        result = dollars + lines.join(' ') + dollars;
    } else {
        // a line that would open a block goes on the line before it, the opening $$ too
        QStringList written;
        written << DISPLAY_DOLLARS;
        for (const QString& line : lines) {
            if (markdownOpensBlock(firstWord(line))) written.last() += ' ' + line.trimmed();
            else written << line;
        }
        written << DISPLAY_DOLLARS;
        result = written.join(NEWLINE);
    }
    if (needsSpace(before)) result.prepend(' ');
    if (needsSpace(after)) result.append(' ');
    return result;
}
