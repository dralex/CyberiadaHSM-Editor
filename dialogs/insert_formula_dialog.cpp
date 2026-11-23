/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Insert Formula dialog of a Markdown text
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

#include "insert_formula_dialog.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include "markdown_formula.h"
#include "math_renderer.h"

namespace {

// the pause after the last keystroke before the preview is drawn again
const int PREVIEW_DELAY_MS = 200;
// the preview against the dialog font
const qreal PREVIEW_SCALE = 1.5;
const int PREVIEW_MARGIN = 8;
const int PREVIEW_LINES = 4;
const int TEX_LINES = 4;
const int DIALOG_WIDTH = 560;

// the formula on a background, the margin around it
QPixmap formulaPixmap(const MathBox& box, const QColor& background, int margin, qreal ratio)
{
    QSize size(qCeil(box.width) + 2 * margin, qCeil(box.ascent + box.descent) + 2 * margin);
    QPixmap pixmap(size * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(background);
    QPainter painter(&pixmap);
    painter.drawPicture(QPointF(margin, margin + box.ascent), box.picture);
    return pixmap;
}

} // namespace

InsertFormulaDialog::InsertFormulaDialog(const QString& tex, bool display, const FormulaPlace& place, bool editing,
                                         QWidget* parent):
    QDialog(parent), m_place(place)
{
    setWindowTitle(editing ? tr("Edit Formula") : tr("Insert Formula"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();
    m_texEdit = new QPlainTextEdit(this);
    m_texEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_texEdit->setTabChangesFocus(true);
    m_texEdit->setPlainText(tex);
    m_texEdit->setFixedHeight(m_texEdit->fontMetrics().lineSpacing() * TEX_LINES +
                              2 * (m_texEdit->frameWidth() + int(m_texEdit->document()->documentMargin())));
    form->addRow(tr("TeX:"), m_texEdit);

    QHBoxLayout* kind = new QHBoxLayout();
    m_inlineRadio = new QRadioButton(tr("inline $..$"), this);
    m_displayRadio = new QRadioButton(tr("display $$..$$"), this);
    kind->addWidget(m_inlineRadio);
    kind->addWidget(m_displayRadio);
    kind->addStretch();
    form->addRow(tr("Kind:"), kind);

    // black on white as the rendered page, a long formula scrolled
    m_previewLabel = new QLabel(this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    QScrollArea* preview = new QScrollArea(this);
    preview->setWidget(m_previewLabel);
    preview->setWidgetResizable(true);
    preview->setAlignment(Qt::AlignCenter);
    QPalette white = preview->palette();
    white.setColor(QPalette::Window, Qt::white);
    preview->setPalette(white);
    preview->setBackgroundRole(QPalette::Window);
    m_previewLabel->setPalette(white);
    preview->setMinimumHeight(fontMetrics().lineSpacing() * PREVIEW_LINES);
    form->addRow(tr("Preview:"), preview);
    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setTextFormat(Qt::PlainText);
    QPalette red = m_errorLabel->palette();
    red.setColor(QPalette::WindowText, Qt::red);
    m_errorLabel->setPalette(red);
    form->addRow(QString(), m_errorLabel);
    m_lineLabel = new QLabel(this);
    m_lineLabel->setTextFormat(Qt::PlainText);
    m_lineLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Inserts:"), m_lineLabel);
    layout->addLayout(form);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_insertButton = buttons->button(QDialogButtonBox::Ok);
    m_insertButton->setText(editing ? tr("Replace") : tr("Insert"));
    layout->addWidget(buttons);
    resize(DIALOG_WIDTH, sizeHint().height());

    (display ? m_displayRadio : m_inlineRadio)->setChecked(true);
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(PREVIEW_DELAY_MS);
    connect(m_previewTimer, &QTimer::timeout, this, &InsertFormulaDialog::renderPreview);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_texEdit, &QPlainTextEdit::textChanged, this, [this]() {
        refresh();
        m_previewTimer->start();
    });
    // a switch of the kind is drawn at once
    connect(m_displayRadio, &QRadioButton::toggled, this, [this]() {
        refresh();
        renderPreview();
    });
    refresh();
    renderPreview();
    m_texEdit->setFocus();
    m_texEdit->moveCursor(QTextCursor::End);
}

QString InsertFormulaDialog::tex() const
{
    return m_texEdit->toPlainText();
}

bool InsertFormulaDialog::display() const
{
    return m_displayRadio->isChecked();
}

QString InsertFormulaDialog::insertedText() const
{
    // a display formula of its own paragraph has no neighbours
    bool block = display() && m_place.displayBlock;
    return markdownFormulaText(tex(), display(), block ? QChar() : m_place.before, block ? QChar() : m_place.after);
}

void InsertFormulaDialog::refresh()
{
    QString text = insertedText();
    m_lineLabel->setText(text);
    m_insertButton->setEnabled(!text.isEmpty());
}

void InsertFormulaDialog::renderPreview()
{
    m_previewTimer->stop();
    QString source = markdownFormulaTex(tex());
    if (source.isEmpty()) {
        m_preview = QPixmap();
        m_previewLabel->clear();
        m_errorLabel->clear();
        return;
    }
    QFont font = this->font();
    if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * PREVIEW_SCALE);
    else font.setPixelSize(qRound(font.pixelSize() * PREVIEW_SCALE));
    const MathBox& box = MathRenderer::instance().render(source, display(), font, Qt::black);
    m_preview = formulaPixmap(box, Qt::white, PREVIEW_MARGIN, devicePixelRatioF());
    m_previewLabel->setPixmap(m_preview);
    m_errorLabel->setText(box.errors);
}
