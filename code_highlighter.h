/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
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
    codeRoleFormalComment    // the whole text
};

enum CodeTokenKind {
    codeTokenKeyword = 0,
    codeTokenType,
    codeTokenString,
    codeTokenNumber,
    codeTokenComment,
    codeTokenKindsCount
};

struct CodeSpan {
    int start;
    int length;
    CodeTokenKind kind;
};

// one rule set of syntax/<language>.json
struct CodeLanguage {
    QString name;
    QStringList aliases;
    QString lineComment;
    QString blockOpen;
    QString blockClose;
    QString quotes;
    bool preprocessor = false;
    QSet<QString> keywords;
    QSet<QString> types;
};

// the languages and the switches; the language itself belongs to a document
class CodeStyle : public QObject {
    Q_OBJECT

public:
    CodeStyle(CodeStyle &other) = delete;
    void operator=(const CodeStyle &) = delete;

    static CodeStyle& instance() {
        static CodeStyle instance;
        return instance;
    }

    // the platformLanguage value matched by the name or an alias, any case;
    // null for an empty or unknown value
    const CodeLanguage* find(const QString& name) const;
    QStringList languages() const;
    // the language to draw with, null when the switches make the text plain
    const CodeLanguage* effective(const CodeLanguage* lang) const;
    // an export draws plain text unless the preference keeps the colours
    void overrideExportPlain(bool on);

    QTextCharFormat format(CodeTokenKind kind) const;

    // the coloured spans of the whole text
    static QVector<CodeSpan> spans(const QString& text, CodeRole role, const CodeLanguage& lang);

signals:
    void styleChanged();

private:
    CodeStyle();
    void loadLanguages();

    QVector<CodeLanguage> registry;
    bool exportPlain = false;
};

class CodeHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    CodeHighlighter(QTextDocument* document, CodeRole role,
                    const CodeLanguage* language = nullptr);

    CodeRole getRole() const { return role; }
    void setRole(CodeRole newRole);
    const CodeLanguage* getLanguage() const { return language; }
    void setLanguage(const CodeLanguage* newLanguage);

protected:
    void highlightBlock(const QString& text) override;

private:
    CodeRole role;
    const CodeLanguage* language;
};

#endif // CODE_HIGHLIGHTER_H
