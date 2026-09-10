# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the chat completions adapter
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

"""The chat completions protocol: POST <base_url>/chat/completions with a
bearer key; any server speaking it, local ones included."""

import base64

from .base import Adapter, AdapterError


class ChatCompletions(Adapter):
    kind = "chat-completions"

    def request(self, system, messages, images):
        payload = [{"role": "system", "content": system}]
        for m in messages:
            payload.append({"role": m["role"], "content": m["content"]})
        if images and self.supports_images():
            last = payload[-1]
            content = [{"type": "text", "text": last["content"]}]
            for png in images:
                content.append({"type": "image_url", "image_url": {
                    "url": "data:image/png;base64," + base64.b64encode(png).decode()}})
            last["content"] = content
        body = {"model": self.backend.model, "messages": payload,
                "temperature": self.backend.temperature}
        headers = {"Authorization": "Bearer " + self.key}
        reply = self.post(self.backend.base_url.rstrip("/") + "/chat/completions", headers, body)
        try:
            self.usage.append(reply.get("usage", {}))
            return reply["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as e:
            raise AdapterError("malformed reply: %s" % e) from None
