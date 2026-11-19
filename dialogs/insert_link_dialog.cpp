/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Insert Link dialog of a Markdown text
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

#include "insert_link_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "file_dialog.h"
#include "markdown_tools.h"

// a heading's title in the list, after the anchor
static const int HEADING_TITLE_ROLE = Qt::UserRole + 1;
// the indent of a heading level in the list
static const char* const HEADING_INDENT = "    ";

InsertLinkDialog::InsertLinkDialog(const QString& textPath, const QString& ownText, const QString& selection,
                                   const std::function<QString(const QString&)>& openText, QWidget* parent):
    QDialog(parent),
    m_textPath(QDir::cleanPath(QFileInfo(textPath).absoluteFilePath())),
    m_baseDir(QFileInfo(textPath).absolutePath()), m_ownText(ownText), m_openText(openText)
{
    setWindowTitle(tr("Insert Link"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();
    m_textEdit = new QLineEdit(selection, this);
    form->addRow(tr("Text:"), m_textEdit);

    QGridLayout* target = new QGridLayout();
    m_headingRadio = new QRadioButton(tr("heading"), this);
    m_textBox = new QComboBox(this);
    m_textBox->addItem(tr("this text"), QString());
    QString own = relative(m_textPath);
    for (const QString& file : markdownFilesUnder(m_baseDir)) {
        if (file != own) m_textBox->addItem(file, QDir::cleanPath(QDir(m_baseDir).absoluteFilePath(file)));
    }
    m_browseTextButton = new QPushButton(tr("Browse..."), this);
    m_headingBox = new QComboBox(this);
    m_fileRadio = new QRadioButton(tr("file"), this);
    m_fileEdit = new QLineEdit(this);
    m_browseFileButton = new QPushButton(tr("Browse..."), this);
    m_addressRadio = new QRadioButton(tr("address"), this);
    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setPlaceholderText("https://");
    target->addWidget(m_headingRadio, 0, 0);
    target->addWidget(new QLabel(tr("Text:"), this), 0, 1);
    target->addWidget(m_textBox, 0, 2);
    target->addWidget(m_browseTextButton, 0, 3);
    target->addWidget(new QLabel(tr("Heading:"), this), 1, 1);
    target->addWidget(m_headingBox, 1, 2, 1, 2);
    target->addWidget(m_fileRadio, 2, 0);
    target->addWidget(m_fileEdit, 2, 1, 1, 2);
    target->addWidget(m_browseFileButton, 2, 3);
    target->addWidget(m_addressRadio, 3, 0);
    target->addWidget(m_addressEdit, 3, 1, 1, 3);
    target->setColumnStretch(2, 1);
    form->addRow(tr("Target:"), target);
    m_lineLabel = new QLabel(this);
    m_lineLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Inserts:"), m_lineLabel);
    layout->addLayout(form);
    m_noteLabel = new QLabel(this);
    m_noteLabel->setWordWrap(true);
    layout->addWidget(m_noteLabel);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_insertButton = buttons->button(QDialogButtonBox::Ok);
    m_insertButton->setText(tr("Insert"));
    layout->addWidget(buttons);

    // a selected text is the link text as it is
    m_textTouched = !selection.isEmpty();
    m_headingRadio->setChecked(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_browseTextButton, &QPushButton::clicked, this, &InsertLinkDialog::browseText);
    connect(m_browseFileButton, &QPushButton::clicked, this, &InsertLinkDialog::browseFile);
    for (QRadioButton* radio : QList<QRadioButton*>({m_headingRadio, m_fileRadio, m_addressRadio})) {
        connect(radio, &QRadioButton::toggled, this, &InsertLinkDialog::refresh);
    }
    connect(m_textBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InsertLinkDialog::textSourceChanged);
    connect(m_headingBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InsertLinkDialog::refresh);
    connect(m_fileEdit, &QLineEdit::textChanged, this, &InsertLinkDialog::refresh);
    connect(m_addressEdit, &QLineEdit::textChanged, this, &InsertLinkDialog::refresh);
    // textChanged comes before textEdited: a keystroke marks the text typed, then refreshes
    connect(m_textEdit, &QLineEdit::textEdited, this, [this]() {
        m_textTouched = true;
        refresh();
    });
    textSourceChanged();
}

QString InsertLinkDialog::relative(const QString& path) const
{
    return QDir(m_baseDir).relativeFilePath(QFileInfo(path).absoluteFilePath());
}

QString InsertLinkDialog::chosenText() const
{
    return m_textBox->currentData().toString();
}

QString InsertLinkDialog::target() const
{
    if (m_fileRadio->isChecked()) return m_fileEdit->text().trimmed();
    if (m_addressRadio->isChecked()) return m_addressEdit->text().trimmed();
    QString anchor = m_headingBox->currentData().toString();
    if (anchor.isEmpty()) return QString();
    return (chosenText().isEmpty() ? QString() : relative(chosenText())) + "#" + anchor;
}

QString InsertLinkDialog::text() const
{
    return m_textEdit->text();
}

QString InsertLinkDialog::insertedLine() const
{
    if (text().trimmed().isEmpty() || target().isEmpty()) return QString();
    return markdownLinkLine(text(), target());
}

void InsertLinkDialog::chooseText(const QString& path)
{
    QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    m_headingRadio->setChecked(true);
    if (absolute == m_textPath) {
        m_textBox->setCurrentIndex(0);
        return;
    }
    int at = m_textBox->findData(absolute);
    if (at < 0) {
        m_textBox->addItem(relative(absolute), absolute);
        at = m_textBox->count() - 1;
    }
    m_textBox->setCurrentIndex(at);
}

void InsertLinkDialog::chooseFile(const QString& path)
{
    m_fileRadio->setChecked(true);
    m_fileEdit->setText(relative(path));
}

void InsertLinkDialog::browseText()
{
    CyberiadaFileDialog dlg(this, tr("Insert Link"), tr("Markdown texts (%1)").arg("*.md *.markdown"));
    dlg.setFileMode(QFileDialog::ExistingFile);
    if (dlg.exec() == QDialog::Accepted && !dlg.selectedFile().isEmpty()) chooseText(dlg.selectedFile());
}

void InsertLinkDialog::browseFile()
{
    CyberiadaFileDialog dlg(this, tr("Insert Link"), tr("All files (*)"));
    dlg.setFileMode(QFileDialog::ExistingFile);
    if (dlg.exec() == QDialog::Accepted && !dlg.selectedFile().isEmpty()) chooseFile(dlg.selectedFile());
}

void InsertLinkDialog::textSourceChanged()
{
    // the headings of the chosen text: of its open tab, unsaved ones too, else of its file
    QString markdown = m_ownText;
    QString path = chosenText();
    if (!path.isEmpty()) {
        markdown = m_openText ? m_openText(path) : QString();
        if (markdown.isNull()) {
            QFile file(path);
            markdown = file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
        }
    }
    QSignalBlocker block(m_headingBox);
    m_headingBox->clear();
    for (const MarkdownHeading& h : markdownHeadings(markdown)) {
        m_headingBox->addItem(QString(HEADING_INDENT).repeated(h.level - 1) + h.title, h.anchor);
        m_headingBox->setItemData(m_headingBox->count() - 1, h.title, HEADING_TITLE_ROLE);
    }
    refresh();
}

void InsertLinkDialog::refresh()
{
    bool heading = m_headingRadio->isChecked();
    m_textBox->setEnabled(heading);
    m_browseTextButton->setEnabled(heading);
    m_headingBox->setEnabled(heading);
    m_fileEdit->setEnabled(m_fileRadio->isChecked());
    m_browseFileButton->setEnabled(m_fileRadio->isChecked());
    m_addressEdit->setEnabled(m_addressRadio->isChecked());
    // the link text proposed by the target until it is typed
    if (!m_textTouched) {
        QString proposed;
        if (heading) proposed = m_headingBox->currentData(HEADING_TITLE_ROLE).toString();
        else if (m_fileRadio->isChecked()) proposed = QFileInfo(target()).fileName();
        else proposed = target();
        QSignalBlocker block(m_textEdit);
        m_textEdit->setText(proposed);
    }
    QString note;
    bool missing = false;
    if (heading && m_headingBox->count() == 0) {
        note = tr("The text has no headings.");
    } else if (m_fileRadio->isChecked() && !target().isEmpty() &&
               !QFileInfo::exists(QDir(m_baseDir).absoluteFilePath(target()))) {
        note = tr("No such file.");
        missing = true;
    }
    m_noteLabel->setText(note);
    m_lineLabel->setText(insertedLine());
    m_insertButton->setEnabled(!insertedLine().isEmpty() && !missing);
}
