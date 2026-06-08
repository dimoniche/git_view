#!/usr/bin/env python3
"""Nautilus context menu integration for git_view (TortoiseGit-style)."""

import os
import subprocess
from pathlib import Path

import gi

try:
    gi.require_version("Nautilus", "4.0")
except ValueError:
    gi.require_version("Nautilus", "3.0")

from gi.repository import GObject, Nautilus  # noqa: E402

CONFIG_PATH = (
    Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    / "git_view"
    / "integration.conf"
)


def load_binary():
    if CONFIG_PATH.is_file():
        for line in CONFIG_PATH.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("GIT_VIEW_BIN="):
                value = line.split("=", 1)[1].strip().strip('"').strip("'")
                if value and os.access(value, os.X_OK):
                    return value
    from shutil import which

    return which("git_view")


def git_toplevel(path):
    try:
        output = subprocess.check_output(
            ["git", "-C", path, "rev-parse", "--show-toplevel"],
            stderr=subprocess.DEVNULL,
            text=True,
        )
        return output.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def normalize_local_path(path):
    if path.startswith(":/"):
        return path[1:]
    if path.lower().startswith("file:"):
        from urllib.parse import unquote, urlparse

        parsed = urlparse(path)
        if parsed.path:
            return unquote(parsed.path)
        remainder = path[5:]
        if remainder.startswith("//"):
            remainder = remainder[2:]
        elif remainder.startswith("/"):
            remainder = remainder[1:]
        return unquote(remainder)
    return path


def repo_relative_paths(repo, paths):
    relative = []
    for raw_path in paths:
        path = normalize_local_path(raw_path)
        try:
            rel = os.path.relpath(path, repo)
        except ValueError:
            continue
        if rel in (".", ""):
            continue
        relative.append(rel)
    return relative


def launch(action, repo, paths=None):
    binary = load_binary()
    if not binary:
        return

    command = [binary, "--repo", repo, "--action", action]
    if paths:
        for rel_path in repo_relative_paths(repo, paths):
            command.extend(["--file", rel_path])
    subprocess.Popen(command, start_new_session=True)


def separator(name):
    item = Nautilus.MenuItem(name=name, label="")
    return item


class GitViewExtension(GObject.GObject, Nautilus.MenuProvider):
    def _repo_context(self, files):
        if not files:
            return None, [], []

        first_path = normalize_local_path(files[0].get_location().get_path())
        repo = git_toplevel(first_path)
        if not repo:
            return None, [], []

        selected_files = []
        selected_dirs = []
        for item in files:
            path = normalize_local_path(item.get_location().get_path())
            if item.is_directory():
                selected_dirs.append(path)
            else:
                selected_files.append(path)
        return repo, selected_files, selected_dirs

    def _activate_repo_action(self, _menu, action, repo):
        launch(action, repo)

    def _activate_path_action(self, _menu, action, repo, paths):
        launch(action, repo, paths)

    def _menu_item(self, name, label, action, repo, paths=None):
        item = Nautilus.MenuItem(name=name, label=label)
        if paths:
            item.connect("activate", self._activate_path_action, action, repo, paths)
        else:
            item.connect("activate", self._activate_repo_action, action, repo)
        return item

    def _build_items(self, repo, selected_files, selected_dirs):
        items = [
            self._menu_item("GitView::open", "Open in git_view", "open", repo),
            separator("GitView::sep1"),
            self._menu_item(
                "GitView::working", "Show working changes", "working-changes", repo
            ),
            self._menu_item("GitView::commit", "Commit…", "commit", repo),
            self._menu_item("GitView::log", "Show log", "log", repo),
        ]

        history_paths = selected_files + selected_dirs
        if history_paths:
            items.extend(
                [
                    separator("GitView::sep2"),
                    self._menu_item(
                        "GitView::history",
                        "Show file history",
                        "file-history",
                        repo,
                        history_paths,
                    ),
                ]
            )
        if selected_files:
            items.append(
                self._menu_item(
                    "GitView::diff",
                    "Show diff",
                    "file-diff",
                    repo,
                    selected_files,
                )
            )
        return items

    def get_file_items(self, *args):
        files = args[-1]
        repo, selected_files, selected_dirs = self._repo_context(files)
        if not repo:
            return []
        return self._build_items(repo, selected_files, selected_dirs)

    def get_background_items(self, *args):
        current_folder = args[-1]
        folder_path = normalize_local_path(current_folder.get_location().get_path())
        repo = git_toplevel(folder_path)
        if not repo:
            return []
        return self._build_items(repo, [], [])


def nautilus_python_extension_init():
    return GitViewExtension()


def nautilus_python_extension_types():
    return [GitViewExtension.__gtype__]
