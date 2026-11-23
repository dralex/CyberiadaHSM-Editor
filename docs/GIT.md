# Git in the Editor — Architecture

**Document version:** 0.1 (2026-11-23) — specified (stage 0). The requirements are
`EDIT-GIT-1..23` in `EDITOR-SPEC.md` §4.20, the disk check `EDIT-DOC-13..18` §4.11 and the
docks `EDIT-UI-6`.

The editor shows and changes the git repository of the active tab itself: the files of the
work tree with their status, the stage, the commit, the compares with a revision, the
history and the local branches. It runs the git program and reads its machine output only;
it links no git library and needs no plugin. The work tree is the only project the editor
knows: the dock follows the active tab, and a tab of another repository brings its own.

```
 CyberiadaSMEditorWindow
 | Git menu       Repository dock (left)        History dock (left, behind)
 | Commit dialog  Revision dialog  Branch dialog            Preferences > Git
 |        | verbs, paths, revisions                 ^ snapshot, log, results, errors
 |        v                                         |
 | RepositoryRegistry   folder -> work tree root (cached); root -> RepositoryService
 |        |
 | RepositoryService (QtCore, one per work tree)
 |   status: --no-optional-locks status --porcelain=v2 -z --branch --untracked-files=all
 |           [--ignored] + ls-files -z              -> RepositorySnapshot
 |   verbs:  add, restore, rm, mv, commit -F -, switch, branch, log, cat-file
 |   one command at a time, a queue; a generation number drops a stale read
 |   re-read: a save, a verb's end, the git folder watcher, the activation, Refresh;
 |            at most once a second, only while a git dock is shown
 |        |                                          ^ exit code, stdout bytes, stderr
 |        v                                          |
 | GitProcess (QtCore)  the program of Preferences or PATH, 2.23+; stdin closed,
 |                      GIT_TERMINAL_PROMPT=0, GIT_LITERAL_PATHSPECS=1; async in the
 |                      GUI, waited in the batch mode
 |        |
 |        v                                   git (a process per command)
 |
 | RepositoryModel (QAbstractItemModel)  the snapshot as a tree or as groups, filtered;
 |                                       the open and unsaved marks of the tabs
 | DiffOrigin::originGit                 the base of a diff tab: a revision or the index
 | DiskWatcher + EditorDocument::disk()  the files of the open tabs against the disk
```

## The git program

* `GitProcess::program()` is the path of Preferences > Git, else `git` in `PATH`; `git
  --version` runs once per path and a version before 2.23 (`git restore`, `git switch`)
  counts as not found. *Use git* off makes `program()` empty.
* A command is `GitProcess::start(root, args, stdin)`: `-C <root>`, stdin closed unless the
  command writes it (the commit message), `GIT_TERMINAL_PROMPT=0` so no command waits on a
  password, `GIT_LITERAL_PATHSPECS=1` so a path is never a pattern. `GitResult` holds the exit
  code, stdout as bytes and stderr as text.
* Only the `-z` output is parsed: the paths are bytes split at NUL, never quoted. The messages
  of git are localized; they pass to the user as they are and are never matched.
* The batch mode waits for every command (`waitForFinished`), so a script is deterministic.

## The repository of a tab

```
 tab activated / saved (Save As) / window activated
   -> EditorDocument::filePath() (a diff tab: its target's file)
   -> RepositoryRegistry::rootFor(folder): cached, else git rev-parse --show-toplevel
                                           --absolute-git-dir (async, then cached)
   same root as the dock        -> the dock stays; the tab's file is the current row
   another root                 -> RepositoryModel::setService(that root): the cached
                                   snapshot at once, the view state of the root restored,
                                   a read started
   no root / no file / no git   -> the placeholder line (EDIT-GIT-3)
```

* `rev-parse` gives the root of a linked work tree and of a submodule, and the git folder,
  which may lie outside the root (`.git` as a file).
* The registry keeps a service per root while the editor runs. The view state — the expanded
  folders and the scroll — is kept per root in the dock.
* The plugin token `git` keeps its own `.git` lookup (`EDIT-PLUG-55`); the registry does not
  change the plugins.

## The status

```
 git --no-optional-locks status --porcelain=v2 -z --branch --untracked-files=all [--ignored]
   # branch.oid <hash>|(initial)   # branch.head <name>|(detached)
   # branch.upstream <name>        # branch.ab +<ahead> -<behind>
   1 <XY> <sub> <mH> <mI> <mW> <hH> <hI> <path>
   2 <XY> <sub> <mH> <mI> <mW> <hH> <hI> <R|C><score> <path> NUL <old path>
   u <XY> <sub> <m1> <m2> <m3> <mW> <h1> <h2> <h3> <path>
   ? <path>      ! <path>
 git ls-files -z                                 the tracked files: the unchanged ones
 <git folder>/MERGE_HEAD, rebase-merge/, rebase-apply/   -> merging | rebasing
                     |
                     v
 RepositorySnapshot { branch, oid, upstream, ahead, behind, state,
                      entries: [{ path, oldPath, index, tree }] }      repository_status.*
```

* `parseStatus()` and `parseFiles()` are pure functions of bytes: the unit tests feed them
  recorded outputs. `X` and `Y` of `XY` are the index and the work tree letters, `.` none.
* A file both in `ls-files` and in the status takes the status letters; a file only in
  `ls-files` is unchanged.
* The read is `--no-optional-locks`: it never takes `index.lock`, so a `git commit` the user
  runs in a terminal at the same time does not fail.
* The watcher watches the git folder and `refs/heads`, not the `index` file: git replaces the
  index by a rename, and a watcher of the file would lose it.
* A read is asked by a save of a tab of the repository, the end of a verb, a watcher event,
  the window activation and Refresh; the asks within a second join into one read, and a
  service whose docks are hidden reads only when they are shown again.

## The model

```
 RepositorySnapshot + the open tabs (path -> unsaved)
          |
          v
 RepositoryModel::rebuild(layout, changedOnly, ignored)            repository_model.*
   tree:   folder nodes and file nodes; per folder the folders, then the files, the
           changed before the unchanged, then by name; a folder with a change gets *
   groups: Conflicts | Staged | Changes | Untracked | Ignored, flat paths, a file in
           Staged and Changes when both letters are set; empty groups left out
   columns: Name (● for an unsaved tab, the old path of a rename in the tooltip),
            Index, Tree
          |
          v
 RepositoryView: QTreeView over the model, the toolbar, the placeholder, the context
                 menu, the branch button                           repository_view.*
```

* One model with a layout, not two proxies: both layouts are a rebuild of the same snapshot,
  and the tree keeps the expansion by path across a rebuild.
* A rebuild after a read replaces the rows with `beginResetModel` only when the structure
  changed; otherwise the letters change in place (`dataChanged`), so the selection and the
  scroll stay.
* `RepositoryModel::files(indexes)` turns a selection into files: a folder or a group gives
  all its files.

## The verbs

| Verb | git | After it |
|---|---|---|
| Stage | `add -- <files>` | read |
| Unstage | `restore --staged -- <files>`; before the first commit `rm --cached -q -- <files>` | read |
| Discard | `restore -- <files>`; the untracked files removed by the editor | read; the tabs read again, edits dropped |
| Rename | `mv -- <from> <to>`; an untracked file renamed by the editor | read; the tab follows the path |
| Delete | `rm -q -- <files>`; an untracked file removed by the editor | read; the tabs closed |
| Commit | `commit -F -` [`--amend`], the message on stdin | read |
| Compare | `rev-parse --verify -q <rev>^{commit}`, `cat-file blob <hash>:<path>`; the index: `ls-files --stage -z -- <path>` + `cat-file blob <hash>` | the diff tab |
| History | `log --follow -z -n 100 --format=%H%x1f%h%x1f%ad%x1f%an%x1f%s --date=short -- <path>` | the History dock |
| Switch | `switch <branch>` | read; the disk check |
| New Branch | `switch -c <name> <start>` or `branch <name> <start>` | read; the disk check |
| Delete Branch | `branch -d <name>` | read |

* Every verb that may change the repository, and every failure, goes to the *Git output* tab
  of the plugin dock, headed `== git <command> - <root>` (`EDIT-GIT-17`): the plugin dock's
  output tabs are reused under the id `git`.
* A verb then runs the disk check (`EDIT-DOC-13`): any of them may rewrite a file under an
  open tab.
* The paths of a verb are relative to the root and passed after `--`; a revision typed by the
  user is joined to its option or checked not to start with `-`.

## The compares

```
 Compare with HEAD | the Index | a Revision...         compareDocument / compareText
   the file's tab (opened first)                       (smeditor_window.cpp)
   DiffOrigin { kind: originGit, path, revision ("" = the index), label }
        |
        v
 refreshDiff(): RepositoryService::base(path, revision)  (waited: a diff tab is filled
        | rev-parse REV^{commit} -> cat-file blob HASH:PATH       at once)
        | not there: a staged rename's old path (diff --cached -M --name-status), else ""
        v
 ComparisonDocument::setSides(base bytes, the tab's snapshot)
```

* The base is fetched again on every Refresh: `HEAD` is resolved anew after a commit.
* A conflicted file has stages 1..3 in the index, no stage 0: Compare with the Index is
  greyed for it; the three-way compare belongs to Later.
* The kind of the tab follows the file's tab: a diagram gets a `DiffDocument`, a text a
  `TextDiffDocument`, as `docs/MODES.md` (The diff tab) does for any base.

## The commit

```
 Commit...  -> CommitDialog(service)                              dialogs/commit_dialog.*
   title: Commit to <branch> | Commit at <hash>
   the staged entries of the snapshot (index letter set), a rename as old -> new
   a warning per unsaved tab of the root; "concludes the merge" while merging
   message (QPlainTextEdit), Amend (fills an empty message: log -1 --format=%B)
   Commit greyed: a blank message | nothing staged and no Amend | a conflicted entry
        | commit -F - [--amend]
        v
   exit 0: accept, read the status      else: git's stderr under the message, stay open
 the message not committed is kept per root until the editor exits
```

The hooks run as for any git command, without a terminal: a hook that asks on a terminal
fails, its message shown.

## The history

`HistoryModel` holds the rows of one query — a file (`--follow`) or the repository — and
fetches the next 100 on *More...*. The dock follows the current row of the Repository dock,
else the active tab. A double-click compares the file of the query (in the repository scope
the active tab's file) with the row's hash. A read of a query no longer shown is dropped.

## Branches

```
 the snapshot: branch.head, branch.oid, upstream, ab; state from the git folder
   devel | devel ↑1 ↓2 | detached at 3f2a9c1 | devel (no commits) | + ", merging"
 the branch list: git for-each-ref -z --format=%(refname:short) refs/heads
   (read with the status; the menu of the branch button and Git > Branch share it)

 a branch clicked (not while merging or rebasing)
   the unsaved tabs of the root -> "Save <n> documents before switching?"
                                   [Save and Switch] [Cancel]
   git switch <branch>
     refused: git's message (Git output + a message box); nothing changed
     done:    read the status; the disk check of every open tab
```

New Branch... (`dialogs/branch_dialog.*`) takes the name, the start (the current commit or a
local branch) and *Switch to it*; git checks the name. Delete Branch... lists the local
branches but the current one; `branch -d` refuses the unmerged ones.

## The disk check

```
 EditorDocument::disk(): DiskState { size, mtime, digest (SHA-1 of the bytes) }
   set by every read and every write of the tab's file                disk_state.*
 DiskWatcher (window): QFileSystemWatcher on the files of the open tabs
   (a path re-added after git or a save replaced the file by a rename)
 triggers: the watcher, the window activation, a git verb's end, the batch disk-check
        |
        v
 check(tab):  size+mtime same          -> nothing
              digest same              -> take the new mtime (a touch, our own save)
              file gone                -> bar "not on disk" [Close] [Keep]; modified
              changed, tab clean       -> read again (mode, flag, zoom, scroll kept)
              changed, tab edited      -> bar "changed on disk" [Reload] [Keep my edits]
                                          [Compare]
              changed, cannot be read  -> the tab as it was + the bar
 Save: the file changed since the remembered state -> "Overwrite it?" (Cancel default)
```

* The bar (`disk_bar.*`) is a strip above the page of the tab, like the find bar under it.
* *Keep my edits* takes the file's state as the remembered one: the next Save writes without
  a question, as the user chose.
* A reload empties the undo history: the history of the old bytes does not apply to the new
  ones.
* Discard reads its tabs again even with edits: the confirmation named them
  (`EDIT-GIT-10`).

## Batch mode

The verbs are `EDIT-GIT-21` and `EDIT-DOC-18`; the dumps `EDIT-GIT-22`. The script's
`commit`, `compare` and `discard` were taken, so the git verbs go under `git`, the dock state
under `repository` and the bar under `disk`. A test makes its repositories with a
configuration of its own — `GIT_CONFIG_GLOBAL` and `GIT_CONFIG_SYSTEM` empty files, `HOME`
in the temporary folder — and fixed `GIT_AUTHOR_DATE` and `GIT_COMMITTER_DATE`, so the
hashes in the dumps are the same on every run.

## Code layout

| File | Holds |
|---|---|
| `git_process.{h,cpp}` | `GitProcess`, `GitResult`: the program, the environment, the runs |
| `repository_status.{h,cpp}` | `RepositorySnapshot`, `StatusEntry`, `parseStatus()`, `parseFiles()`, `parseLog()` |
| `repository_service.{h,cpp}` | `RepositoryRegistry`, `RepositoryService`: the roots, the reads, the verbs, the queue, the watcher |
| `repository_model.{h,cpp}` | `RepositoryModel`: the layouts, the filters, the marks |
| `repository_view.{h,cpp}` | `RepositoryView`: the dock contents, the toolbar, the branch button, the placeholder, the context menu |
| `history_view.{h,cpp}` | `HistoryModel`, `HistoryView` |
| `dialogs/commit_dialog.*`, `dialogs/revision_dialog.*`, `dialogs/branch_dialog.*` | the dialogs |
| `disk_state.{h,cpp}`, `disk_bar.{h,cpp}` | `DiskState`, `DiskWatcher`, `DiskBar` |
| `batch_git.{h,cpp}` | the `git`, `repository` and `disk` verbs, `--dump-repository`, `--dump-history` |
| `comparison_document.h` | `DiffOrigin::originGit` |
| `smeditor_window.*`, `editor_document.*`, `dialogs/preferences_dialog.*` | the menu, the docks, the wiring; the disk state of a tab; the Git page |

## The git plugin and the textconv

The git plugin of cybersphere-git-plugin is superseded: its stage, commit, compares and
history are the verbs above, its diff text the change list of a diff tab. Its one function
outside the editor, the listing of a diagram that makes `git diff` show the elements, is
`cybparser listing` of libcyberiadaml:

```
 .gitattributes:  *.graphml diff=cyberiada
 git config diff.cyberiada.textconv "cybparser listing"
```

The `cyberiada-pro` package breaks the old plugin package, so the Plugins menu never shows a
second Git.

## Testing

* `repository_status` on recorded outputs: every entry kind, a rename, the `-z` paths with
  spaces and newlines, the branch headers (initial, detached, an upstream).
* The service, the dock, the dialogs and the disk check in `l4-git` on temporary
  repositories; `QSKIP` without git.
* The `git-*`, `repository-*` and `disk-*` batch cases with `--dump-repository`,
  `--dump-history` and `--dump-tabs` goods (`docs/TESTING.md`, Git).

## Stages

0. The specification and this document.
1. `cybparser listing` in libcyberiadaml.
2. The disk check (`EDIT-DOC-13..18`).
3. `GitProcess`, `repository_status`, `RepositoryService`, `git refresh`, `--dump-repository`.
4. The Repository dock: the model, the layouts, the filters, the tab following, Open,
   Preferences > Git, the `repository` verbs.
5. The verbs, the compares, the commit, the history, the Git menu, the Git output.
6. The branches.
7. The git plugin retired from the packaging and the installers.

## Later

* A change set of the whole repository (the files, then the elements) and the three-way
  compare of a conflicted diagram (the index stages 1..3).
* A rename that updates the Markdown links and the submachine references to the file.
* Merge, rebase, stash; the remotes — fetch, pull, push with the upstream, the credentials
  through `GIT_ASKPASS`. Until then they stay in File > Open Terminal (`EDIT-DOC-12`).

## Decisions of 0.1

* Git is a part of the editor, not a plugin: a dock and a repository scope are outside the
  plugin interface, and the work of several files builds on it.
* The git program, not libgit2: the hooks, the whole configuration, the credential helpers,
  the signing and the filters work as in a terminal; no library joins the build.
* No project of its own: the work tree of the active tab. No work tree, no dock content.
* Both layouts, a tree and status groups, from one snapshot.
* The commit takes the index; the dialog shows it and never stages.
* Local branches only; a switch saves the unsaved tabs first, then checks the open tabs
  against the disk.
* A file changed on disk reloads a clean tab, bars an edited one.
