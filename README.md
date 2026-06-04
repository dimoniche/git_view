# git_view

Minimal Git GUI for Linux (Ubuntu), focused on branch history and merge operations.
Built with **Qt 6** and the system **git** CLI.

## Requirements

- CMake 3.16+
- C++17 compiler
- Qt 6 Widgets (`qt6-base-dev` on Ubuntu)
- Git 2.34+

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
