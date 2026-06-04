#!/usr/bin/env bash
# Creates a small repository with branches and a merge commit for manual testing.
set -euo pipefail

TARGET="${1:-/tmp/git_view-fixture}"
rm -rf "$TARGET"
mkdir -p "$TARGET"
cd "$TARGET"

git init -b main
git config user.email "dev@git-view.local"
git config user.name "git_view fixture"

echo "main line 1" > README.md
git add README.md
git commit -m "Initial commit on main"

git checkout -b feature
echo "feature work" >> README.md
git add README.md
git commit -m "Feature commit"

git checkout main
echo "main line 2" >> README.md
git add README.md
git commit -m "Main moves forward"

git merge feature --no-ff -m "Merge branch 'feature' into main"

git checkout -b release
echo "release prep" >> README.md
git add README.md
git commit -m "Release branch commit"

git checkout main

echo "Fixture repository created at: $TARGET"
git log --oneline --graph --all
