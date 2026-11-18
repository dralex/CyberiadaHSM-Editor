/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Insert Diagram dialog of a Markdown text
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

#include "insert_diagram_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "file_dialog.h"
#include "markdown_tools.h"

InsertDiagramDialog::InsertDiagramDialog(const QList<DiagramTab>& tabs, const QString& baseDir,
                                         const QString& folder, const QString& font, QWidget* parent):
    QDialog(parent), m_tabs(tabs), m_baseDir(baseDir)
{
    setWindowTitle(tr("Insert Diagram"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();

    QGridLayout* choice = new QGridLayout();
    m_tabRadio = new QRadioButton(tr("open tab"), this);
    m_tabBox = new QComboBox(this);
    for (const DiagramTab& tab : m_tabs) m_tabBox->addItem(tab.title, tab.path);
    m_fileRadio = new QRadioButton(tr("file"), this);
    m_fileEdit = new QLineEdit(this);
    m_browseButton = new QPushButton(tr("Browse..."), this);
    choice->addWidget(m_tabRadio, 0, 0);
    choice->addWidget(m_tabBox, 0, 1, 1, 2);
    choice->addWidget(m_fileRadio, 1, 0);
    choice->addWidget(m_fileEdit, 1, 1);
    choice->addWidget(m_browseButton, 1, 2);
    choice->setColumnStretch(1, 1);
    form->addRow(tr("Diagram:"), choice);

    m_folderEdit = new QLineEdit(folder, this);
    form->addRow(tr("Copy to:"), m_folderEdit);
    m_fontBox = new QFontComboBox(this);
    m_fontBox->setFontFilters(QFontComboBox::MonospacedFonts);
    m_fontBox->setCurrentFont(QFont(font));
    form->addRow(tr("Font:"), m_fontBox);
    m_altEdit = new QLineEdit(this);
    form->addRow(tr("Alt text:"), m_altEdit);
    m_writesLabel = new QLabel(this);
    m_writesLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Writes:"), m_writesLabel);
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

    bool haveTabs = !m_tabs.isEmpty();
    m_tabRadio->setEnabled(haveTabs);
    m_tabBox->setEnabled(haveTabs);
    (haveTabs ? m_tabRadio : m_fileRadio)->setChecked(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_browseButton, &QPushButton::clicked, this, &InsertDiagramDialog::browse);
    connect(m_tabRadio, &QRadioButton::toggled, this, &InsertDiagramDialog::sourceChanged);
    connect(m_tabBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InsertDiagramDialog::sourceChanged);
    connect(m_fileEdit, &QLineEdit::textChanged, this, &InsertDiagramDialog::sourceChanged);
    connect(m_folderEdit, &QLineEdit::textChanged, this, &InsertDiagramDialog::refresh);
    connect(m_altEdit, &QLineEdit::textEdited, this, [this]() { m_altTouched = true; });
    connect(m_altEdit, &QLineEdit::textChanged, this, &InsertDiagramDialog::refresh);
    sourceChanged();
}

QString InsertDiagramDialog::source() const
{
    QString path = m_tabRadio->isChecked() ? m_tabBox->currentData().toString() : m_fileEdit->text().trimmed();
    return path.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool InsertDiagramDialog::copies() const
{
    return !source().isEmpty() && !isInsideDirectory(source(), m_baseDir);
}

QString InsertDiagramDialog::target() const
{
    if (!copies()) return source();
    QDir folder(QDir(m_baseDir).filePath(m_folderEdit->text().trimmed()));
    return QDir::cleanPath(folder.filePath(QFileInfo(source()).fileName()));
}

QString InsertDiagramDialog::svg() const
{
    QFileInfo t(target());
    return target().isEmpty() ? QString() : QDir::cleanPath(t.absoluteDir().filePath(t.completeBaseName() + ".svg"));
}

QString InsertDiagramDialog::alt() const
{
    return m_altEdit->text();
}

QString InsertDiagramDialog::font() const
{
    return m_fontBox->currentFont().family();
}

QString InsertDiagramDialog::folder() const
{
    return copies() ? m_folderEdit->text().trimmed() : QString();
}

QString InsertDiagramDialog::insertedLine() const
{
    return target().isEmpty() ? QString() : markdownDiagramLine(alt(), svg(), target(), m_baseDir);
}

void InsertDiagramDialog::setFile(const QString& path)
{
    m_fileRadio->setChecked(true);
    m_fileEdit->setText(path);
}

void InsertDiagramDialog::browse()
{
    CyberiadaFileDialog dlg(this, tr("Insert Diagram"), tr("CyberiadaML diagrams (*.graphml)"));
    dlg.setFileMode(QFileDialog::ExistingFile);
    if (dlg.exec() == QDialog::Accepted && !dlg.selectedFile().isEmpty()) setFile(dlg.selectedFile());
}

void InsertDiagramDialog::sourceChanged()
{
    m_tabBox->setEnabled(m_tabRadio->isChecked());
    m_fileEdit->setEnabled(m_fileRadio->isChecked());
    m_browseButton->setEnabled(m_fileRadio->isChecked());
    QString path = source();
    // the alt text proposed from the diagram until it is typed
    if (path != m_lastSource) {
        m_lastSource = path;
        if (!m_altTouched) m_altEdit->setText(QFileInfo::exists(path) ? diagramFileTitle(path) : QString());
    }
    refresh();
}

void InsertDiagramDialog::refresh()
{
    QString path = source();
    bool exists = !path.isEmpty() && QFileInfo(path).isFile();
    m_folderEdit->setEnabled(copies());
    if (exists) {
        QStringList writes;
        if (copies()) writes << markdownPath(target(), m_baseDir);
        writes << markdownPath(svg(), m_baseDir);
        m_writesLabel->setText(writes.join(", "));
        m_lineLabel->setText(insertedLine());
    } else {
        m_writesLabel->clear();
        m_lineLabel->clear();
    }
    QString note;
    if (!path.isEmpty() && !exists) {
        note = tr("No such diagram file.");
    } else if (m_tabRadio->isChecked() && m_tabBox->currentIndex() >= 0 &&
               m_tabs.at(m_tabBox->currentIndex()).modified) {
        note = tr("The tab has unsaved changes: its saved file is drawn.");
    } else if (exists && !copies()) {
        note = tr("The diagram lies in the folder of the text: the image is written beside it.");
    }
    m_noteLabel->setText(note);
    // an empty folder is the directory of the text
    m_insertButton->setEnabled(exists);
}
