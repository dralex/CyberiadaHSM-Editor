# The Interface Translations

**Document version:** 0.1 (2026-10-14)

The readers are the translators and the reviewers of the interface; the last section is for
the developers who add strings. The requirements are `EDIT-UI-1..4` in `EDITOR-SPEC.md` §4.13.

## Structure

```
 *.cpp *.h *.ui ───────────── lupdate ──┐   cmake --build build --target translations-update
 QtPropertyBrowser/src ────── lupdate ──┤   (-DQTPROPERTYBROWSER_SOURCE_DIR=...)
                                        ▼
            translations/cyberiada-pro_<lang>.ts         the editor strings
            translations/qtpropertybrowser_<lang>.ts     the property panel labels
                                        │   Qt Linguist: translate, review, mark done
                                        ▼   every build: lrelease
            <build>/i18n/*.qm + qtbase_<lang>.qm (from the Qt used for the build)
                                        │   generated i18n.qrc, linked into the editor
                                        ▼
            :/i18n/…  ──►  GUI start: QTranslator × 3  ──►  the main window
                           batch mode: none (English, EDIT-UI-2)
```

The language is chosen in *View ▸ Preferences ▸ General ▸ Interface language* and applies
at the next start. *System default* takes the first of the system UI languages that has a
translation, else English. The setting is stored as `interface/language` (`""`, `en`, `ru`, …).
Qt's own strings (the standard buttons, the colour and font dialogs) come from
`qtbase_<lang>.qm` of the running Qt, or from the copy bundled at build time.

## Workflow

1. `cmake --build build --target translations-update` — refresh the `.ts` files from the
   sources (new strings appear unfinished, removed ones become obsolete);
2. translate and review in Qt Linguist (`linguist translations/cyberiada-pro_ru.ts`);
   a reviewed entry is marked *done* (`Ctrl+Return`);
3. build — `lrelease` compiles the `.ts` into the resources; unfinished entries are
   included too, so a draft shows in the editor before its review.

The Russian translation is a draft: every entry is unfinished until reviewed.

## Glossary (Russian)

The terms follow ПНСТ 984-2024 (clause 3); a menu or a tool may use the short form.

| English | Русский | Short form | ПНСТ 984 |
|---|---|---|---|
| state machine | машина состояний | | 3.20 (РИМС) |
| state / composite state | состояние / составное состояние | | 3.48 / 3.46 |
| transition / internal transition | переход / внутренний переход | | 3.30 / 3.5 |
| pseudostate | псевдосостояние | | 3.42 |
| initial pseudostate | начальное псевдосостояние | начальное | 3.22 |
| final state | конечное состояние | | 3.15 |
| terminate pseudostate | завершающее псевдосостояние | завершение | 3.9 |
| choice pseudostate | псевдосостояние выбора | выбор | 3.6 |
| shallow / deep history | псевдосостояние локальной / глубокой истории | история / глубокая история | 3.18 / 3.8 |
| entry / exit point | псевдосостояние точки входа / выхода | точка входа / выхода | 3.50 / 3.51 |
| submachine state | состояние с вложенной машиной состояний | вложенная машина | 3.49 |
| region | область | | 3.24 |
| comment / formal / informal | комментарий / формальный (машиночитаемый) / неформальный (человекочитаемый) | | 3.12 / 3.13 / 3.14 |
| subject (of a comment) | предмет комментирования | предмет | 3.38 |
| event, trigger | событие | | 3.44 |
| guard | ограждающее условие | условие | 3.26 |
| action, behaviour | поведение; действие (a transition's) | | 3.32 |
| entry / exit / do behaviour | поведение входа / выхода / исполнения | | 3.33 / 3.34 / 3.35 |
| transition order | порядок выполнения перехода | | 3.36 |
| metainformation | метаинформация | | |
| element / document | элемент / документ | | 3.56 |
| label, polyline | надпись, ломаная | | |
| platform language (`platformLanguage`) | язык платформы | язык кода (Edit menu) | |
| markup | разметка | | |
| inspection mode | режим инспекции | | |
| Preferences | Настройки | | |
| plugin, plugin manager | плагин, менеджер плагинов | | |
| diagnostics (of a plugin) | замечания | | |
| toolbar, dock | панель инструментов, панель | | |

## Rules

- Never translate the notation and the format names: `entry`, `exit`, `do`, `propagate`,
  `block`, `defer`, `else`, `H`, `H*`, `CGML_*`, `platformLanguage`, `markupLanguage`, the
  file format names (`graphml`, `yEd`, `PNG`, `SVG`, …) and the keyboard shortcuts.
- The property names of one element must stay distinct (the panel finds a row by its label)
  and short: the panel is narrow, use the short forms of the glossary.
- Undo and redo texts are built as `Undo %1` from a noun: translate as «Отменить: %1» and
  the nouns in the nominative case.
- Keep `%1`, `%2`, the HTML tags and `\n` of the source.

## Adding a language

1. add the code to `EDITOR_TRANSLATIONS` in `CMakeLists.txt`;
2. run `translations-update` — it creates `translations/*_<code>.ts`;
3. translate; the language appears in the Preferences by its own name.

## For the developers

- a visible string is `tr()` in a `Q_OBJECT` class; a class without `Q_OBJECT` declares
  `Q_DECLARE_TR_FUNCTIONS(Class)`; a constant is marked `QT_TRANSLATE_NOOP(context, text)` and
  translated where it is shown;
- write whole sentences with `%1`, never glue translated fragments; give a short or ambiguous
  word a disambiguation comment: `tr("Open", "file")`;
- never translate what a machine reads: the batch dump, the script verbs, the session log,
  the plugin protocol, the settings keys, the saved document (except the default names of new
  elements, which follow the interface language in the GUI only);
- the batch mode and the in-process tests install no translator: their texts are the sources.

## Plugin texts

A plugin translates its own texts (`EDIT-UI-4`): its directory carries `i18n/<lang>.json`, a
map from the English text of `plugin.json` to the translation, read when the plugins are
scanned (`docs/PLUGIN-API.md`, Translation). They are not in the editor `.ts` files; the
plugin's authors keep them with the plugin. The editor tells a run the interface language
(`EDIT-PLUG-33`), so the plugin's own messages can follow it.
