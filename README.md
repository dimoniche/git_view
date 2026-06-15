# git_view

Minimal Git GUI for Linux (Ubuntu), focused on branch history and merge operations.
Built with **Qt 6** and the system **git** CLI.

Application version is defined in [`VERSION`](VERSION) (used by CMake and the running app).
For Debian packages, bump the upstream part in `debian/changelog` to match.

## Requirements

- CMake 3.16+
- C++17 compiler
- Qt 6 Widgets (`qt6-base-dev` on Ubuntu)
- Git 2.34+
- macOS/Linux: POSIX PTY for the built-in terminal (bundled [libvterm](https://github.com/neovim/libvterm))

### Ubuntu

```bash
sudo apt install build-essential cmake git qt6-base-dev
```

### macOS (Homebrew)

```bash
brew install qt@6 cmake git
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
```

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/src/git_view
```

On macOS, pass `CMAKE_PREFIX_PATH` if Qt is not found automatically (see above).

## Terminal

The bottom panel is a **real interactive terminal** (PTY + VT100 emulator): you can run `git`, `vim`, `htop`, pagers, and full-screen programs. Working directory is the repository root. Press **Ctrl+`** to focus it.

## Tests

```bash
cmake -B build -DGIT_VIEW_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Create a sample repository for manual testing:

```bash
./tests/fixtures/create_fixture_repo.sh /tmp/git_view-fixture
./build/src/git_view
# File → Open repository → /tmp/git_view-fixture
```

## Command line

```bash
git_view --help
git_view /path/to/repo
git_view --repo /path/to/repo --action working-changes
git_view --action file-history --file src/main.cpp /path/to/repo
git_view --action file-diff --diff-scope staged --file README.md /path/to/repo
```

Actions: `open`, `working-changes`, `file-history`, `file-diff`, `commit`, `log`.

## File manager integration (Ubuntu / Nautilus)

### Debian package (recommended on Ubuntu)

```bash
sudo apt install build-essential devscripts debhelper cmake g++ qt6-base-dev
./packaging/build-deb.sh
sudo apt install ./git-view_*.deb
nautilus -q
```

The package installs:

| Component | Path |
|-----------|------|
| Application | `/usr/bin/git_view` |
| Menu entry | Applications → Development → git_view |
| Nautilus scripts | **Scripts → GitView** |
| Context menu extension | main menu (needs `python3-nautilus`, recommended) |

Recommended packages for full Nautilus menu:

```bash
sudo apt install nautilus python3-nautilus python3-gi zenity
```

### Manual install (from source tree)

TortoiseGit-style context menu for git repositories:

```bash
chmod +x integrations/install-nautilus-integration.sh
./integrations/install-nautilus-integration.sh /path/to/git_view
nautilus -q
```

- **Scripts → GitView** — works without extra packages.
- **Direct context menu items** — install `python3-nautilus`, then re-run the installer.

Configuration (manual install only): `~/.config/git_view/integration.conf` (`GIT_VIEW_BIN`).

## Features (current)

- Open local repository, list branches (local and remote)
- Commit history with **branch graph** (lane layout) and metadata columns
- Commit details panel: file list + **per-file diff** (`git show`, syntax highlighting)
- **Working tree** tab: uncommitted/staged changes (`git status`, `git diff`)
- **Commit** working tree changes (`git add -A`, `git commit`) via dialog
- Create branches (`git branch` / `git checkout -b`, optional base ref)
- Merge branch into current (optional `--no-ff`, conflict file list, abort merge)
- Load more commits (pagination in pages of 500)

## Project layout

```
src/
  core/     Domain models, GraphLayout
  git/      Git CLI wrapper and parsers
  ui/       MainWindow, CommitHistoryView, CommitDetailsPanel
tests/
  fixtures/ Sample repo generator
```
staged-test
