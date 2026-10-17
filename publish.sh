#!/usr/bin/env bash
# Publishes this folder to a new public GitHub repo.  Usage: ./publish.sh [repo-name]
set -e
NAME="${1:-afo-q-4}"
command -v git >/dev/null || { echo "Install git first: https://git-scm.com/downloads"; exit 1; }
[ -d .git ] || git init -b main
git add -A
git commit -m "Afo-Q 4" || true
if command -v gh >/dev/null; then
  gh auth status >/dev/null 2>&1 || gh auth login
  gh repo create "$NAME" --public --source . --remote origin --push
else
  echo
  echo "GitHub CLI (gh) not found. Do this instead:"
  echo "  1. Create an EMPTY public repo named $NAME on github.com"
  echo "  2. git remote add origin https://github.com/<your-username>/$NAME.git"
  echo "  3. git push -u origin main"
fi
echo
echo "Next: Settings > Pages > Source = GitHub Actions   (one time, publishes the web version)"
echo "Then release downloads:  git tag v0.1.0 && git push origin v0.1.0"
