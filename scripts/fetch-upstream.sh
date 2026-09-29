#!/bin/bash
# Puts the game's source (WinFish/) and its framework (PvZ-Portable/) in place: downloads
# the commit pinned in scripts/upstream.lock and applies patches/winfish.patch and
# patches/pvz-portable.patch. Safe to re-run: a folder is redone only when the lock or
# its patch changed. A folder without the .fetched marker (a developer copy) is left
# alone. Needs git.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/scripts/upstream.lock"
if [ "$(uname -s)" = Darwin ]; then SUM="shasum -a 256"; else SUM="sha256sum"; fi
U="$ROOT/build-upstream/insaniquarium-mac"

download() {
  [ -d "$U/.git" ] && [ "$(git -C "$U" rev-parse HEAD 2>/dev/null)" = "$UPSTREAM_COMMIT" ] && return
  command -v git >/dev/null || { echo "Needs git (Mac: xcode-select --install)"; exit 1; }
  echo "Downloading $UPSTREAM_REPO at ${UPSTREAM_COMMIT:0:7}..."
  rm -rf "$U"; mkdir -p "$U"
  git -C "$U" init -q
  git -C "$U" config core.autocrlf false
  git -C "$U" fetch -q --depth 1 "$UPSTREAM_REPO" "$UPSTREAM_COMMIT"
  git -C "$U" -c advice.detachedHead=false checkout -q FETCH_HEAD
  # git checks the commit's content hash, so a changed upstream can't slip in.
  [ "$(git -C "$U" rev-parse HEAD)" = "$UPSTREAM_COMMIT" ] || { echo "Unexpected upstream commit"; exit 1; }
}

fetch_dir() { # folder patch
  local D="$ROOT/$1" PATCH="$ROOT/patches/$2"
  local STAMP="$UPSTREAM_COMMIT $($SUM "$PATCH" | cut -d' ' -f1)"
  if [ -d "$D" ] && [ ! -f "$D/.fetched" ]; then echo "$1/: developer copy, left as it is"; return; fi
  if [ -f "$D/.fetched" ] && [ "$(cat "$D/.fetched")" = "$STAMP" ]; then echo "$1/: up to date"; return; fi
  download
  rm -rf "$D.new"; cp -R "$U/$1" "$D.new"
  (cd "$ROOT" && git apply --whitespace=nowarn --directory="$1.new" -p2 "$PATCH") \
    || { echo "patches/$2 didn't apply"; rm -rf "$D.new"; exit 1; }
  rm -f "$D.new/source/WinFish/Insaniquarium.ico"		# PopCap's icon; the build uses ours
  rm -rf "$D"; mv "$D.new" "$D"
  echo "$STAMP" > "$D/.fetched"
  echo "$1/: ready (patched with patches/$2)"
}

fetch_dir WinFish winfish.patch
fetch_dir PvZ-Portable pvz-portable.patch
