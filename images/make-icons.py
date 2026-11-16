# -----------------------------------------------------------------------------
#  CyberiadaPro, the Cyberiada State Machine Editor
#
#  The icon sizes of the editor, rendered from the logo: run by hand when the
#  logo changes (needs librsvg and cairo); the build takes the files as they
#  are committed
#
#  Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or (at your
#  option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
#  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
#  License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with this program. If not, see https://www.gnu.org/licenses/
#
#  -----------------------------------------------------------------------------

"""Render the small sizes of the editor icon from cyberiada.svg:

    python3 make-icons.py

cyberiada-<n>.png are the sizes the systems ask for (the title bar, the
taskbar, the file lists) below the 256 px cyberiada.png; cyberiada.ico holds
them all for Windows (the executable, the installer, the .graphml files). A
system shrinking the 256 px picture itself loses the thin lines of the logo,
so each size is rendered from the drawing, its lines at least MIN_LINE wide.
"""

import ctypes
import os
import re
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, 'cyberiada.svg')
LARGEST = os.path.join(HERE, 'cyberiada.png')
ICON = os.path.join(HERE, 'cyberiada.ico')
SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128)
MIN_LINE = 1.2   # pixels
CAIRO_FORMAT_ARGB32 = 0


class Viewport(ctypes.Structure):
    _fields_ = [('x', ctypes.c_double), ('y', ctypes.c_double),
                ('width', ctypes.c_double), ('height', ctypes.c_double)]


def libraries():
    rsvg = ctypes.CDLL('librsvg-2.so.2')
    cairo = ctypes.CDLL('libcairo.so.2')
    rsvg.rsvg_handle_new_from_data.restype = ctypes.c_void_p
    rsvg.rsvg_handle_new_from_data.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_void_p]
    rsvg.rsvg_handle_render_document.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                                 ctypes.POINTER(Viewport), ctypes.c_void_p]
    rsvg.g_object_unref.argtypes = [ctypes.c_void_p]
    cairo.cairo_image_surface_create.restype = ctypes.c_void_p
    cairo.cairo_create.restype = ctypes.c_void_p
    cairo.cairo_create.argtypes = [ctypes.c_void_p]
    cairo.cairo_surface_write_to_png.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    cairo.cairo_destroy.argtypes = [ctypes.c_void_p]
    cairo.cairo_surface_destroy.argtypes = [ctypes.c_void_p]
    return rsvg, cairo


def widened(svg, size):
    """The drawing whose strokes are at least MIN_LINE pixels at this size."""
    extent = float(re.search(r'viewBox="[^"]*?([0-9.]+)"', svg).group(1))
    least = MIN_LINE * extent / size
    return re.sub(r'(stroke-width:)([0-9.]+)',
                  lambda m: '{}{:.4f}'.format(m.group(1), max(float(m.group(2)), least)), svg)


def render(rsvg, cairo, svg, size, path):
    data = svg.encode('utf-8')
    handle = rsvg.rsvg_handle_new_from_data(data, len(data), None)
    if not handle:
        raise SystemExit('error: librsvg cannot read ' + SOURCE)
    surface = cairo.cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size)
    context = cairo.cairo_create(surface)
    rsvg.rsvg_handle_render_document(handle, context, ctypes.byref(Viewport(0, 0, size, size)), None)
    cairo.cairo_surface_write_to_png(surface, path.encode())
    cairo.cairo_destroy(context)
    cairo.cairo_surface_destroy(surface)
    rsvg.g_object_unref(handle)


def write_icon(images, path):
    """An ICO of PNG images (Windows Vista and newer): [(size, png bytes)]."""
    header = struct.pack('<HHH', 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries, blobs = b'', b''
    for size, png in images:
        # a width or height of 256 is written as 0
        entries += struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(png), offset)
        blobs += png
        offset += len(png)
    with open(path, 'wb') as file:
        file.write(header + entries + blobs)


def main():
    rsvg, cairo = libraries()
    with open(SOURCE, encoding='utf-8') as file:
        svg = file.read()
    images = []
    for size in SIZES:
        path = os.path.join(HERE, 'cyberiada-{}.png'.format(size))
        render(rsvg, cairo, widened(svg, size), size, path)
        with open(path, 'rb') as file:
            images.append((size, file.read()))
    with open(LARGEST, 'rb') as file:
        images.append((256, file.read()))
    write_icon(images, ICON)
    print('wrote', ', '.join('cyberiada-{}.png'.format(s) for s in SIZES), 'and cyberiada.ico')


if __name__ == '__main__':
    main()
