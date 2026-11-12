/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Code Syntax Highlighting by the Platform Language
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

#ifndef CODE_HIGHLIGHTER_H
#define CODE_HIGHLIGHTER_H

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

// where the code starts in a text (EDIT-TEXT-7)
enum CodeRole {
    codeRoleNone = 0,
    codeRoleBehaviour,       // "entry/ code"
    codeRoleTransition,      // "EVENT [code] / code"
    codeRoleFormalComment,   // the whole text
    codeRoleMarkup           // an informal comment body by its markup (EDIT-TEXT-11)
};

// the markup language of an informal comment (PNST 1044 9.3)
enum CodeMarkup {
    codeMarkupPlain = 0,
    codeMarkupMarkdown
};

enum CodeTokenKind {
    codeTokenKeyword = 0,
    codeTokenType,
    codeTokenString,
    codeTokenNumber,
    codeTokenComment,
    // the Markdown elements
    codeTokenHeading,
    codeTokenEmphasis,
    codeTokenCode,
    codeTokenLink,
    codeTokenMarker,
    // the text of a quote, beside its marker and inline elements
    codeTokenQuote,
    codeTokenKindsCount
};

struct CodeSpan {
    int start;
    int length;
    CodeTokenKind kind;
};

// one rule set of syntax/<language>.json
struct CodeLanguage {
    // the file name without .json: the id of its text mode (EDIT-MODE-1)
    QString id;
    QString name;
    QStringList aliases;
    // the file extensions of its sources, lower case with the dot (EDIT-MODE-13)
    QStringList extensions;
    QString lineComment;
    QString blockOpen;
    QString blockClose;
    QString quotes;
    bool preprocessor = false;
    QSet<QString> keywords;
    QSet<QString> types;
    // the plugin that brings it, empty for a built-in one (EDIT-PLUG-60)
    QString owner;
};

// a language file of a plugin: the input of setPluginLanguages and what became of it
struct CodeLanguageFile {
    QString owner;     // the plugin id
    QString name;      // as the descriptor names it
    QString path;      // absolute
    QString id;        // the language added, empty when skipped
    QString warning;   // why it is skipped
};

// the languages and the switches; the language itself belongs to a document
class CodeStyle : public QObject {
    Q_OBJECT

public:
    CodeStyle(CodeStyle &other) = delete;
    void operator=(const CodeStyle &) = delete;
    ~CodeStyle();

    static CodeStyle& instance() {
        static CodeStyle instance;
        return instance;
    }

    // the platformLanguage value matched by the name or an alias, any case;
    // null for an empty or unknown value
    const CodeLanguage* find(const QString& name) const;
    QStringList languages() const;
    // the language of a mode id, of a file extension (".cpp", any case); null without one
    const CodeLanguage* findId(const QString& id) const;
    const CodeLanguage* forExtension(const QString& extension) const;
    QStringList ids() const;
    // the languages of the plugins after the built-in ones, in the order given; a file
    // that is missing, malformed or whose id, name or alias is taken is skipped. A
    // language replaced stays allocated, so a pointer held to it never dangles
    void setPluginLanguages(QList<CodeLanguageFile>& files);
    // the language to draw with, null when the switches make the text plain
    const CodeLanguage* effective(const CodeLanguage* lang) const;
    bool isActive() const;

    // the markupLanguage or dMarkup value: empty and "plain" are plain, "markdown"
    // or "md" Markdown; known is false for any other value
    static CodeMarkup findMarkup(const QString& value, bool* known = nullptr);
    // the value written for the markup, empty for plain
    static QString markupValue(CodeMarkup markup);
    // an informal comment: its own dMarkup when set, else the document markup
    static CodeMarkup commentMarkup(const QString& own, CodeMarkup document);
    // an export draws plain text unless the preference keeps the colours
    void overrideExportPlain(bool on);

    QTextCharFormat format(CodeTokenKind kind) const;

    // the coloured spans of the whole text
    static QVector<CodeSpan> spans(const QString& text, CodeRole role, const CodeLanguage& lang);
    // the Markdown spans; a fenced block by the language of its tag, else by the fallback
    QVector<CodeSpan> markdownSpans(const QString& text, const CodeLanguage* fallback) const;

signals:
    void styleChanged();
    // the plugin languages changed: the holders of a language look it up again
    void languagesChanged();

private:
    CodeStyle();
    void loadLanguages();
    // the id, the name or an alias another language has, empty when none
    QString clash(const CodeLanguage& lang) const;

    QVector<CodeLanguage*> registry;
    QVector<CodeLanguage*> retired;
    bool exportPlain = false;
};

class CodeHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    CodeHighlighter(QTextDocument* document, CodeRole role,
                    const CodeLanguage* language = nullptr,
                    CodeMarkup markup = codeMarkupPlain);

    CodeRole getRole() const { return role; }
    void setRole(CodeRole newRole);
    const CodeLanguage* getLanguage() const { return language; }
    void setLanguage(const CodeLanguage* newLanguage);
    CodeMarkup getMarkup() const { return markup; }
    void setMarkup(CodeMarkup newMarkup);

protected:
    void highlightBlock(const QString& text) override;

private:
    CodeRole role;
    // the code language; for the markup role, the fallback of a fenced block
    const CodeLanguage* language;
    CodeMarkup markup;
};

#endif // CODE_HIGHLIGHTER_H
