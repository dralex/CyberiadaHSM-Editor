/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Color Schemes Panel of the Preferences
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

#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QToolButton>
#include <QVBoxLayout>
#include "text_scheme_panel.h"
#include "code_editor.h"
#include "code_highlighter.h"
#include "cyberiada_constants.h"
#include "settings_manager.h"

// the editor colors in the first column, the highlighting in the next ones
static const int SWATCH_ROWS = textSchemeFirstToken;
static const char* const ROLE_PROPERTY = "schemeRole";

TextSchemePanel::TextSchemePanel(QWidget* parent):
    QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* top = new QHBoxLayout;
    QLabel* label = new QLabel(tr("Scheme"), this);
    m_list = new QComboBox(this);
    m_list->setObjectName("textSchemeList");
    label->setBuddy(m_list);
    top->addWidget(label);
    top->addWidget(m_list, 1);
    layout->addLayout(top);

    QHBoxLayout* buttons = new QHBoxLayout;
    m_copy = new QPushButton(tr("Copy..."), this);
    m_copy->setObjectName("textSchemeCopy");
    m_rename = new QPushButton(tr("Rename..."), this);
    m_rename->setObjectName("textSchemeRename");
    m_delete = new QPushButton(tr("Delete"), this);
    m_delete->setObjectName("textSchemeDelete");
    m_restore = new QPushButton(tr("Restore"), this);
    m_restore->setObjectName("textSchemeRestore");
    m_restore->setToolTip(tr("The colors of the built-in scheme"));
    buttons->addStretch(1);
    buttons->addWidget(m_copy);
    buttons->addWidget(m_rename);
    buttons->addWidget(m_delete);
    buttons->addWidget(m_restore);
    layout->addLayout(buttons);

    QGridLayout* grid = new QGridLayout;
    int side = fontMetrics().height();
    for (int role = 0; role < textSchemeRolesCount; role++) {
        QToolButton* swatch = new QToolButton(this);
        swatch->setObjectName("textSchemeSwatch-" + textSchemeRoleKey(role));
        swatch->setProperty(ROLE_PROPERTY, role);
        swatch->setText(textSchemeRoleName(role));
        swatch->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        swatch->setIconSize(QSize(side, side));
        swatch->setAutoRaise(true);
        grid->addWidget(swatch, role % SWATCH_ROWS, role / SWATCH_ROWS, Qt::AlignLeft);
        connect(swatch, &QToolButton::clicked, this, &TextSchemePanel::swatchClicked);
        m_swatches[role] = swatch;
    }
    layout->addLayout(grid);

    // a Markdown text with a C++ block shows every color
    m_preview = new CodeEditor(this);
    m_preview->setObjectName("textSchemePreview");
    m_preview->setReadOnly(true);
    m_preview->setPlainText(tr("# Heading *emphasis*\n"
                               "> A quote, a [link](https://example.org), $x^2$\n"
                               "- `code` <!-- a comment -->\n"
                               "\n"
                               "```cpp\n"
                               "// a comment\n"
                               "int main() { return printf(\"%d\", 42); }\n"
                               "```"));
    m_highlighter = new CodeHighlighter(m_preview->document(), codeRoleMarkup, nullptr, codeMarkupMarkdown);
    // the selection shown too
    QTextCursor selected = m_preview->document()->find("main");
    if (!selected.isNull()) m_preview->setTextCursor(selected);
    m_preview->setMinimumHeight(m_preview->fontMetrics().lineSpacing() * (m_preview->blockCount() + 1));
    layout->addWidget(m_preview, 1);

    connect(m_list, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TextSchemePanel::schemeChosen);
    connect(m_copy, &QPushButton::clicked, this, &TextSchemePanel::copyClicked);
    connect(m_rename, &QPushButton::clicked, this, &TextSchemePanel::renameClicked);
    connect(m_delete, &QPushButton::clicked, this, &TextSchemePanel::deleteScheme);
    connect(m_restore, &QPushButton::clicked, this, &TextSchemePanel::restoreScheme);
    load();
}

void TextSchemePanel::load()
{
    SettingsManager& sm = SettingsManager::instance();
    m_schemes = sm.getTextSchemes();
    fill(sm.currentTextScheme().id);
}

void TextSchemePanel::apply()
{
    SettingsManager& sm = SettingsManager::instance();
    sm.setTextSchemes(m_schemes);
    sm.setTextScheme(shownId());
}

QString TextSchemePanel::shownId() const
{
    return m_list->currentData().toString();
}

TextScheme* TextSchemePanel::shown()
{
    QString id = shownId();
    for (TextScheme& scheme : m_schemes) {
        if (scheme.id == id) return &scheme;
    }
    return &m_schemes.first();
}

bool TextSchemePanel::showScheme(const QString& id)
{
    int index = m_list->findData(id);
    if (index < 0) return false;
    m_list->setCurrentIndex(index);
    return true;
}

void TextSchemePanel::fill(const QString& id)
{
    {
        QSignalBlocker block(m_list);
        m_list->clear();
        for (const TextScheme& scheme : m_schemes) m_list->addItem(scheme.displayName(), scheme.id);
        m_list->setCurrentIndex(qMax(0, m_list->findData(id)));
    }
    showColors();
}

void TextSchemePanel::showColors()
{
    const TextScheme& scheme = *shown();
    for (int role = 0; role < textSchemeRolesCount; role++) {
        QPixmap pixmap(m_swatches[role]->iconSize());
        pixmap.fill(scheme.colors[role]);
        // a frame keeps a color like the panel's own visible
        QPainter painter(&pixmap);
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
        painter.end();
        m_swatches[role]->setIcon(QIcon(pixmap));
        m_swatches[role]->setToolTip(scheme.colors[role].name());
    }
    m_rename->setEnabled(!scheme.isBuiltin());
    m_delete->setEnabled(!scheme.isBuiltin());
    m_restore->setEnabled(scheme.isBuiltin() && !scheme.sameColors(builtinTextScheme(scheme.id)));
    m_preview->setScheme(scheme);
    m_highlighter->setScheme(scheme);
}

void TextSchemePanel::schemeChosen()
{
    showColors();
}

void TextSchemePanel::setColor(int role, const QColor& color)
{
    if (role < 0 || role >= textSchemeRolesCount || !color.isValid()) return;
    shown()->colors[role] = color;
    showColors();
}

void TextSchemePanel::swatchClicked()
{
    int role = sender()->property(ROLE_PROPERTY).toInt();
    QColor color = QColorDialog::getColor(shown()->colors[role], this, textSchemeRoleName(role));
    setColor(role, color);
}

bool TextSchemePanel::isFree(const QString& name, const QString& ownId) const
{
    for (const TextScheme& scheme : m_schemes) {
        if (scheme.id == ownId) continue;
        if (scheme.name.compare(name, Qt::CaseInsensitive) == 0 ||
            scheme.displayName().compare(name, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return true;
}

bool TextSchemePanel::copyScheme(const QString& name)
{
    QString given = name.trimmed();
    if (given.isEmpty() || !isFree(given, QString())) return false;
    TextScheme copy = *shown();
    int n = 1;
    while (findTextScheme(m_schemes, TEXT_SCHEME_USER_PREFIX + QString::number(n))) n++;
    copy.id = TEXT_SCHEME_USER_PREFIX + QString::number(n);
    copy.name = given;
    m_schemes << copy;
    fill(copy.id);
    return true;
}

bool TextSchemePanel::renameScheme(const QString& name)
{
    TextScheme* scheme = shown();
    QString given = name.trimmed();
    if (scheme->isBuiltin() || given.isEmpty() || !isFree(given, scheme->id)) return false;
    scheme->name = given;
    fill(scheme->id);
    return true;
}

void TextSchemePanel::deleteScheme()
{
    QString id = shownId();
    for (int i = 0; i < m_schemes.size(); i++) {
        if (m_schemes[i].id == id && !m_schemes[i].isBuiltin()) {
            m_schemes.removeAt(i);
            fill(TEXT_SCHEME_DEFAULT);
            return;
        }
    }
}

void TextSchemePanel::restoreScheme()
{
    TextScheme* scheme = shown();
    if (!scheme->isBuiltin()) return;
    *scheme = builtinTextScheme(scheme->id);
    showColors();
}

QString TextSchemePanel::askName(const QString& title, const QString& proposed, const QString& ownId)
{
    bool ok = false;
    QString name = QInputDialog::getText(this, title, tr("Name"), QLineEdit::Normal, proposed, &ok).trimmed();
    if (!ok || name.isEmpty()) return QString();
    if (!isFree(name, ownId)) {
        QMessageBox::warning(this, title, tr("A scheme named \"%1\" exists already.").arg(name));
        return QString();
    }
    return name;
}

void TextSchemePanel::copyClicked()
{
    QString name = askName(tr("Copy the Scheme"), tr("%1 copy").arg(shown()->displayName()), QString());
    if (!name.isEmpty()) copyScheme(name);
}

void TextSchemePanel::renameClicked()
{
    TextScheme* scheme = shown();
    if (scheme->isBuiltin()) return;
    QString name = askName(tr("Rename the Scheme"), scheme->name, scheme->id);
    if (!name.isEmpty()) renameScheme(name);
}
