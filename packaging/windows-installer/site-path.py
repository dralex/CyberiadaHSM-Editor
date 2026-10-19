# -----------------------------------------------------------------------------
#  CyberiadaPro, the Cyberiada State Machine Editor
#
#  Put a folder on the sys.path of this Python with a .pth file in its
#  site-packages, or take it off: the installer adds the folder of the C++
#  CyberiadaML binding on Windows 7-8.1, the uninstaller removes it
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

"""Add a folder to the sys.path of this Python, or remove it:

    python site-path.py add DIR
    python site-path.py remove

The .pth file is written in the encoding this Python reads it with.
"""

import os
import sys
import sysconfig

PTH = 'cyberiada-pro.pth'


def main(argv):
    if len(argv) == 2 and argv[0] == 'add':
        folder = os.path.abspath(argv[1])
    elif argv == ['remove']:
        folder = None
    else:
        sys.stderr.write(__doc__)
        return 2
    path = os.path.join(sysconfig.get_paths()['purelib'], PTH)
    if folder:
        with open(path, 'w') as file:
            file.write(folder + '\n')
    elif os.path.exists(path):
        os.remove(path)
    print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
