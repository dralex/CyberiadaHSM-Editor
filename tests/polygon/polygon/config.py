# -----------------------------------------------------------------------------
# CyberiadaPro, the Cyberiada State Machine Editor
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

"""The polygon configuration: polygon.toml beside the package, with defaults.
A backend names a Pydantic AI model and where its key lives."""

import os
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

DEFAULT_TIMEOUT = 30.0
DEFAULT_CALLS = 40
DEFAULT_SECONDS = 0
DEFAULT_PROBE_PX = 2
DEFAULT_INK_MIN = 0.5
DEFAULT_TEMPERATURE = 0.7

# the environment variable of a key by the provider prefix of the model id
PROVIDER_KEYS = {"openai": "OPENAI_API_KEY", "anthropic": "ANTHROPIC_API_KEY",
                 "deepseek": "DEEPSEEK_API_KEY", "google-gla": "GEMINI_API_KEY",
                 "mistral": "MISTRAL_API_KEY", "groq": "GROQ_API_KEY",
                 "openrouter": "OPENROUTER_API_KEY"}


@dataclass
class Backend:
    name: str
    model: str                      # provider:model-name
    key_env: str = ""               # the variable the key is read from and exported into
    key_file: str = ""              # the file whose first line is the key
    temperature: float = DEFAULT_TEMPERATURE
    max_calls: int = DEFAULT_CALLS

    @property
    def provider(self):
        return self.model.split(":", 1)[0]

    @property
    def key_variable(self):
        """The variable the provider reads the key from."""
        return self.key_env or PROVIDER_KEYS.get(self.provider, "")


@dataclass
class Config:
    timeout: float = DEFAULT_TIMEOUT
    calls: int = DEFAULT_CALLS
    seconds: int = DEFAULT_SECONDS               # [run] seconds: the wall-clock limit of a session
    probe_px: int = DEFAULT_PROBE_PX
    ink_min: float = DEFAULT_INK_MIN
    text: bool = True                            # show and check the canvas texts
    ignore: list = field(default_factory=list)   # [oracles] ignore: more tags to skip
    strict: bool = True                          # [oracles] strict: reload saved files strictly
    backends: dict = field(default_factory=dict)

    def backend(self, name):
        if name not in self.backends:
            raise KeyError("unknown backend '%s'" % name)
        return self.backends[name]


def api_key(backend):
    """The key of a backend: the environment variable, else the first line
    of the key file; never logged."""
    variable = backend.key_variable
    if variable and os.environ.get(variable):
        return os.environ[variable].strip()
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
    config.calls = int(run.get("calls", config.calls))
    config.seconds = int(run.get("seconds", config.seconds))
    config.text = bool(run.get("text", config.text))
    render = data.get("render", {})
    config.probe_px = int(render.get("probe_px", config.probe_px))
    config.ink_min = float(render.get("ink_min", config.ink_min))
    config.ignore = list(data.get("oracles", {}).get("ignore", []))
    config.strict = bool(data.get("oracles", {}).get("strict", config.strict))
    for name, b in data.get("backend", {}).items():
        config.backends[name] = Backend(
            name=name, model=b["model"], key_env=b.get("key_env", ""),
            key_file=b.get("key_file", ""),
            temperature=float(b.get("temperature", DEFAULT_TEMPERATURE)),
            max_calls=int(b.get("max_calls", config.calls)))
    return config
