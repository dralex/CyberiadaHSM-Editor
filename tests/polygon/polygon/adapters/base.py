# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the backend adapter interface
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

"""The LLM backend adapters: complete(system, messages[, images]) -> text.
Raw HTTP through urllib, one protocol per module, no vendor SDK."""

import json
import time
import urllib.error
import urllib.request

RETRIES = 3
BACKOFF = 2.0
TIMEOUT = 300.0


class AdapterError(Exception):
    pass


class BudgetExceeded(AdapterError):
    pass


class Adapter:
    kind = ""

    def __init__(self, backend, key):
        self.backend = backend
        self.key = key
        self.calls = 0
        self.usage = []   # one record per call: the usage object the server returned

    def supports_images(self):
        return bool(self.backend.vision)

    def complete(self, system, messages, images=None):
        """messages: [{'role': 'user'|'assistant', 'content': str}]; images:
        png bytes attached to the last user message when supported."""
        if self.calls >= self.backend.max_calls:
            raise BudgetExceeded("%d calls, the budget of backend %s" % (self.calls, self.backend.name))
        self.calls += 1
        return self.request(system, messages, images or [])

    def request(self, system, messages, images):
        raise NotImplementedError

    def post(self, url, headers, body):
        """A JSON post with retries on the transient failures."""
        data = json.dumps(body).encode()
        last = None
        for attempt in range(RETRIES):
            request = urllib.request.Request(url, data=data, method="POST",
                                             headers=dict(headers, **{"Content-Type": "application/json"}))
            try:
                with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
                    return json.loads(response.read().decode())
            except urllib.error.HTTPError as e:
                text = e.read().decode(errors="replace")[:500]
                last = AdapterError("HTTP %d from %s: %s" % (e.code, url, text))
                if e.code != 429 and e.code < 500:
                    raise last
            except (urllib.error.URLError, TimeoutError, OSError) as e:
                last = AdapterError("cannot reach %s: %s" % (url, e))
            time.sleep(BACKOFF * (attempt + 1))
        raise last


def make(backend, key):
    from . import chat_completions, messages
    kinds = {chat_completions.ChatCompletions.kind: chat_completions.ChatCompletions,
             messages.Messages.kind: messages.Messages}
    if backend.kind not in kinds:
        raise AdapterError("unknown backend kind %r" % backend.kind)
    if not backend.base_url:
        raise AdapterError("backend %s has no base_url" % backend.name)
    return kinds[backend.kind](backend, key)
