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

// the language of the document and the switches; every highlighter follows it
class CodeStyle : public QObject {
    Q_OBJECT

public:
    CodeStyle(CodeStyle &other) = delete;
    void operator=(const CodeStyle &) = delete;

    static CodeStyle& instance() {
        static CodeStyle instance;
        return instance;
    }

    // the platformLanguage value; matched by the name or an alias, any case
    void setLanguage(const QString& name);
    QString languageName() const { return currentName; }
    // the language to draw with, null when the text is plain
    const CodeLanguage* activeLanguage() const;
    QStringList languages() const;
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
    const CodeLanguage* find(const QString& name) const;

    QVector<CodeLanguage> registry;
    QString currentName;
    const CodeLanguage* current = nullptr;
    bool exportPlain = false;
};

class CodeHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    CodeHighlighter(QTextDocument* document, CodeRole role);

    CodeRole getRole() const { return role; }
    void setRole(CodeRole newRole);

protected:
    void highlightBlock(const QString& text) override;

private:
    CodeRole role;
};

#endif // CODE_HIGHLIGHTER_H
