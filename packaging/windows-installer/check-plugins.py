# -----------------------------------------------------------------------------
#  CyberiadaPro, the Cyberiada State Machine Editor
#
#  The check of the python plugins of an installation: the installer runs it
#  after the install, the Start-menu shortcut "Check CyberiadaPro plugins" later
#
#  Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or (at your
#  option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
#  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
#  License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with this program. If not, see https://www.gnu.org/licenses/
#
#  -----------------------------------------------------------------------------

"""Check that the editor finds the python plugins and runs them with this Python.

Run with the Python the plugins are bound to:

    python check-plugins.py --editor <CyberiadaPro.exe> [--samples DIR] [--log FILE]
                            [--summary FILE] [--language en|ru] [--pause]

Every check prints "ok" or "FAIL" with what to check; the exit code is 0 when
all pass, 1 otherwise. Only the standard library is used, so the check runs
even when the packages are missing.
"""

import argparse
import importlib
import importlib.metadata
import json
import os
import shutil
import subprocess
import sys
import tempfile

PLUGINS = ('hsm-to-python', 'hsm-maker')
MODULES = ('cyberiadaml_py.cml', 'pydantic', 'hsmd', 'gencode', 'hsm_maker')
DISTRIBUTIONS = ('cyberiadaml-py', 'pydantic', 'hsm-dispatcher-py', 'hsm-to-python',
                 'cybersphere-hsm-maker')
TIMEOUT = 120

TEXTS = {
    'en': {
        'imports': 'the python packages import',
        'imports-hint': 'the packages are not installed into {python}; install CyberiadaPro '
                        'again (Repair) and see logs\\pip.log in the CyberiadaPro folder',
        'editor': 'the editor starts',
        'editor-hint': 'the editor did not start ({detail}); check that {editor} exists and '
                       'that an antivirus does not block it',
        'listed': 'the editor finds the plugin {plugin}',
        'missing-hint': 'the plugin is not in {folder}; install CyberiadaPro again (Repair)',
        'shadowed-hint': 'an older copy of the plugin in {folder} hides the installed one; '
                         'delete that folder',
        'disabled-hint': 'the editor disables the plugin: {reason}',
        'runs': 'the plugin {plugin} runs',
        'run-hint': 'the editor could not run the plugin with {python} ({detail}); check that '
                    'this Python is still installed and that an antivirus does not block it',
        'passed': 'all checks passed',
        'failed': '{count} check(s) failed',
        'pause': 'Press Enter to close this window.',
    },
    'ru': {
        'imports': 'пакеты Python импортируются',
        'imports-hint': 'пакеты не установлены в {python}; запустите установку CyberiadaPro '
                        'ещё раз (Восстановить) и посмотрите logs\\pip.log в папке CyberiadaPro',
        'editor': 'редактор запускается',
        'editor-hint': 'редактор не запустился ({detail}); проверьте, что {editor} на месте и '
                       'что его не блокирует антивирус',
        'listed': 'редактор находит модуль {plugin}',
        'missing-hint': 'модуля нет в {folder}; запустите установку CyberiadaPro ещё раз '
                        '(Восстановить)',
        'shadowed-hint': 'старая копия модуля в {folder} закрывает установленный; удалите эту '
                         'папку',
        'disabled-hint': 'редактор отключил модуль: {reason}',
        'runs': 'модуль {plugin} работает',
        'run-hint': 'редактор не смог запустить модуль через {python} ({detail}); проверьте, '
                    'что этот Python по-прежнему установлен и его не блокирует антивирус',
        'passed': 'все проверки пройдены',
        'failed': 'проверок не пройдено: {count}',
        'pause': 'Нажмите Enter, чтобы закрыть окно.',
    },
}


class Check:
    """The results of the checks, printed as they come and kept for the log."""

    def __init__(self, language, log):
        self.text = TEXTS.get(language, TEXTS['en'])
        self.log = log
        self.failures = 0
        self.lines = []        # what was printed, for the summary file

    def say(self, key, **values):
        return self.text[key].format(**values)

    def result(self, ok, name, hint=None, output=None):
        line = ('ok   ' if ok else 'FAIL ') + name
        if not ok:
            self.failures += 1
            if hint:
                line += '\n     ' + hint
        print(line, flush=True)
        self.lines.append(line)
        if self.log:
            self.log.write(line + '\n')
            if output:
                self.log.write('     ' + output.replace('\n', '\n     ').rstrip() + '\n')
        return ok


def run(command, env):
    """Run a command, its output decoded (the editor writes UTF-8)."""
    try:
        done = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=TIMEOUT, check=False)
    except (OSError, subprocess.SubprocessError) as e:
        return None, str(e)
    return done.returncode, done.stdout.decode('utf-8', errors='replace')


def editor_environment(editor):
    """The environment of the editor runs: headless when the bundle has the
    offscreen platform, else the default one (a window shows briefly)."""
    env = dict(os.environ)
    offscreen = os.path.join(os.path.dirname(editor), 'platforms', 'qoffscreen.dll')
    if 'QT_QPA_PLATFORM' not in env and os.path.isfile(offscreen):
        env['QT_QPA_PLATFORM'] = 'offscreen'
    return env


def user_plugin_folder():
    """The user plugin folder of the editor (it shadows the installed plugins)."""
    base = os.environ.get('APPDATA') or os.path.join(os.path.expanduser('~'), 'AppData',
                                                     'Roaming')
    return os.path.join(base, 'Cyberiada', 'CyberiadaPro', 'plugins')


def listed_plugins(output):
    """The --list-plugins lines: id -> (status, origin, reason)."""
    plugins = {}
    for line in output.splitlines():
        if not line or line.startswith(('==', ' ')):
            continue
        parts = line.split(' ', 3)
        if len(parts) < 4:
            continue
        origin, _, reason = parts[3].partition(': ')
        plugins.setdefault(parts[0], (parts[2], origin, reason))
    return plugins


def check_imports(check):
    """The packages the plugins import, in this Python."""
    problems = []
    for module in MODULES:
        try:
            importlib.import_module(module)
        except Exception as e:  # pylint: disable=broad-except
            problems.append('{}: {}'.format(module, e))
    versions = []
    for dist in DISTRIBUTIONS:
        try:
            versions.append('{} {}'.format(dist, importlib.metadata.version(dist)))
        except importlib.metadata.PackageNotFoundError:
            versions.append('{} -'.format(dist))
    return check.result(not problems, check.say('imports'),
                        check.say('imports-hint', python=sys.executable),
                        '\n'.join(problems + versions))


def check_listed(check, editor, env):
    """The editor lists both plugins, enabled, from the installation."""
    code, output = run([editor, '--batch', '--list-plugins'], env)
    if code != 0:
        check.result(False, check.say('editor'),
                     check.say('editor-hint', editor=editor,
                               detail='exit {}'.format(code) if code is not None else output),
                     output)
        return False
    check.result(True, check.say('editor'), output=output)
    plugins = listed_plugins(output)
    folder = os.path.join(os.path.dirname(editor), 'share', 'cyberiada-pro', 'plugins')
    ok = True
    for plugin in PLUGINS:
        status, origin, reason = plugins.get(plugin, (None, None, None))
        if status == 'enabled' and origin == 'system':
            hint = None
        elif status is None:
            hint = check.say('missing-hint', folder=folder)
        elif origin == 'user' or status == 'shadowed':
            hint = check.say('shadowed-hint',
                             folder=os.path.join(user_plugin_folder(), plugin))
        else:
            hint = check.say('disabled-hint', reason=reason or status)
        ok = check.result(hint is None, check.say('listed', plugin=plugin), hint) and ok
    return ok


def pinned_python(editor, plugin):
    """The Python an installed plugin is bound to: the first argument of its
    actions (the installer pins them), else this Python."""
    base = os.path.dirname(os.path.abspath(editor))
    # the system plugin folders of the editor: <exe>/share (Windows), <exe>/../share
    for root in (base, os.path.dirname(base)):
        descriptor = os.path.join(root, 'share', 'cyberiada-pro', 'plugins', plugin,
                                  'plugin.json')
        try:
            with open(descriptor, encoding='utf-8') as file:
                data = json.load(file)
        except (OSError, ValueError):
            continue
        try:
            if 'interpreter' not in data:
                return data['actions'][0]['argv'][0]
        except (KeyError, IndexError, TypeError):
            pass
        break
    return sys.executable


def check_run(check, editor, env, document, action, marks, work):
    """One plugin action on a copy of a sample diagram."""
    plugin = action.partition(':')[0]
    copy = os.path.join(work, os.path.basename(document))
    shutil.copyfile(document, copy)
    code, output = run([editor, '--batch', copy, '--run-plugin', action, '--dump-plugin'], env)
    ok = code == 0 and all(mark in output for mark in marks)
    detail = 'exit {}'.format(code) if code is not None else output
    errors = [line.strip() for line in output.splitlines() if line.startswith('error:')]
    if errors:
        detail += '; ' + errors[0]
    return check.result(ok, check.say('runs', plugin=plugin),
                        check.say('run-hint', python=pinned_python(editor, plugin),
                                  detail=detail), output)


def main(argv=None):
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description='Check the python plugins of CyberiadaPro.')
    parser.add_argument('--editor', required=True, help='the CyberiadaPro executable')
    parser.add_argument('--samples', default=os.path.join(here, 'samples'),
                        help='the folder of the sample diagrams')
    parser.add_argument('--log', help='append the full output to this file')
    parser.add_argument('--summary', help='write the printed lines to this file (UTF-16, '
                        'the form the installer reads)')
    parser.add_argument('--language', default='en', choices=sorted(TEXTS))
    parser.add_argument('--pause', action='store_true', help='wait for Enter at the end')
    args = parser.parse_args(argv)

    log = open(args.log, 'a', encoding='utf-8') if args.log else None
    check = Check(args.language, log)
    try:
        env = editor_environment(args.editor)
        imported = check_imports(check)
        if check_listed(check, args.editor, env) and imported:
            with tempfile.TemporaryDirectory() as work:
                check_run(check, args.editor, env,
                          os.path.join(args.samples, 'code_layout.graphml'),
                          'hsm-to-python:check', ('state: done',), work)
                check_run(check, args.editor, env,
                          os.path.join(args.samples, 'lift.graphml'),
                          'hsm-maker:notation', ('state: done', 'machine Lift'), work)
        summary = check.say('passed') if not check.failures else check.say(
            'failed', count=check.failures)
        print(summary, flush=True)
        check.lines.append(summary)
        if log:
            log.write(summary + '\n')
        if args.summary:
            with open(args.summary, 'w', encoding='utf-16-le', newline='') as file:
                file.write(''.join(line.replace('\n', '\r\n') + '\r\n' for line in check.lines))
    finally:
        if log:
            log.close()
    if args.pause:
        input(check.say('pause'))
    return 1 if check.failures else 0


if __name__ == '__main__':
    sys.exit(main())
