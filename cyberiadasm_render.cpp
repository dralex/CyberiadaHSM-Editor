/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The scene rendering and image comparison for exports and the batch mode
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

#include <QImage>
#include <QPainter>

#include "cyberiadasm_render.h"
#include "cyberiadasm_editor_scene.h"

bool renderScene(CyberiadaSMEditorScene* scene, const QString& path, QString* error)
{
	QRectF scene_rect = scene->sceneRect();
	if (scene_rect.isEmpty()) {
		if (error) *error = "the scene is empty, nothing to export";
		return false;
	}
	// exported images must not show the editing selection
	scene->clearSelection();
	QRect target(QPoint(0, 0), scene_rect.toRect().size());
	QImage image(target.size(), QImage::Format_ARGB32);
	if (image.isNull()) {
		if (error) *error = "cannot allocate the image";
		return false;
	}
	image.fill(Qt::white);
	QPainter painter(&image);
	scene->render(&painter, target, scene_rect);
	painter.end();
	if (!image.save(path)) {
		if (error) *error = "cannot save the image " + path;
		return false;
	}
	return true;
}

int compareImages(const QString& a, const QString& b, int epsilon,
				  double max_diff_fraction, QString* report)
{
	QImage first(a), second(b);
	if (first.isNull() || second.isNull()) {
		if (report) *report = "cannot read " + (first.isNull() ? a : b);
		return -1;
	}
	if (first.size() != second.size()) {
		if (report) {
			*report = QString("size mismatch: %1x%2 vs %3x%4")
				.arg(first.width()).arg(first.height())
				.arg(second.width()).arg(second.height());
		}
		return 1;
	}
	first = first.convertToFormat(QImage::Format_ARGB32);
	second = second.convertToFormat(QImage::Format_ARGB32);
	qint64 differing = 0;
	for (int y = 0; y < first.height(); y++) {
		const QRgb* p1 = reinterpret_cast<const QRgb*>(first.constScanLine(y));
		const QRgb* p2 = reinterpret_cast<const QRgb*>(second.constScanLine(y));
		for (int x = 0; x < first.width(); x++) {
			if (qAbs(qRed(p1[x]) - qRed(p2[x])) > epsilon ||
				qAbs(qGreen(p1[x]) - qGreen(p2[x])) > epsilon ||
				qAbs(qBlue(p1[x]) - qBlue(p2[x])) > epsilon ||
				qAbs(qAlpha(p1[x]) - qAlpha(p2[x])) > epsilon) {
				differing++;
			}
		}
	}
	qint64 total = qint64(first.width()) * first.height();
	double fraction = total > 0 ? double(differing) / double(total) : 0.0;
	if (report) {
		*report = QString("%1x%2: %3 of %4 pixels differ (%5%, epsilon %6, allowed %7%)")
			.arg(first.width()).arg(first.height()).arg(differing).arg(total)
			.arg(fraction * 100.0, 0, 'f', 3).arg(epsilon)
			.arg(max_diff_fraction * 100.0, 0, 'f', 3);
	}
	return fraction <= max_diff_fraction ? 0 : 1;
}
