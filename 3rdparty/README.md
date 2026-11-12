# The Vendored Libraries

The Markdown rendering (`docs/MARKDOWN.md`) uses two libraries kept here as their authors
published them; no file is changed, so an update replaces the files from the same places.

| Directory | Upstream | Version | Licence |
|---|---|---|---|
| `md4c` | https://github.com/mity/md4c | `release-0.5.2` | MIT (`md4c/LICENSE.md`) |
| `jkqtmathtext` | https://github.com/jkriege2/JKQtPlotter | `v5.0.0-beta2` | LGPL-2.1+ (`jkqtmathtext/LICENSE`) |
| `jkqtmathtext/jkqtmathtext/resources/xits` | the XITS fonts shipped with JKQtPlotter | — | OFL-1.1 (`xits/OFL.txt`) |

* `md4c`: `src/md4c.c`, `src/md4c.h` and `LICENSE.md`.
* `jkqtmathtext`: `lib/jkqtmathtext` without `jkqtmathtextlabel.*` and `private/`, and of
  `lib/jkqtcommon` the files it includes — `jkqtpcsstools`, `jkqtpdrawingtools`,
  `jkqtpenhancedpainter`, `jkqtpstringtools` and the headers they need; the six XITS `.otf`
  files with `xits.qrc`; the repository `LICENSE`.

`CMakeLists.txt` builds them as the static libraries `md4c` and `CyberiadaMathText`.
