/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Comment Fragment Dialog
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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextEdit>

#include "fragmentdialog.h"
#include "fontmanager.h"

// the text boxes show a few lines at most; the longer ones scroll
static const int FRAGMENT_BOX_LINES = 4;

FragmentDialog::FragmentDialog(const Cyberiada::Element* _target, Cyberiada::CommentSubjectType type,
                               const QString& fragment, const CodeLanguage* language, QWidget* parent):
    QDialog(parent), target(_target),
    transition(_target && _target->get_type() == Cyberiada::elementTransition)
{
    setWindowTitle(tr("Comment a Fragment"));
    QVBoxLayout* layout = new QVBoxLayout(this);

    QString kind = transition ? tr("transition") :
        Cyberiada::element_is_comment(target) ? tr("comment") :
        (target->get_type() == Cyberiada::elementSimpleState ||
         target->get_type() == Cyberiada::elementCompositeState ||
         target->get_type() == Cyberiada::elementSubmachineState) ? tr("state") : tr("pseudostate");
    QString name = target->has_name() ? QString::fromStdString(target->get_name()) :
        QString::fromStdString(target->get_id());
    layout->addWidget(new QLabel(tr("Target: %1 (%2)").arg(name, kind), this));

    // the boxes in the search order of the subject rules (EDIT-STRUCT-12): the name,
    // then the data; a transition has the label only
    QVector<SubjectFragment::Field> fields;
    if (!transition) fields = SubjectFragment::fields(target, Cyberiada::commentSubjectName);
    fields += SubjectFragment::fields(target, Cyberiada::commentSubjectData);
    QGridLayout* grid = new QGridLayout();
    QVector<int> row;
    int rowKey = -2;
    QString rowLabel;
    auto flush = [&]() {
        if (!row.isEmpty()) addRow(grid, rowLabel, row);
        row.clear();
    };
    for (const SubjectFragment::Field& f : fields) {
        // a row per name, body and action
        int key = f.kind == SubjectFragment::fieldName ? -3 :
            f.kind == SubjectFragment::fieldBody ? -4 : f.action;
        if (key != rowKey) {
            flush();
            rowKey = key;
            if (f.kind == SubjectFragment::fieldName) rowLabel = tr("Name");
            else if (f.kind == SubjectFragment::fieldBody) rowLabel = tr("Body");
            else if (transition) rowLabel = tr("Label");
            else {
                const Cyberiada::Action& a =
                    static_cast<const Cyberiada::State*>(target)->get_actions().at(size_t(f.action));
                rowLabel = a.get_type() == Cyberiada::actionEntry ? QString("entry") :
                    a.get_type() == Cyberiada::actionExit ? QString("exit") : tr("Reaction");
            }
        }
        Box box;
        box.field = f;
        box.edit = makeBox(f, language);
        QString part = f.kind == SubjectFragment::fieldTrigger ? tr("event") :
            f.kind == SubjectFragment::fieldGuard ? tr("guard") :
            f.kind == SubjectFragment::fieldBehaviour ? tr("behaviour") : QString();
        box.edit->setToolTip(part.isEmpty() ? rowLabel : part);
        box.caption = part.isEmpty() ? rowLabel : rowLabel + ", " + part;
        row.append(boxes.size());
        boxes.append(box);
    }
    flush();
    if (boxes.isEmpty()) {
        grid->addWidget(new QLabel(tr("The target has no text to comment."), this), 0, 0);
    }
    layout->addLayout(grid);

    QHBoxLayout* result = new QHBoxLayout();
    result->addWidget(new QLabel(tr("Fragment"), this));
    fragmentEdit = new QPlainTextEdit(this);
    fragmentEdit->setFont(FontManager::instance().font(fontRoleStateAction));
    fragmentEdit->setTabChangesFocus(true);
    fragmentEdit->setFixedHeight(fragmentEdit->fontMetrics().lineSpacing() * 2 +
                                 2 * (fragmentEdit->frameWidth() + int(fragmentEdit->document()->documentMargin())));
    result->addWidget(fragmentEdit, 1);
    result->addWidget(new QLabel(tr("Type"), this));
    typeBox = new QComboBox(this);
    typeBox->addItem(tr("Name"), int(Cyberiada::commentSubjectName));
    typeBox->addItem(tr("Data"), int(Cyberiada::commentSubjectData));
    result->addWidget(typeBox);
    layout->addLayout(result);

    noteLabel = new QLabel(this);
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    okButton = buttons->button(QDialogButtonBox::Ok);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(fragmentEdit, &QPlainTextEdit::textChanged, this, &FragmentDialog::onFragmentEdited);
    connect(typeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FragmentDialog::onFragmentEdited);

    // an element subject starts with the data, when there is any
    if (type == Cyberiada::commentSubjectElement) {
        type = !SubjectFragment::fields(target, Cyberiada::commentSubjectData).isEmpty() ?
            Cyberiada::commentSubjectData : Cyberiada::commentSubjectName;
    }
    selecting = true;
    typeBox->setCurrentIndex(typeBox->findData(int(type)));
    fragmentEdit->setPlainText(fragment);
    selecting = false;
    // the current fragment comes selected where it is underlined
    SubjectFragment::Place place = SubjectFragment::locate(target, type, fragment);
    int b = place.found() ? boxOf(place.field) : -1;
    if (b >= 0) {
        QTextCursor c = boxes[b].edit->textCursor();
        c.setPosition(place.offset);
        c.setPosition(place.offset + fragment.length(), QTextCursor::KeepAnchor);
        boxes[b].edit->setTextCursor(c);
    }
    validate();
    if (b >= 0) boxes[b].edit->setFocus();
    else if (!boxes.isEmpty()) boxes.first().edit->setFocus();
}

QPlainTextEdit* FragmentDialog::makeBox(const SubjectFragment::Field& f, const CodeLanguage* language)
{
    QPlainTextEdit* edit = new QPlainTextEdit(this);
    edit->setReadOnly(true);
    edit->setTabChangesFocus(true);
    edit->setFont(FontManager::instance().font(fontRoleStateAction));
    bool multiline = f.kind == SubjectFragment::fieldBehaviour || f.kind == SubjectFragment::fieldBody;
    edit->setLineWrapMode(multiline ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    // the code is highlighted as on the scene (EDIT-TEXT-7)
    if (f.kind == SubjectFragment::fieldGuard || f.kind == SubjectFragment::fieldBehaviour) {
        new CodeHighlighter(edit->document(), codeRoleBehaviour, language);
    } else if (f.kind == SubjectFragment::fieldBody && target->get_type() == Cyberiada::elementFormalComment) {
        new CodeHighlighter(edit->document(), codeRoleFormalComment, language);
    }
    edit->setPlainText(f.text);
    int lines = multiline ? qBound(1, edit->document()->blockCount() + f.text.length() / 60, FRAGMENT_BOX_LINES) : 1;
    edit->setFixedHeight(edit->fontMetrics().lineSpacing() * lines +
                         2 * (edit->frameWidth() + int(edit->document()->documentMargin())) + 2);
    connect(edit, &QPlainTextEdit::selectionChanged, this, &FragmentDialog::onSelectionChanged);
    return edit;
}

void FragmentDialog::addRow(QGridLayout* grid, const QString& label, const QVector<int>& row)
{
    int r = grid->rowCount();
    grid->addWidget(new QLabel(label, this), r, 0, Qt::AlignTop);
    QHBoxLayout* line = new QHBoxLayout();
    for (int i : row) {
        // a behaviour takes the room the event and the guard leave
        int stretch = boxes[i].field.kind == SubjectFragment::fieldTrigger ||
            boxes[i].field.kind == SubjectFragment::fieldGuard ? 1 : 2;
        line->addWidget(boxes[i].edit, stretch);
    }
    grid->addLayout(line, r, 1);
}

int FragmentDialog::boxOf(const SubjectFragment::Field& f) const
{
    for (int i = 0; i < boxes.size(); i++) {
        if (boxes[i].field.kind == f.kind && boxes[i].field.action == f.action) return i;
    }
    return -1;
}

QPlainTextEdit* FragmentDialog::fieldBox(int i) const
{
    return boxes.at(i).edit;
}

const SubjectFragment::Field& FragmentDialog::field(int i) const
{
    return boxes.at(i).field;
}

Cyberiada::CommentSubjectType FragmentDialog::subjectType() const
{
    return Cyberiada::CommentSubjectType(typeBox->currentData().toInt());
}

QString FragmentDialog::fragment() const
{
    return fragmentEdit->toPlainText();
}

bool FragmentDialog::acceptable() const
{
    return okButton->isEnabled();
}

QString FragmentDialog::note() const
{
    return noteLabel->text();
}

void FragmentDialog::onSelectionChanged()
{
    if (selecting) return;
    QPlainTextEdit* edit = qobject_cast<QPlainTextEdit*>(sender());
    int b = -1;
    for (int i = 0; i < boxes.size(); i++) if (boxes[i].edit == edit) b = i;
    if (b < 0) return;
    QTextCursor c = edit->textCursor();
    // a box losing its selection changes nothing
    if (!c.hasSelection()) return;
    selecting = true;
    // one selection at a time, so the fragment stays in one field
    for (int i = 0; i < boxes.size(); i++) {
        if (i == b) continue;
        QTextCursor other = boxes[i].edit->textCursor();
        if (other.hasSelection()) {
            other.clearSelection();
            boxes[i].edit->setTextCursor(other);
        }
    }
    QString text = c.selectedText();
    text.replace(QChar::ParagraphSeparator, '\n').replace(QChar::LineSeparator, '\n');
    int lead = 0;
    while (lead < text.length() && text[lead].isSpace()) lead++;
    selectedBox = b;
    selectedOffset = c.selectionStart() + lead;
    fragmentEdit->setPlainText(text.trimmed());
    // the name box gives a name fragment, the others data; a transition keeps the kind
    if (!transition) {
        Cyberiada::CommentSubjectType type = boxes[b].field.kind == SubjectFragment::fieldName ?
            Cyberiada::commentSubjectName : Cyberiada::commentSubjectData;
        typeBox->setCurrentIndex(typeBox->findData(int(type)));
    }
    selecting = false;
    validate();
}

void FragmentDialog::onFragmentEdited()
{
    if (selecting) return;
    // a typed fragment no longer comes from the selection
    selectedBox = -1;
    validate();
}

void FragmentDialog::validate()
{
    for (const Box& box : boxes) box.edit->setExtraSelections(QList<QTextEdit::ExtraSelection>());
    marked = -1;
    QString f = fragment();
    SubjectFragment::Place place = SubjectFragment::locate(target, subjectType(), f);
    int b = place.found() ? boxOf(place.field) : -1;
    okButton->setEnabled(b >= 0);
    if (b < 0) {
        noteLabel->setText(f.isEmpty() ? tr("Select the commented text in one field.") :
                           tr("The fragment does not lie in one field of the target."));
        return;
    }
    // the occurrence the link will underline (EDIT-EDGE-21)
    QTextEdit::ExtraSelection mark;
    mark.cursor = boxes[b].edit->textCursor();
    mark.cursor.setPosition(place.offset);
    mark.cursor.setPosition(place.offset + f.length(), QTextCursor::KeepAnchor);
    mark.format.setFontUnderline(true);
    boxes[b].edit->setExtraSelections(QList<QTextEdit::ExtraSelection>() << mark);
    marked = b;
    if (selectedBox >= 0 && (selectedBox != b || selectedOffset != place.offset)) {
        noteLabel->setText(tr("The first occurrence (%1) will be underlined; extend the selection "
                              "to make it unique.").arg(boxes[b].caption));
    } else {
        noteLabel->setText(tr("Underlined in: %1.").arg(boxes[b].caption));
    }
}
