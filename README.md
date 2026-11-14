# CyberiadaPro — the Cyberiada Hierarchical State Machines Editor

![The Cyberiada Logo](images/logo.png)

The visual editor program for Cyberiada HSM graphs based on the Qt Framework.

The code is distributed under the GNU Public License (version 3), the documentation -- under
the GNU Free Documentation License (version 1.3).

## The Editor

The editor is able to create HSM diagrams and edit Cyberiada GraphML files.
It suports multiple HSM formats and has many drawing tools - HSM diagram
elements (states, pseudostate, transitions, etc.), view management (zoom,
pan), copy/paste, undo/redo, etc. Several documents are open at once in tabs,
with the clipboard shared between them. The code in the actions, guards and formal
comments is highlighted by the `platformLanguage` of the document (C, C++, C#,
Python; the rules are in `syntax/`), chosen in Document > Language.
The informal comments in Markdown are highlighted as well (Document > Markup).
Each tab has a mode (Document > Mode, see `docs/MODES.md`): an HSM diagram, a finite
automaton (FA) diagram with the automaton tools and the EXT-FA check, the inspection of
a file exactly as it is stored, or a text - plain, Markdown or a C, C++, C# or Python
source, highlighted; any tab can also be made read-only (a read-only Markdown tab shows
the rendered document with its TeX formulas and SVG images, see `docs/MARKDOWN.md`).
Document > Compare with... opens a diff tab: two revisions of a
diagram side by side, the added, removed and changed elements marked and listed, or of a
text, two columns line by line with the changed characters marked.
Edit > Find (Ctrl+F) searches the text of a tab or the texts drawn on a diagram canvas,
the matches marked yellow and the current one orange (see `docs/FIND.md`).

![CyberiadaPro. Sample 1](images/gui-sample-1.jpg)

![CyberiadaPro. Sample 2](images/gui-sample-2.jpg)

## Plugins

The editor supports plugins (see ![The Plugin Interface](docs/PLUGIN-API.md))
which allow to process HSM diagrams and run controlled by HSM diagrams programs.
A plugin may also work on the text tabs, bring the programming languages of its tool and
give a diff tab the base revision of a diagram or a text.
The most interesting plugins are:

* ![HSM Diagram Maker](https://github.com/dralex/cybersphere-hsm-maker): generating HSM diagrams using LLM;
* ![HSM Python](https://github.com/dralex/hsm-to-python): running HSM diagrams with Python code;
* ![Finite Automata](https://github.com/kruzhok-team/fa-dispatcher-py/): running FA diargams;
* ![HSM-ROS2 Node Generator](https://github.com/kruzhok-team/hsm_robot_ros_generator): generating and checking HSM-based ROS2 code; 
* ![HSM-ROS2 Turtle](https://github.com/dralex/turtle_driver): running ROS2 turtle controlled by a HSM diagram;
* ![Git](https://github.com/dralex/cybersphere-git-plugin): staging and committing the diagram, its changes since a revision state by state.

## Installation

One can install ![binary releases](https://github.com/dralex/CyberiadaHSM-Editor/releases)
for Ubuntu Linux or Windows.

## Requirements

* [libcyberidaml++](https://github.com/kruzhok-team/libcyberiadamlpp/) and its required parts: [libcyberiadaml](https://github.com/kruzhok-team/libcyberiadaml), [libhtreegeom](https://github.com/kruzhok-team/libhtreegeom) and libxml2 
* cmake (version 3.12+)
* Qt Framework version 5.12+ with the Linguist tools (`qttools5-dev` and
  `qttools5-dev-tools` on Debian/Ubuntu)
* [QtPropertyBrowser](https://github.com/greenjava/QtPropertyBrowser/)

## Building from Sources

Configure and build with CMake:

```
cmake -B build
cmake --build build
```

If the dependencies are not in the default system location, point
`find_package` at them with `-DCMAKE_PREFIX_PATH=<prefix>`.

The Russian translations of the Qt dialogs are taken from the Qt translations
folder (`qttranslations5-l10n` on Debian/Ubuntu); `-DQT_TRANSLATIONS_DIR=<dir>`
names another one. The interface translations are described in
[docs/TRANSLATION.md](docs/TRANSLATION.md).

The Markdown rendering ([docs/MARKDOWN.md](docs/MARKDOWN.md)) uses md4c and
JKQTMathText, kept in `3rdparty`; `-DCYBERIADA_WITH_MATH=OFF` builds the editor without the
TeX formulas.

Build a Debian package with `cpack -G DEB` from the build directory.

To build on Windows, see [docs/BUILDING-WINDOWS.md](docs/BUILDING-WINDOWS.md).

The editor is extended by plugins, external programs shipped by the tools themselves: see
`docs/PLUGINS.md` for the plugin system architecture and `docs/PLUGIN-API.md` for writing
a plugin (in Russian: `docs/PLUGIN-API-ru.md`).

## Testing

The editor has a console test system based on the batch mode and the Qt
offscreen platform. Build the project, then run `./run-tests.sh` from the
repository root. See `docs/TESTING.md` for the testing architecture, the
batch mode contract and the test layers, and `docs/POLYGON.md` for the
agent-driven test polygon (`docs/POLYGON-PLUGINS.md` for its plugin part).

