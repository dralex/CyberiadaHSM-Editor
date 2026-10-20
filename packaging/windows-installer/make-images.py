# -----------------------------------------------------------------------------
#  CyberiadaPro, the Cyberiada State Machine Editor
#
#  The pictures of the Windows installer, drawn from the editor logo: run by
#  hand when the logo changes (needs Pillow); the build takes the bitmaps as
#  they are committed
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

"""Draw welcome.bmp and header.bmp of the installer from the editor logo:

    python3 make-images.py

welcome.bmp (164x314) is the picture of the Welcome and Finish pages, header.bmp
(150x57) the header of the other pages; both 24-bit, as NSIS takes them. Either
can be replaced by a picture of the same size.
"""

import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
EDITOR = os.path.dirname(os.path.dirname(HERE))
LOGO = os.path.join(EDITOR, 'images', 'cyberiada.png')
FONT = os.path.join(EDITOR, 'fonts', 'cyberiada-mono-bold.ttf')
BLUE = (44, 90, 160)            # the colour of the logo
PALE = (223, 233, 246)          # the bottom of the welcome background
NAME = 'CyberiadaPro'


def logo(size):
    return Image.open(LOGO).convert('RGBA').resize((size, size), Image.LANCZOS)


def welcome():
    width, height = 164, 314
    image = Image.new('RGB', (width, height))
    draw = ImageDraw.Draw(image)
    # white at the top, as the page beside it, to pale blue at the bottom
    for y in range(height):
        t = y / (height - 1)
        draw.line([(0, y), (width, y)],
                  fill=tuple(round(255 + (p - 255) * t) for p in PALE))
    mark = logo(120)
    image.paste(mark, ((width - 120) // 2, 56), mark)
    font = ImageFont.truetype(FONT, 17)
    left, top, right, bottom = draw.textbbox((0, 0), NAME, font=font)
    draw.text(((width - (right - left)) // 2 - left, 196 - top), NAME, font=font, fill=BLUE)
    return image


def header():
    width, height = 150, 57
    image = Image.new('RGB', (width, height), (255, 255, 255))
    mark = logo(49)
    image.paste(mark, (width - 49 - 4, (height - 49) // 2), mark)
    return image


def main():
    welcome().save(os.path.join(HERE, 'welcome.bmp'))
    header().save(os.path.join(HERE, 'header.bmp'))


if __name__ == '__main__':
    main()
