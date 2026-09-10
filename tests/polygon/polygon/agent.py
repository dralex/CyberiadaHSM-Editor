# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the agent producer
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

"""The agent as a producer of the session: a conversation with the backend
over the mission, one answer per round, the feedback after each."""

from . import dump as D
from . import expectations as X
from . import prompt as P
from .adapters.base import AdapterError

MAX_FORMAT_RETRIES = 2


class Agent:
    def __init__(self, adapter, catalog, mission, first_message, recorder=None):
        self.adapter = adapter
        self.catalog = catalog
        self.mission = mission
        self.system = P.preamble(catalog)
        self.messages = [{"role": "user", "content": first_message}]
        self.recorder = recorder     # called with (kind, text) for every prompt and answer
        self.pending = None          # the answer of the current round, until feedback
        self.calls = 0

    def record(self, kind, text):
        if self.recorder:
            self.recorder(kind, text)

    def ask(self):
        self.record("prompt", self.messages[-1]["content"])
        answer = self.adapter.complete(self.system, self.messages)
        self.calls += 1
        self.record("answer", answer)
        self.messages.append({"role": "assistant", "content": answer})
        return answer

    def next(self, dump, exclude=()):
        """(lines, verb, kind, expectations, plan) of the next round."""
        for _ in range(MAX_FORMAT_RETRIES + 1):
            answer = self.ask()
            parsed = P.parse_answer(answer)
            if parsed is not None:
                plan, script, facts = parsed
                lines = [l for l in script.splitlines() if l.strip() and not l.strip().startswith("#")]
                if lines:
                    verb = lines[0].split()[0]
                    self.pending = (plan, script, facts)
                    return lines, verb, "agent", facts, plan
            self.messages.append({"role": "user", "content":
                                  "Your answer had no script. Answer with the three sections "
                                  "== plan, == script and == expectations, the script holding "
                                  "at least one command."})
        return None

    def rejected(self, record, message):
        """The session retries the round: tell the agent what was refused."""
        self.messages.append({"role": "user", "content": P.feedback(
            record.number, False, record.script_error, [], [], None)})

    def feedback(self, record, result):
        failures = []
        if result.dump is not None and record.expectations:
            failures = X.evaluate(record.expectations, result.dump)
        self.messages.append({"role": "user", "content": P.feedback(
            record.number, record.accepted, record.script_error, record.findings, failures, result.dump)})
