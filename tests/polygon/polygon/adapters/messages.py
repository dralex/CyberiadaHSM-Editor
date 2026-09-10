# -----------------------------------------------------------------------------
# The Cyberiada State Machine Editor
# -----------------------------------------------------------------------------
#
# The test polygon: the messages adapter
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

"""The messages protocol: POST <base_url>/messages with the key header and
the protocol version header; the system text goes in its own field."""

import base64

from .base import Adapter, AdapterError

MAX_TOKENS = 8192
PROTOCOL_VERSION = "2023-06-01"


class Messages(Adapter):
    kind = "messages"

    def request(self, system, messages, images):
        payload = [{"role": m["role"], "content": m["content"]} for m in messages]
        if images and self.supports_images():
            last = payload[-1]
            content = []
            for png in images:
                content.append({"type": "image", "source": {
                    "type": "base64", "media_type": "image/png",
                    "data": base64.b64encode(png).decode()}})
            content.append({"type": "text", "text": last["content"]})
            last["content"] = content
        body = {"model": self.backend.model, "max_tokens": MAX_TOKENS, "system": system,
                "messages": payload, "temperature": self.backend.temperature}
        headers = {"x-api-key": self.key, "anthropic-version": PROTOCOL_VERSION}
        reply = self.post(self.backend.base_url.rstrip("/") + "/messages", headers, body)
        try:
            self.usage.append(reply.get("usage", {}))
            return "".join(block.get("text", "") for block in reply["content"] if block.get("type") == "text")
        except (KeyError, TypeError) as e:
            raise AdapterError("malformed reply: %s" % e) from None
