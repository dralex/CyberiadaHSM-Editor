/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The Insert Image dialog of a Markdown text
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

#include "insert_image_dialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QSvgRenderer>
#include <QTransform>
#include <QVBoxLayout>

#include "file_dialog.h"

static const int CROP_HANDLE_SIZE = 8;
static const int CROP_VIEW_MARGIN = 6;
static const int CROP_VIEW_WIDTH = 420;
static const int CROP_VIEW_HEIGHT = 300;
static const int CROP_SHADE_ALPHA = 110;
// an SVG is previewed no larger than this
static const int SVG_PREVIEW_SIZE = 1024;

// ---------------------------------------------------------------------------- the view

ImageCropView::ImageCropView(QWidget* parent):
    QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(CROP_VIEW_WIDTH / 2, CROP_VIEW_HEIGHT / 2);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize ImageCropView::sizeHint() const
{
    return QSize(CROP_VIEW_WIDTH, CROP_VIEW_HEIGHT);
}

void ImageCropView::setImage(const QImage& image)
{
    m_image = image;
    m_crop = image.rect();
    update();
}

void ImageCropView::setCrop(const QRect& crop)
{
    QRect c = crop.normalized().intersected(m_image.rect());
    if (c.isEmpty()) c = m_image.rect();
    if (c == m_crop) return;
    m_crop = c;
    update();
}

void ImageCropView::setEditable(bool on)
{
    m_editable = on;
    update();
}

QRectF ImageCropView::imageArea() const
{
    QRectF area = QRectF(rect()).adjusted(CROP_VIEW_MARGIN, CROP_VIEW_MARGIN,
                                          -CROP_VIEW_MARGIN, -CROP_VIEW_MARGIN);
    if (m_image.isNull()) return area;
    QSizeF size = m_image.size();
    // scaled down to fit, never up
    if (size.width() > area.width() || size.height() > area.height()) {
        size.scale(area.size(), Qt::KeepAspectRatio);
    }
    return QRectF(area.center() - QPointF(size.width() / 2, size.height() / 2), size);
}

QPoint ImageCropView::toImage(const QPointF& point) const
{
    QRectF area = imageArea();
    if (m_image.isNull() || area.isEmpty()) return QPoint();
    int x = qRound((point.x() - area.left()) * m_image.width() / area.width());
    int y = qRound((point.y() - area.top()) * m_image.height() / area.height());
    return QPoint(qBound(0, x, m_image.width()), qBound(0, y, m_image.height()));
}

QRectF ImageCropView::toWidget(const QRect& r) const
{
    QRectF area = imageArea();
    if (m_image.isNull()) return QRectF();
    qreal sx = area.width() / m_image.width();
    qreal sy = area.height() / m_image.height();
    return QRectF(area.left() + r.x() * sx, area.top() + r.y() * sy, r.width() * sx, r.height() * sy);
}

QVector<QPointF> ImageCropView::handles() const
{
    QRectF f = toWidget(m_crop);
    return QVector<QPointF>() << f.topLeft() << QPointF(f.center().x(), f.top()) << f.topRight()
                              << QPointF(f.right(), f.center().y()) << f.bottomRight()
                              << QPointF(f.center().x(), f.bottom()) << f.bottomLeft()
                              << QPointF(f.left(), f.center().y());
}

int ImageCropView::handleAt(const QPointF& point) const
{
    if (!m_editable || m_image.isNull()) return -1;
    QVector<QPointF> h = handles();
    for (int i = 0; i < h.size(); i++) {
        QRectF box(h[i] - QPointF(CROP_HANDLE_SIZE, CROP_HANDLE_SIZE),
                   QSizeF(2 * CROP_HANDLE_SIZE, 2 * CROP_HANDLE_SIZE));
        if (box.contains(point)) return i;
    }
    return toWidget(m_crop).contains(point) ? h.size() : -1;
}

void ImageCropView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().dark());
    if (m_image.isNull()) return;
    QRectF area = imageArea();
    painter.drawImage(area, m_image);
    if (!m_editable) return;
    QRectF frame = toWidget(m_crop);
    QPainterPath outside;
    outside.addRect(area);
    outside.addRect(frame);
    painter.fillPath(outside, QColor(0, 0, 0, CROP_SHADE_ALPHA));
    painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(frame);
    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(Qt::white);
    for (const QPointF& h : handles()) {
        painter.drawRect(QRectF(h - QPointF(CROP_HANDLE_SIZE / 2, CROP_HANDLE_SIZE / 2),
                                QSizeF(CROP_HANDLE_SIZE, CROP_HANDLE_SIZE)));
    }
}

void ImageCropView::mousePressEvent(QMouseEvent* event)
{
    m_drag = handleAt(event->pos());
    m_pressPoint = toImage(event->pos());
    m_pressCrop = m_crop;
}

void ImageCropView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drag < 0) {
        static const Qt::CursorShape shapes[] = {
            Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor,
            Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor,
            Qt::SizeAllCursor };
        int h = handleAt(event->pos());
        if (h < 0) unsetCursor();
        else setCursor(shapes[h]);
        return;
    }
    QPoint p = toImage(event->pos());
    QRect c = m_pressCrop;
    int left = c.x(), top = c.y(), right = c.x() + c.width(), bottom = c.y() + c.height();
    switch (m_drag) {
    case 0: left = p.x(); top = p.y(); break;
    case 1: top = p.y(); break;
    case 2: right = p.x(); top = p.y(); break;
    case 3: right = p.x(); break;
    case 4: right = p.x(); bottom = p.y(); break;
    case 5: bottom = p.y(); break;
    case 6: left = p.x(); bottom = p.y(); break;
    case 7: left = p.x(); break;
    default: {
        // moved from inside, kept in the image
        QPoint d = p - m_pressPoint;
        int dx = qBound(-c.x(), d.x(), m_image.width() - c.x() - c.width());
        int dy = qBound(-c.y(), d.y(), m_image.height() - c.y() - c.height());
        left += dx; right += dx; top += dy; bottom += dy;
    }
    }
    QRect next(qMin(left, right), qMin(top, bottom), qMax(1, qAbs(right - left)), qMax(1, qAbs(bottom - top)));
    next = next.intersected(m_image.rect());
    if (next.isEmpty() || next == m_crop) return;
    m_crop = next;
    update();
    emit cropChanged(m_crop);
}

void ImageCropView::mouseReleaseEvent(QMouseEvent*)
{
    m_drag = -1;
}

// ---------------------------------------------------------------------------- the dialog

InsertImageDialog::InsertImageDialog(const QString& source, const QImage& pasted, const QString& baseDir,
                                     const QString& folder, QWidget* parent):
    QDialog(parent), m_pasted(pasted), m_baseDir(baseDir), m_folder(folder)
{
    setWindowTitle(tr("Insert Image"));
    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* sourceRow = new QHBoxLayout();
    sourceRow->addWidget(new QLabel(tr("Source:"), this));
    m_sourceLabel = new QLabel(this);
    m_sourceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sourceLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    sourceRow->addWidget(m_sourceLabel, 1);
    QPushButton* browseButton = new QPushButton(tr("Browse..."), this);
    sourceRow->addWidget(browseButton);
    layout->addLayout(sourceRow);

    QHBoxLayout* middle = new QHBoxLayout();
    m_view = new ImageCropView(this);
    middle->addWidget(m_view, 1);
    QGridLayout* side = new QGridLayout();
    auto spin = [this]() {
        QSpinBox* box = new QSpinBox(this);
        box->setRange(0, 0);
        connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this, &InsertImageDialog::cropEdited);
        return box;
    };
    m_xSpin = spin();
    m_ySpin = spin();
    m_wSpin = spin();
    m_hSpin = spin();
    side->addWidget(new QLabel(tr("Crop"), this), 0, 0, 1, 4);
    side->addWidget(new QLabel(tr("x"), this), 1, 0);
    side->addWidget(m_xSpin, 1, 1);
    side->addWidget(new QLabel(tr("y"), this), 1, 2);
    side->addWidget(m_ySpin, 1, 3);
    side->addWidget(new QLabel(tr("w"), this), 2, 0);
    side->addWidget(m_wSpin, 2, 1);
    side->addWidget(new QLabel(tr("h"), this), 2, 2);
    side->addWidget(m_hSpin, 2, 3);
    m_rotateLeft = new QPushButton(tr("Rotate Left"), this);
    m_rotateRight = new QPushButton(tr("Rotate Right"), this);
    side->addWidget(m_rotateLeft, 3, 0, 1, 2);
    side->addWidget(m_rotateRight, 3, 2, 1, 2);
    side->addWidget(new QLabel(tr("Width at most"), this), 4, 0, 1, 2);
    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setSuffix(tr(" px"));
    side->addWidget(m_widthSpin, 4, 2, 1, 2);
    m_resetButton = new QPushButton(tr("Reset"), this);
    side->addWidget(m_resetButton, 5, 0, 1, 2);
    m_sizeLabel = new QLabel(this);
    side->addWidget(m_sizeLabel, 6, 0, 1, 4);
    side->setRowStretch(7, 1);
    middle->addLayout(side);
    layout->addLayout(middle, 1);

    QFormLayout* form = new QFormLayout();
    m_altEdit = new QLineEdit(this);
    form->addRow(tr("Alt text:"), m_altEdit);
    QHBoxLayout* saveRow = new QHBoxLayout();
    m_folderEdit = new QLineEdit(this);
    m_nameEdit = new QLineEdit(this);
    saveRow->addWidget(m_folderEdit, 1);
    saveRow->addWidget(new QLabel("/", this));
    saveRow->addWidget(m_nameEdit, 2);
    form->addRow(tr("Save as:"), saveRow);
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

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browseButton, &QPushButton::clicked, this, &InsertImageDialog::browse);
    connect(m_rotateLeft, &QPushButton::clicked, this, [this]() { rotate(-IMAGE_QUARTER_TURN); });
    connect(m_rotateRight, &QPushButton::clicked, this, [this]() { rotate(IMAGE_QUARTER_TURN); });
    connect(m_resetButton, &QPushButton::clicked, this, &InsertImageDialog::reset);
    connect(m_view, &ImageCropView::cropChanged, this, &InsertImageDialog::viewCropChanged);
    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &InsertImageDialog::widthEdited);
    connect(m_altEdit, &QLineEdit::textChanged, this, &InsertImageDialog::refresh);
    connect(m_folderEdit, &QLineEdit::textEdited, this, &InsertImageDialog::nameEdited);
    connect(m_nameEdit, &QLineEdit::textEdited, this, &InsertImageDialog::nameEdited);

    load(source);
}

bool InsertImageDialog::load(const QString& source)
{
    m_source = source;
    m_svg = !source.isEmpty() && isSvgFile(source);
    if (source.isEmpty()) {
        m_image = m_pasted;
        m_sourceLabel->setText(tr("the clipboard"));
    } else if (m_svg) {
        QSvgRenderer renderer(source);
        QSize size = renderer.defaultSize();
        if (size.width() > SVG_PREVIEW_SIZE || size.height() > SVG_PREVIEW_SIZE) {
            size.scale(SVG_PREVIEW_SIZE, SVG_PREVIEW_SIZE, Qt::KeepAspectRatio);
        }
        m_image = QImage();
        if (renderer.isValid() && !size.isEmpty()) {
            m_image = QImage(size, QImage::Format_ARGB32_Premultiplied);
            m_image.fill(Qt::transparent);
            QPainter painter(&m_image);
            renderer.render(&painter);
        }
        m_sourceLabel->setText(QDir::toNativeSeparators(source));
        m_sourceLabel->setToolTip(QDir::toNativeSeparators(source));
    } else {
        QImageReader reader(source);
        reader.setAutoTransform(true);
        m_image = reader.read();
        m_sourceLabel->setText(QDir::toNativeSeparators(source));
        m_sourceLabel->setToolTip(QDir::toNativeSeparators(source));
    }
    m_rotation = 0;
    m_nameTouched = false;
    m_view->setImage(m_image);
    m_view->setEditable(!m_svg);
    const QList<QWidget*> editors = QList<QWidget*>() << m_xSpin << m_ySpin << m_wSpin << m_hSpin
        << m_widthSpin << m_rotateLeft << m_rotateRight << m_resetButton;
    for (QWidget* w : editors) w->setEnabled(!m_svg && !m_image.isNull());
    m_altEdit->setText(source.isEmpty() ? QString() : QFileInfo(source).completeBaseName());
    showCrop(m_view->crop());
    {
        QSignalBlocker block(m_widthSpin);
        m_widthSpin->setRange(1, qMax(1, m_image.width()));
        m_widthSpin->setValue(m_image.width());
    }
    refresh();
    return !m_image.isNull();
}

QImage InsertImageDialog::rotated() const
{
    return m_rotation ? m_image.transformed(QTransform().rotate(m_rotation)) : m_image;
}

ImageEdits InsertImageDialog::edits() const
{
    ImageEdits edits;
    if (m_svg || m_image.isNull()) return edits;
    edits.rotation = m_rotation;
    QRect crop = m_view->crop();
    QSize full = m_rotation % (2 * IMAGE_QUARTER_TURN) ? m_image.size().transposed() : m_image.size();
    if (crop != QRect(QPoint(0, 0), full)) edits.crop = crop;
    if (m_widthSpin->value() < crop.width()) edits.maxWidth = m_widthSpin->value();
    return edits;
}

ImageInsert InsertImageDialog::insert() const
{
    ImageInsert insert;
    insert.source = m_source;
    if (m_source.isEmpty()) insert.pasted = m_pasted;
    insert.edits = edits();
    insert.baseDir = m_baseDir;
    insert.alt = m_altEdit->text();
    if (m_inPlace) {
        insert.target = QDir::cleanPath(QFileInfo(m_source).absoluteFilePath());
    } else {
        QDir folder(QDir(m_baseDir).filePath(m_folderEdit->text().trimmed()));
        insert.target = QDir::cleanPath(folder.filePath(m_nameEdit->text().trimmed()));
    }
    return insert;
}

QString InsertImageDialog::folder() const
{
    // the folder of a copy is remembered, not the one of an image of the directory
    bool copy = m_source.isEmpty() || !isInsideDirectory(m_source, m_baseDir);
    return copy ? m_folderEdit->text().trimmed() : QString();
}

QString InsertImageDialog::insertedLine() const
{
    ImageInsert i = insert();
    return markdownImageLine(i.alt, i.target, m_baseDir);
}

void InsertImageDialog::propose()
{
    ImageInsert probe = insert();
    QString target = proposeImageTarget(probe, m_folder);
    m_inPlace = !m_source.isEmpty() &&
        QDir::cleanPath(QFileInfo(m_source).absoluteFilePath()) == target;
    QString dir = QDir(m_baseDir).relativeFilePath(QFileInfo(target).absolutePath());
    if (dir.isEmpty()) dir = ".";
    m_folderEdit->setEnabled(!m_inPlace);
    m_nameEdit->setEnabled(!m_inPlace);
    if (m_inPlace || !m_nameTouched) {
        m_folderEdit->setText(dir);
        m_nameEdit->setText(QFileInfo(target).fileName());
    }
}

void InsertImageDialog::refresh()
{
    if (m_updating) return;
    propose();
    QRect crop = m_view->crop();
    int w = qMin(crop.width(), m_widthSpin->value());
    int h = crop.width() ? qRound(qreal(crop.height()) * w / crop.width()) : 0;
    m_sizeLabel->setText(m_image.isNull() ? tr("The image cannot be read") : tr("Result: %1 × %2 px").arg(w).arg(h));
    m_lineLabel->setText(insertedLine());
    QString note;
    if (m_svg) note = tr("An SVG image is copied as it is, without the edits.");
    else if (m_inPlace) note = tr("The image lies in the folder of the text: it is linked where it is.");
    else if (m_source.isEmpty()) note = tr("The pasted image is saved as a new file.");
    m_noteLabel->setText(note);
    m_insertButton->setEnabled(loaded() && (m_inPlace || !m_nameEdit->text().trimmed().isEmpty()));
}

void InsertImageDialog::showCrop(const QRect& crop)
{
    m_updating = true;
    QSize full = m_rotation % (2 * IMAGE_QUARTER_TURN) ? m_image.size().transposed() : m_image.size();
    m_xSpin->setRange(0, qMax(0, full.width() - 1));
    m_ySpin->setRange(0, qMax(0, full.height() - 1));
    m_wSpin->setRange(1, qMax(1, full.width()));
    m_hSpin->setRange(1, qMax(1, full.height()));
    m_xSpin->setValue(crop.x());
    m_ySpin->setValue(crop.y());
    m_wSpin->setValue(crop.width());
    m_hSpin->setValue(crop.height());
    // a width at the whole crop is no limit and follows it
    bool limited = m_widthSpin->value() < m_widthSpin->maximum();
    m_widthSpin->setRange(1, qMax(1, crop.width()));
    if (!limited) m_widthSpin->setValue(crop.width());
    m_updating = false;
}

void InsertImageDialog::rotate(int degrees)
{
    if (m_svg || m_image.isNull()) return;
    m_rotation = ((m_rotation + degrees) % IMAGE_FULL_TURN + IMAGE_FULL_TURN) % IMAGE_FULL_TURN;
    QImage image = rotated();
    m_view->setImage(image);
    {
        QSignalBlocker block(m_widthSpin);
        m_widthSpin->setRange(1, qMax(1, image.width()));
        m_widthSpin->setValue(image.width());
    }
    showCrop(m_view->crop());
    refresh();
}

void InsertImageDialog::setCrop(const QRect& crop)
{
    m_view->setCrop(crop);
    showCrop(m_view->crop());
    refresh();
}

void InsertImageDialog::reset()
{
    m_rotation = 0;
    m_view->setImage(m_image);
    {
        QSignalBlocker block(m_widthSpin);
        m_widthSpin->setRange(1, qMax(1, m_image.width()));
        m_widthSpin->setValue(m_image.width());
    }
    showCrop(m_view->crop());
    refresh();
}

void InsertImageDialog::browse()
{
    CyberiadaFileDialog dlg(this, tr("Insert Image"), imageFileFilter());
    dlg.setFileMode(QFileDialog::ExistingFile);
    if (dlg.exec() != QDialog::Accepted) return;
    QString path = dlg.selectedFile();
    if (!path.isEmpty()) load(path);
}

void InsertImageDialog::cropEdited()
{
    if (m_updating) return;
    m_view->setCrop(QRect(m_xSpin->value(), m_ySpin->value(), m_wSpin->value(), m_hSpin->value()));
    showCrop(m_view->crop());
    refresh();
}

void InsertImageDialog::viewCropChanged(const QRect& crop)
{
    showCrop(crop);
    refresh();
}

void InsertImageDialog::widthEdited()
{
    if (m_updating) return;
    refresh();
}

void InsertImageDialog::nameEdited()
{
    m_nameTouched = true;
    refresh();
}
