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

Every check prints "ok" or "FAIL" with what to check, or "note" when git is not
installed (the git of the editor waits for it); the exit code is 0 when no check
fails, 1 otherwise. Only the standard library is used, so the check runs even
when the packages are missing.
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

PLUGINS = ('hsm-to-python', 'hsm-maker', 'fa-dispatcher')
MODULES = ('cyberiadaml_py.cml', 'pydantic', 'hsmd', 'gencode', 'hsm_maker', 'fad')
DISTRIBUTIONS = ('cyberiadaml-py', 'pydantic', 'pydantic-core', 'hsm-dispatcher-py',
                 'hsm-to-python', 'cybersphere-hsm-maker', 'fa-dispatcher-py')
TIMEOUT = 120

TEXTS = {
    'en': {
        'imports': 'the python packages import',
        'imports-hint': 'the packages are not installed into {python}; install CyberiadaPro '
                        'again (Repair) and see logs\\pip.log in the CyberiadaPro folder',
        'core-hint': 'pydantic-core does not load into {python} ({detail}); on Windows 7 '
                     'install Service Pack 1 and the update KB2533623, then install '
                     'CyberiadaPro again (Repair)',
        'editor': 'the editor starts',
        'editor-hint': 'the editor did not start ({detail}); check that {editor} exists and '
                       'that an antivirus does not block it',
        'listed': 'the editor finds the plugin {plugin}',
        'missing-hint': 'the plugin is not in {folder}; install CyberiadaPro again (Repair)',
        'shadowed-hint': 'an older copy of the plugin in {folder} hides the installed one; '
                         'delete that folder',
        'disabled-hint': 'the editor disables the plugin: {reason}',
        'git': 'the editor runs git',
        'program-hint': 'install git (Git for Windows, git-scm.com) so that it is on PATH: '
                        'the editor then shows the repository of a diagram',
        'git-hint': 'the editor did not stage a file of a new repository ({detail}); check '
                    'the Git for Windows installation',
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
        'core-hint': 'pydantic-core не загружается в {python} ({detail}); в Windows 7 '
                     'установите пакет обновления 1 и обновление KB2533623, затем запустите '
                     'установку CyberiadaPro ещё раз (Восстановить)',
        'editor': 'редактор запускается',
        'editor-hint': 'редактор не запустился ({detail}); проверьте, что {editor} на месте и '
                       'что его не блокирует антивирус',
        'listed': 'редактор находит модуль {plugin}',
        'missing-hint': 'модуля нет в {folder}; запустите установку CyberiadaPro ещё раз '
                        '(Восстановить)',
        'shadowed-hint': 'старая копия модуля в {folder} закрывает установленный; удалите эту '
                         'папку',
        'disabled-hint': 'редактор отключил модуль: {reason}',
        'git': 'редактор запускает git',
        'program-hint': 'установите git (Git для Windows, git-scm.com), чтобы он был в PATH: '
                        'тогда редактор показывает репозиторий диаграммы',
        'git-hint': 'редактор не добавил файл нового репозитория в индекс ({detail}); '
                    'проверьте установку Git для Windows',
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

    def result(self, ok, name, hint=None, output=None, note=False):
        line = ('ok   ' if ok else 'note ' if note else 'FAIL ') + name
        if not ok:
            if not note:
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

    def note(self, name, hint):
        """Not a failure: the plugin is installed and waits for the user."""
        return self.result(False, name, hint, note=True)


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
    core = None            # why pydantic-core (a binary module) did not load
    for module in MODULES:
        try:
            importlib.import_module(module)
        except Exception as e:  # pylint: disable=broad-except
            problems.append('{}: {}'.format(module, e))
            if core is None and 'DLL load failed' in str(e):
                core = str(e)
    found = []
    for dist in DISTRIBUTIONS:
        try:
            found.append('{} {}'.format(dist, importlib.metadata.version(dist)))
        except importlib.metadata.PackageNotFoundError:
            found.append('{} -'.format(dist))
    if core is not None:
        hint = check.say('core-hint', python=sys.executable, detail=core)
    else:
        hint = check.say('imports-hint', python=sys.executable)
    return check.result(not problems, check.say('imports'), hint, '\n'.join(problems + found))


def check_listed(check, editor, env):
    """The editor lists the plugins, enabled, from the installation; whether
    all are, the enabled ones and whether the editor started."""
    code, output = run([editor, '--batch', '--list-plugins'], env)
    if code != 0:
        check.result(False, check.say('editor'),
                     check.say('editor-hint', editor=editor,
                               detail='exit {}'.format(code) if code is not None else output),
                     output)
        return False, set(), False
    check.result(True, check.say('editor'), output=output)
    plugins = listed_plugins(output)
    folder = os.path.join(os.path.dirname(editor), 'share', 'cyberiada-pro', 'plugins')
    ok = True
    enabled = set()
    for plugin in PLUGINS:
        status, origin, reason = plugins.get(plugin, (None, None, None))
        if status == 'enabled' and origin == 'system':
            hint = None
            enabled.add(plugin)
        elif status is None:
            hint = check.say('missing-hint', folder=folder)
        elif origin == 'user' or status == 'shadowed':
            hint = check.say('shadowed-hint',
                             folder=os.path.join(user_plugin_folder(), plugin))
        else:
            hint = check.say('disabled-hint', reason=reason or status)
        ok = check.result(hint is None, check.say('listed', plugin=plugin), hint) and ok
    return ok, enabled, True


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


def check_run(check, editor, env, document, action, marks, work, params=()):
    """One plugin action on a copy of a sample diagram; params name the files of
    the samples folder beside it: (parameter, file name)."""
    plugin = action.partition(':')[0]
    copy = os.path.join(work, os.path.basename(document))
    shutil.copyfile(document, copy)
    command = [editor, '--batch', copy, '--run-plugin', action, '--dump-plugin']
    for name, file in params:
        path = os.path.join(work, file)
        shutil.copyfile(os.path.join(os.path.dirname(document), file), path)
        command += ['--plugin-param', '{}={}'.format(name, path)]
    code, output = run(command, env)
    ok = code == 0 and all(mark in output for mark in marks)
    detail = 'exit {}'.format(code) if code is not None else output
    errors = [line.strip() for line in output.splitlines() if line.startswith('error:')]
    if errors:
        detail += '; ' + errors[0]
    return check.result(ok, check.say('runs', plugin=plugin),
                        check.say('run-hint', python=pinned_python(editor, plugin),
                                  detail=detail), output)


def check_git(check, editor, env, document, work):
    """The git of the editor stages a sample diagram in a new repository; without git on
    PATH a note, the editor shows no repository then."""
    if shutil.which('git', path=env.get('PATH')) is None:
        return check.note(check.say('git'), check.say('program-hint'))
    repo = os.path.join(work, 'repo')
    code, output = run(['git', 'init', '-q', repo], env)
    if code == 0:
        copy = os.path.join(repo, os.path.basename(document))
        shutil.copyfile(document, copy)
        script = os.path.join(work, 'stage.script')
        with open(script, 'w', encoding='utf-8') as file:
            file.write('git stage {}\n'.format(os.path.basename(document)))
        code, output = run([editor, '--batch', copy, '--script', script, '--dump-repository'], env)
    ok = code == 0 and 'A. ' + os.path.basename(document) in output
    detail = 'exit {}'.format(code) if code is not None else output
    return check.result(ok, check.say('git'), check.say('git-hint', detail=detail), output)


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
        listed, enabled, started = check_listed(check, args.editor, env)
        if listed and imported:
            with tempfile.TemporaryDirectory() as work:
                check_run(check, args.editor, env,
                          os.path.join(args.samples, 'code_layout.graphml'),
                          'hsm-to-python:check', ('state: done',), work)
                check_run(check, args.editor, env,
                          os.path.join(args.samples, 'lift.graphml'),
                          'hsm-maker:notation', ('state: done', 'machine Lift'), work)
                check_run(check, args.editor, env,
                          os.path.join(args.samples, 'morse-decoder.graphml'),
                          'fa-dispatcher:test', ('state: done', '8 passed'), work,
                          (('tests', 'morse-decoder.tests'),))
        # the git of the editor needs no python package, nor a plugin
        if started:
            with tempfile.TemporaryDirectory() as work:
                check_git(check, args.editor, env,
                          os.path.join(args.samples, 'code_layout.graphml'), work)
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
