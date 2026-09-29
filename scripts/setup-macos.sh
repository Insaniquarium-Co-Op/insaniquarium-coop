#!/bin/bash
# First-time setup on a Mac (Apple Silicon) after cloning the repo: everything
# needed to play and develop Insaniquarium Co-op natively.
#
#   scripts/setup-macos.sh [--no-install]
#
# 1. Xcode Command Line Tools and Homebrew (asks before installing either)
# 2. Your game files from Steam (scripts/get-game-files.sh; asks you to log in)
# 3. Builds and packages the app (scripts/build-macos.sh), about 5-10 minutes
#    the first time, mostly Homebrew downloads
# 4. Copies "Insaniquarium Co-op.app" to /Applications (skip with --no-install)
#
# Safe to run again: finished steps are skipped. After this, the day-to-day
# loop is  scripts/build-macos.sh --run -windowed  (rebuilds only what changed).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INSTALL=1
[ "${1:-}" = "--no-install" ] && INSTALL=0
[ "$(uname -s)" = Darwin ] || { echo "This is the Mac setup. On Linux use scripts/setup-test-env.sh."; exit 1; }
[ "$(uname -m)" = arm64 ] || echo "Note: this Mac isn't Apple Silicon; the build will be for $(uname -m)."

echo "== 1/4 Developer tools"
if ! xcode-select -p >/dev/null 2>&1; then
  echo "Installing the Xcode Command Line Tools: click Install in the window that opens,"
  echo "wait for it to finish, then run this script again."
  xcode-select --install || true
  exit 1
fi
if ! command -v brew >/dev/null; then
  for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do [ -x "$b" ] && eval "$("$b" shellenv)" && break; done
fi
if ! command -v brew >/dev/null; then
  read -r -p "Homebrew (https://brew.sh) is needed to build. Install it now? [y/N] " a
  case "$a" in y|Y) /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
                    eval "$(/opt/homebrew/bin/brew shellenv)" ;;
               *) echo "Install Homebrew, then run this again."; exit 1 ;; esac
fi

echo "== 2/4 Game files"
"$ROOT/scripts/get-game-files.sh"

echo "== 3/4 Build"
"$ROOT/scripts/build-macos.sh"

if [ "$INSTALL" = 1 ]; then
  echo "== 4/4 Install"
  SRC="$ROOT/build-macos/package/Insaniquarium Co-op/Insaniquarium Co-op.app"
  DEST="/Applications/Insaniquarium Co-op.app"
  rm -rf "$DEST"
  cp -R "$SRC" "$DEST"
  echo "Installed $DEST"
fi
echo
echo "Done. Open Insaniquarium Co-op from Applications (or Launchpad)."
echo "For development:  scripts/build-macos.sh --run -windowed"
