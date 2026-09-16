# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the render check
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://www.gnu.org/licenses/
#
# -----------------------------------------------------------------------------

"""The reference-free check of an exported image against the scene dump: the
image must carry ink where the dump places every element (see docs/POLYGON.md)."""

import struct
import zlib
from dataclasses import dataclass

from . import dump as D

# the scene rect the export renders is the visible items rect inflated by this
# margin - the editor now exports at the diagram size with only a 1px pad (no
# scene border margin), see EDIT-IO-6
SCENE_MARGIN = 1
# the rounded corners of a state are left out of the border path
CORNER_SKIP = 14
# a channel below this value is ink on the white background
INK_LEVEL = 250
# the frame derived from the dump may differ from the image by this many pixels
FRAME_TOLERANCE = 2

RECT_KINDS = D.STATE_KINDS + D.COMMENT_KINDS + (D.KIND_SM,)


class PngError(Exception):
    pass


@dataclass
class Image:
    width: int
    height: int
    channels: int
    rows: list   # one bytes object per row

    def pixel(self, x, y):
        row = self.rows[y]
        i = x * self.channels
        return tuple(row[i:i + 3]) if self.channels >= 3 else (row[i],) * 3

    def inside(self, x, y):
        return 0 <= x < self.width and 0 <= y < self.height

    def ink(self, x, y):
        return self.inside(x, y) and min(self.pixel(x, y)) < INK_LEVEL


def _unfilter(data, width, height, channels):
    stride = width * channels
    rows = []
    previous = bytearray(stride)
    pos = 0
    for _ in range(height):
        filter_type = data[pos]
        pos += 1
        raw = bytearray(data[pos:pos + stride])
        pos += stride
        bpp = channels
        if filter_type == 1:
            for i in range(bpp, stride):
                raw[i] = (raw[i] + raw[i - bpp]) & 0xFF
        elif filter_type == 2:
            for i in range(stride):
                raw[i] = (raw[i] + previous[i]) & 0xFF
        elif filter_type == 3:
            for i in range(stride):
                left = raw[i - bpp] if i >= bpp else 0
                raw[i] = (raw[i] + ((left + previous[i]) >> 1)) & 0xFF
        elif filter_type == 4:
            for i in range(stride):
                a = raw[i - bpp] if i >= bpp else 0
                b = previous[i]
                c = previous[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                if pa <= pb and pa <= pc:
                    predictor = a
                elif pb <= pc:
                    predictor = b
                else:
                    predictor = c
                raw[i] = (raw[i] + predictor) & 0xFF
        elif filter_type != 0:
            raise PngError("unknown filter %d" % filter_type)
        rows.append(bytes(raw))
        previous = raw
    return rows


def decode_png(path):
    """An 8-bit greyscale, RGB or RGBA non-interlaced png, as Qt writes it."""
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise PngError("not a png file")
    pos = 8
    width = height = channels = None
    idat = []
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or interlace != 0:
                raise PngError("unsupported png: depth %d, interlace %d" % (depth, interlace))
            channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(color)
            if channels is None:
                raise PngError("unsupported colour type %d" % color)
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
    if width is None:
        raise PngError("no header")
    raw = zlib.decompress(b"".join(idat))
    return Image(width, height, channels, _unfilter(raw, width, height, channels))


def scene_frame(items):
    """(left, top, width, height) of the scene rect the export renders,
    derived from the dumped items: their union inflated by the margin."""
    rects = [item.abs_rect for item in items if item.rect[2] > 0 and item.rect[3] > 0]
    if not rects:
        return None
    left = min(r[0] for r in rects) - SCENE_MARGIN
    top = min(r[1] for r in rects) - SCENE_MARGIN
    right = max(r[0] + r[2] for r in rects) + SCENE_MARGIN
    bottom = max(r[1] + r[3] for r in rects) + SCENE_MARGIN
    return (left, top, right - left, bottom - top)


def border_path(rect, skip=CORNER_SKIP):
    """The sample points along the four edges of a rect, corners left out."""
    x, y, w, h = rect
    points = []
    for i in range(int(skip), int(w - skip) + 1):
        points.append((x + i, y))
        points.append((x + i, y + h))
    for i in range(int(skip), int(h - skip) + 1):
        points.append((x, y + i))
        points.append((x + w, y + i))
    return points


def diamond_path(rect):
    x, y, w, h = rect
    cx, cy = x + w / 2, y + h / 2
    corners = [(cx, y), (x + w, cy), (cx, y + h), (x, cy)]
    points = []
    for (ax, ay), (bx, by) in zip(corners, corners[1:] + corners[:1]):
        n = int(max(abs(bx - ax), abs(by - ay)))
        for i in range(n + 1):
            points.append((ax + (bx - ax) * i / n, ay + (by - ay) * i / n))
    return points


def sample_path(item):
    """The points where the item must leave ink, by kind; None when the kind
    is not checked (transitions, elements without geometry)."""
    rect = item.abs_rect
    if rect[2] <= 0 or rect[3] <= 0:
        return None
    if item.kind in RECT_KINDS:
        if rect[2] <= 2 * CORNER_SKIP or rect[3] <= 2 * CORNER_SKIP:
            return None
        return border_path(rect)
    if item.kind == D.KIND_CHOICE:
        return diamond_path(rect)
    if item.kind in (D.KIND_INITIAL, D.KIND_FINAL, D.KIND_TERMINATE):
        x, y, w, h = rect
        return [(x + w / 2, y + h / 2)]
    return None


def ink_ratio(image, frame, points, probe):
    hits = 0
    for sx, sy in points:
        px, py = int(round(sx - frame[0])), int(round(sy - frame[1]))
        found = False
        for dx in range(-probe, probe + 1):
            for dy in range(-probe, probe + 1):
                if image.ink(px + dx, py + dy):
                    found = True
                    break
            if found:
                break
        hits += found
    return hits / len(points) if points else 1.0


@dataclass
class RenderResult:
    frame: tuple = None
    frame_error: str = ""
    skipped: bool = False     # nothing with a size to check
    unpainted: list = None    # [(item, ratio)]
    outside: list = None      # [(child item, parent item)]
    blank_text: list = None   # [text item] a shown text that left no ink


def check(dump, image, probe_px, ink_min, frame=None):
    """The render check of one export against its dump; the frame is the one
    the export reported, or derived from the dump when absent."""
    result = RenderResult(unpainted=[], outside=[])
    items = list(dump.scene_items().values())
    if frame is None:
        frame = scene_frame(items)
    if frame is None:
        result.skipped = True
        return result
    result.frame = frame
    expected = (int(round(frame[2])), int(round(frame[3])))
    if abs(expected[0] - image.width) > FRAME_TOLERANCE or abs(expected[1] - image.height) > FRAME_TOLERANCE:
        result.frame_error = "the dump frames %dx%d, the image is %dx%d" % (
            expected[0], expected[1], image.width, image.height)
        return result
    elements = dump.document.by_id() if dump.document else {}
    for item in items:
        element = elements.get(item.id)
        # a state machine without a stored rect draws no border: its dumped
        # rect is the union of its content
        if item.kind == D.KIND_SM and (element is None or element.geometry is None):
            continue
        points = sample_path(item)
        if points is None:
            continue
        ratio = ink_ratio(image, frame, points, probe_px)
        if ratio < ink_min:
            result.unpainted.append((item, ratio))
        parent = item.parent
        if item.kind in D.STATE_KINDS and parent is not None and parent.kind in D.STATE_KINDS:
            x, y, w, h = item.abs_rect
            px, py, pw, ph = parent.abs_rect
            if not (x >= px and y >= py and x + w <= px + pw and y + h <= py + ph):
                result.outside.append((item, parent))
    return result


def box_has_ink(image, frame, box):
    """Any non-white pixel inside the box (in scene coordinates)."""
    x, y, w, h = box
    x0, y0 = int(round(x - frame[0])), int(round(y - frame[1]))
    for py in range(max(0, y0), min(image.height, y0 + int(round(h)) + 1)):
        for px in range(max(0, x0), min(image.width, x0 + int(round(w)) + 1)):
            if image.ink(px, py):
                return True
    return False


def oracle(round_, script_text, dump):
    """The tier 3 callable of oracles.Round.evaluate: the borders and the
    containment are checked on a text-free render (reproducible); the shown
    texts must leave ink on a text render."""
    from . import oracles
    from . import runner
    import dataclasses
    findings = []
    # borders and containment: a text-free render and its text-free dump
    target = round_.workdir / "render.png"
    border = round_.run(script_text, "render", export=target, dump=True, text=False)
    if border.exit != runner.EXIT_OK or not target.exists():
        return findings
    frame = oracles.export_frame(border)
    try:
        image = decode_png(target)
        ntdump = D.parse_dump(border.stdout)
    except (PngError, zlib.error, D.DumpError) as e:
        return [oracles.Finding(oracles.KIND_RENDER, "render:png:" + oracles.normalize(str(e)), str(e))]
    result = check(ntdump, image, round_.config.probe_px, round_.config.ink_min, frame)
    if result.skipped:
        result = dataclasses.replace(result, unpainted=[], outside=[])
    if result.frame_error:
        findings.append(oracles.Finding(oracles.KIND_REVIEW, "render:frame",
                                        result.frame_error, {"png": str(target)}))
        return findings
    for item, ratio in result.unpainted:
        findings.append(oracles.Finding(
            oracles.KIND_RENDER, "render:unpainted:%s" % item.kind.lower().replace(" ", "-"),
            "%s %s leaves ink on %.0f%% of its border" % (item.kind, item.id, ratio * 100),
            {"png": str(target)}))
    for item, parent in result.outside:
        findings.append(oracles.Finding(
            oracles.KIND_RENDER, "render:outside:%s" % item.kind.lower().replace(" ", "-"),
            "%s %s is drawn outside its parent %s" % (item.kind, item.id, parent.id),
            {"png": str(target)}))
    if round_.text and dump.texts:
        findings += text_ink_check(round_, script_text, dump)
    return findings


def text_ink_check(round_, script_text, dump):
    """Every shown text must leave ink inside its box on a text render."""
    from . import oracles
    from . import runner
    target = round_.workdir / "render-text.png"
    result = round_.run(script_text, "render-text", export=target, text=True)
    if result.exit != runner.EXIT_OK or not target.exists():
        return []
    frame = oracles.export_frame(result)
    if frame is None:
        return []
    try:
        image = decode_png(target)
    except (PngError, zlib.error):
        return []
    findings = []
    for t in dump.texts:
        box = dump.abs_box(t)
        if box is None or box[2] <= 0 or box[3] <= 0:
            continue
        if not box_has_ink(image, frame, box):
            findings.append(oracles.Finding(
                oracles.KIND_RENDER, "render:blank-text:%s" % t.fact_role,
                "the %s text of %s (%r) leaves no ink on the canvas" % (t.fact_role, t.id, t.plain()),
                {"png": str(target)}))
    return findings
