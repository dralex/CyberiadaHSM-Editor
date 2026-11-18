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

#ifndef INSERT_IMAGE_DIALOG_H
#define INSERT_IMAGE_DIALOG_H

#include <QDialog>
#include <QImage>
#include <QRect>
#include <QWidget>

#include "markdown_tools.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

// the image scaled into the widget with a crop frame of eight handles; the frame is
// dragged by a handle or moved from inside (EDIT-MD-3)
class ImageCropView : public QWidget {
    Q_OBJECT

public:
    explicit ImageCropView(QWidget* parent = nullptr);

    // the crop becomes the whole image
    void  setImage(const QImage& image);
    // in the pixels of the image, kept inside it
    void  setCrop(const QRect& crop);
    QRect crop() const { return m_crop; }
    // an SVG is shown without the frame
    void  setEditable(bool on);
    // where the image is drawn and a widget point in its pixels - for the tests
    QRectF imageArea() const;
    QPoint toImage(const QPointF& point) const;
    QSize sizeHint() const override;

signals:
    void  cropChanged(const QRect& crop);

protected:
    void  paintEvent(QPaintEvent* event) override;
    void  mousePressEvent(QMouseEvent* event) override;
    void  mouseMoveEvent(QMouseEvent* event) override;
    void  mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF toWidget(const QRect& rect) const;
    // the handle under the point: 0..7 clockwise from the top left, 8 inside, -1 none
    int   handleAt(const QPointF& point) const;
    QVector<QPointF> handles() const;

    QImage m_image;
    QRect  m_crop;
    bool   m_editable = true;
    int    m_drag = -1;
    QPoint m_pressPoint;
    QRect  m_pressCrop;
};

// Insert Image (EDIT-MD-2/3): the preview and its edits, the alt text, the folder and the
// name it is saved as, the line it inserts
class InsertImageDialog : public QDialog {
    Q_OBJECT

public:
    // a file, or the pasted image when source is empty; the folder last used
    InsertImageDialog(const QString& source, const QImage& pasted, const QString& baseDir,
                      const QString& folder, QWidget* parent = nullptr);

    // the source, the edits, the target and the alt text
    ImageInsert insert() const;
    // the folder to remember, relative to the text; empty when the image is linked in place
    QString     folder() const;
    QString     insertedLine() const;
    bool        loaded() const { return !m_image.isNull(); }

    // the controls, for the tests
    ImageCropView* cropView() const { return m_view; }
    QSpinBox*      widthBox() const { return m_widthSpin; }
    QLineEdit*     altEdit() const { return m_altEdit; }
    QLineEdit*     folderEdit() const { return m_folderEdit; }
    QLineEdit*     nameEdit() const { return m_nameEdit; }
    QPushButton*   insertButton() const { return m_insertButton; }
    void           rotate(int degrees);
    void           setCrop(const QRect& crop);
    void           reset();

private slots:
    void browse();
    void cropEdited();
    void viewCropChanged(const QRect& crop);
    void widthEdited();
    void nameEdited();
    void refresh();

private:
    bool        load(const QString& source);
    // the image as the edits see it: rotated
    QImage      rotated() const;
    ImageEdits  edits() const;
    void        showCrop(const QRect& crop);
    // the folder and the name proposed for the source and its edits
    void        propose();

    QString        m_source;
    QImage         m_pasted;
    QString        m_baseDir;
    QString        m_folder;
    QImage         m_image;
    bool           m_svg = false;
    int            m_rotation = 0;
    bool           m_nameTouched = false;
    bool           m_inPlace = false;
    bool           m_updating = false;
    QLabel*        m_sourceLabel;
    ImageCropView* m_view;
    QSpinBox*      m_xSpin;
    QSpinBox*      m_ySpin;
    QSpinBox*      m_wSpin;
    QSpinBox*      m_hSpin;
    QSpinBox*      m_widthSpin;
    QPushButton*   m_rotateLeft;
    QPushButton*   m_rotateRight;
    QPushButton*   m_resetButton;
    QLabel*        m_sizeLabel;
    QLineEdit*     m_altEdit;
    QLineEdit*     m_folderEdit;
    QLineEdit*     m_nameEdit;
    QLabel*        m_lineLabel;
    QLabel*        m_noteLabel;
    QPushButton*   m_insertButton;
};

#endif // INSERT_IMAGE_DIALOG_H
