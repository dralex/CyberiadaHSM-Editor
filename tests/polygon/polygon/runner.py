# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the editor runner
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

"""One batch run of the editor: the exit code, the signal, the timeout, the
output streams."""

import signal
import subprocess
from dataclasses import dataclass, field
from pathlib import Path

EXIT_OK = 0
EXIT_USAGE = 1
EXIT_LOAD = 2
EXIT_INTERNAL = 3
EXIT_SCRIPT = 4
EXIT_MISMATCH = 5

# stderr lines of the platform, not of the editor
NOISE = ("QStandardPaths:", "This plugin does not support", "qt.qpa.")


@dataclass
class RunResult:
    command: list
    exit: int = None
    signal: int = None
    timed_out: bool = False
    stdout: str = ""
    stderr: str = ""
    files: dict = field(default_factory=dict)

    @property
    def crashed(self):
        return self.timed_out or self.signal is not None or self.exit == EXIT_INTERNAL

    @property
    def signal_name(self):
        if self.signal is None:
            return ""
        try:
            return signal.Signals(self.signal).name
        except ValueError:
            return "SIG%d" % self.signal

    def messages(self):
        """The stderr lines of the editor itself."""
        return [line for line in self.stderr.splitlines()
                if line.strip() and not line.startswith(NOISE)]

    def script_error(self):
        """(line number, message) of a script error, or None."""
        for line in self.messages():
            if line.startswith("line "):
                number, _, message = line[5:].partition(":")
                try:
                    return int(number), message.strip()
                except ValueError:
                    return None
        return None


def run(env, document, script=None, dump=False, stack=False, export=None,
        save=None, timeout=30.0, workdir=None, extra=(), text=False, dump_text=False):
    """Run the editor in batch mode on the document; the outputs go to the
    workdir. A script is a path to a script file. text shows the canvas texts
    (the polygon's full-functional mode); dump_text adds the == text section."""
    command = [str(env.binary), "--batch", "--text" if text else "--no-text"]
    files = {}
    if script is not None:
        command += ["--script", str(script)]
    if dump:
        command.append("--dump")
    if stack:
        command.append("--dump-stack")
    if dump_text:
        command.append("--dump-text")
    if export is not None:
        command += ["--export", str(export)]
        files["export"] = str(export)
    if save is not None:
        command += ["--save", str(save)]
        files["save"] = str(save)
    command += list(extra)
    command.append(str(document))
    result = RunResult(command=command, files=files)
    try:
        # the editor may echo what a script wrote; never let a stray byte
        # kill the run
        completed = subprocess.run(command, cwd=workdir, env=env.environ,
                                   capture_output=True, text=True,
                                   encoding="utf-8", errors="replace",
                                   timeout=timeout)
    except subprocess.TimeoutExpired as e:
        result.timed_out = True
        result.stdout = (e.stdout or b"").decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
        result.stderr = (e.stderr or b"").decode(errors="replace") if isinstance(e.stderr, bytes) else (e.stderr or "")
        return result
    result.stdout = completed.stdout
    result.stderr = completed.stderr
    if completed.returncode < 0:
        result.signal = -completed.returncode
    else:
        result.exit = completed.returncode
    return result


def write_script(path, text):
    path = Path(path)
    path.write_text(text if text.endswith("\n") or not text else text + "\n")
    return path
