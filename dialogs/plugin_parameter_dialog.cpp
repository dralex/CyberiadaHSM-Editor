/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Plugin Parameter Dialog, generated from the parameters (docs/PLUGIN-API.md)
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

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <climits>

#include "plugin_parameter_dialog.h"
#include "code_highlighter.h"
#include "fontmanager.h"

// the lines of a text box before it scrolls
#define TEXT_BOX_LINES 8

PluginParameterDialog::PluginParameterDialog(const QString& title,
                                             const QList<const PluginParameter*>& parameters,
                                             const PluginValues& values, const QString& acceptText,
                                             QWidget* parent):
    QDialog(parent), parameters(parameters)
{
    setWindowTitle(title);
    QVBoxLayout* layout = new QVBoxLayout(this);
    // the text boxes come first, the short fields form the form below them
    for (const PluginParameter* p: parameters) {
        if (p->type != PluginParameter::typeText) continue;
        QLabel* label = new QLabel(p->label, this);
        label->setToolTip(p->description);
        layout->addWidget(label);
        layout->addWidget(createEditor(p, values.value(p->id)));
    }
    QFormLayout* form = new QFormLayout();
    for (const PluginParameter* p: parameters) {
        if (p->type == PluginParameter::typeText) continue;
        QWidget* editor = createEditor(p, values.value(p->id));
        QLabel* label = new QLabel(p->label, this);
        label->setToolTip(p->description);
        form->addRow(label, editor);
    }
    layout->addLayout(form);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    runButton = buttons->button(QDialogButtonBox::Ok);
    runButton->setText(acceptText);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateAccept();
}

QWidget* PluginParameterDialog::createEditor(const PluginParameter* p, const QString& value)
{
    QWidget* editor = nullptr;
    QWidget* field = nullptr;
    switch (p->type) {
    case PluginParameter::typeText: {
        QPlainTextEdit* edit = new QPlainTextEdit(this);
        edit->setFont(FontManager::instance().font(fontRoleStateAction));
        edit->setMinimumHeight(edit->fontMetrics().lineSpacing() * TEXT_BOX_LINES);
        // highlighted by the editor's own rules (docs/PLUGIN-API.md, Parameters)
        bool markup = false;
        CodeMarkup m = CodeStyle::findMarkup(p->language, &markup);
        if (markup && m == codeMarkupMarkdown) {
            new CodeHighlighter(edit->document(), codeRoleMarkup, nullptr, codeMarkupMarkdown);
        } else if (const CodeLanguage* language = CodeStyle::instance().find(p->language)) {
            new CodeHighlighter(edit->document(), codeRoleFormalComment, language);
        }
        edit->setPlainText(value);
        connect(edit, &QPlainTextEdit::textChanged, this, &PluginParameterDialog::updateAccept);
        editor = field = edit;
        break;
    }
    case PluginParameter::typeInt: {
        QSpinBox* spin = new QSpinBox(this);
        spin->setRange(p->hasMin ? int(p->min) : INT_MIN, p->hasMax ? int(p->max) : INT_MAX);
        spin->setValue(value.toInt());
        editor = field = spin;
        break;
    }
    case PluginParameter::typeDouble: {
        QDoubleSpinBox* spin = new QDoubleSpinBox(this);
        spin->setDecimals(6);
        spin->setRange(p->hasMin ? p->min : -1e12, p->hasMax ? p->max : 1e12);
        spin->setValue(value.toDouble());
        editor = field = spin;
        break;
    }
    case PluginParameter::typeBool: {
        QCheckBox* check = new QCheckBox(this);
        check->setChecked(value == "true");
        editor = field = check;
        break;
    }
    case PluginParameter::typeEnum: {
        QComboBox* combo = new QComboBox(this);
        combo->addItems(p->choices);
        combo->setCurrentText(value);
        editor = field = combo;
        break;
    }
    case PluginParameter::typeFile:
    case PluginParameter::typeDir: {
        QLineEdit* edit = new QLineEdit(value, this);
        QToolButton* browse = new QToolButton(this);
        browse->setText("...");
        field = new QWidget(this);
        QHBoxLayout* row = new QHBoxLayout(field);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(edit);
        row->addWidget(browse);
        connect(browse, &QToolButton::clicked, this, [this, p, edit]() {
            QString path;
            if (p->type == PluginParameter::typeDir) {
                path = QFileDialog::getExistingDirectory(this, p->label, edit->text());
            } else if (p->saveFile) {
                path = QFileDialog::getSaveFileName(this, p->label, edit->text(), p->filter);
            } else {
                path = QFileDialog::getOpenFileName(this, p->label, edit->text(), p->filter);
            }
            if (!path.isEmpty()) edit->setText(path);
        });
        connect(edit, &QLineEdit::textChanged, this, &PluginParameterDialog::updateAccept);
        editor = edit;
        break;
    }
    case PluginParameter::typeSecret: {
        // a reference to the key, never the key (EDIT-PLUG-10)
        QLineEdit* edit = new QLineEdit(value, this);
        edit->setPlaceholderText(tr("env:NAME or file:PATH"));
        edit->setValidator(new QRegularExpressionValidator(QRegularExpression("^(|env:.*|file:.*)$"), edit));
        connect(edit, &QLineEdit::textChanged, this, &PluginParameterDialog::updateAccept);
        editor = field = edit;
        break;
    }
    default: {
        QLineEdit* edit = new QLineEdit(value, this);
        connect(edit, &QLineEdit::textChanged, this, &PluginParameterDialog::updateAccept);
        editor = field = edit;
        break;
    }
    }
    editor->setToolTip(p->description);
    editors.insert(p->id, editor);
    return field;
}

QString PluginParameterDialog::value(const PluginParameter* p) const
{
    QWidget* editor = editors.value(p->id);
    switch (p->type) {
    case PluginParameter::typeText:
        return static_cast<QPlainTextEdit*>(editor)->toPlainText();
    case PluginParameter::typeInt:
        return QString::number(static_cast<QSpinBox*>(editor)->value());
    case PluginParameter::typeDouble:
        return QString::number(static_cast<QDoubleSpinBox*>(editor)->value());
    case PluginParameter::typeBool:
        return static_cast<QCheckBox*>(editor)->isChecked() ? "true" : "false";
    case PluginParameter::typeEnum:
        return static_cast<QComboBox*>(editor)->currentText();
    default:
        return static_cast<QLineEdit*>(editor)->text();
    }
}

PluginValues PluginParameterDialog::values() const
{
    PluginValues result;
    for (const PluginParameter* p: parameters) result.insert(p->id, value(p));
    return result;
}

void PluginParameterDialog::updateAccept()
{
    bool ok = true;
    for (const PluginParameter* p: parameters) {
        QString v = value(p);
        if ((p->required && v.isEmpty()) || !pluginCheckValue(*p, v, nullptr)) {
            ok = false;
            break;
        }
    }
    runButton->setEnabled(ok);
}
