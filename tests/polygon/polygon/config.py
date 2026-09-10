# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the configuration
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

"""The polygon configuration: polygon.toml beside the package, with defaults."""

import tomllib
from dataclasses import dataclass, field
from pathlib import Path

DEFAULT_TIMEOUT = 30.0
DEFAULT_ROUNDS = 8
DEFAULT_CALLS = 40
DEFAULT_PROBE_PX = 2
DEFAULT_INK_MIN = 0.5


@dataclass
class Backend:
    name: str
    kind: str
    model: str
    base_url: str = ""
    key_env: str = ""
    key_file: str = ""
    vision: bool = False
    temperature: float = 0.7
    max_calls: int = DEFAULT_CALLS


@dataclass
class Config:
    timeout: float = DEFAULT_TIMEOUT
    rounds: int = DEFAULT_ROUNDS
    calls: int = DEFAULT_CALLS
    probe_px: int = DEFAULT_PROBE_PX
    ink_min: float = DEFAULT_INK_MIN
    backends: dict = field(default_factory=dict)

    def backend(self, name):
        if name not in self.backends:
            raise KeyError("unknown backend '%s'" % name)
        return self.backends[name]


def api_key(backend):
    """The key of a backend: the environment variable, else the first line
    of the key file; never logged."""
    import os
    if backend.key_env and os.environ.get(backend.key_env):
        return os.environ[backend.key_env].strip()
    if backend.key_file:
        path = Path(os.path.expanduser(backend.key_file))
        if path.exists():
            return path.read_text().strip().splitlines()[0].strip()
    return ""


def load(path=None):
    """Read the toml file (default polygon.toml beside the package); the
    absent file gives the defaults."""
    if path is None:
        path = Path(__file__).resolve().parents[1] / "polygon.toml"
    config = Config()
    if not Path(path).exists():
        return config
    with open(path, "rb") as f:
        data = tomllib.load(f)
    run = data.get("run", {})
    config.timeout = float(run.get("timeout", config.timeout))
    config.rounds = int(run.get("rounds", config.rounds))
    config.calls = int(run.get("calls", config.calls))
    render = data.get("render", {})
    config.probe_px = int(render.get("probe_px", config.probe_px))
    config.ink_min = float(render.get("ink_min", config.ink_min))
    for name, b in data.get("backend", {}).items():
        config.backends[name] = Backend(
            name=name, kind=b["kind"], model=b["model"],
            base_url=b.get("base_url", ""), key_env=b.get("key_env", ""),
            key_file=b.get("key_file", ""),
            vision=bool(b.get("vision", False)),
            temperature=float(b.get("temperature", 0.7)),
            max_calls=int(b.get("max_calls", config.calls)))
    return config
