#!/bin/bash
# Writes patches/winfish.patch and patches/pvz-portable.patch: this repository's WinFish/
# and PvZ-Portable/ compared with the pinned upstream (scripts/upstream.lock). Run it
# after changing anything in either folder.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/scripts/upstream.lock"
U="$ROOT/build-upstream/insaniquarium-mac"
if [ ! -d "$U/.git" ] || [ "$(git -C "$U" rev-parse HEAD 2>/dev/null)" != "$UPSTREAM_COMMIT" ]; then
  rm -rf "$U"; mkdir -p "$U"
  git -C "$U" init -q
  git -C "$U" config core.autocrlf false
  git -C "$U" fetch -q --depth 1 "$UPSTREAM_REPO" "$UPSTREAM_COMMIT"
  git -C "$U" -c advice.detachedHead=false checkout -q FETCH_HEAD
fi
mkdir -p "$ROOT/patches"
make_patch() { # folder patch
  local T="$ROOT/build-upstream/diff"; rm -rf "$T"; mkdir -p "$T/a" "$T/b"
  cp -R "$U/$1" "$T/a/$1"; cp -R "$ROOT/$1" "$T/b/$1"
  rm -f "$T/b/$1/.fetched"
  # PopCap's icon is only in upstream; the build never uses it, so the patch leaves it be.
  rm -f "$T/a/$1/source/WinFish/Insaniquarium.ico"
  (cd "$T" && git diff --no-index --no-color "a/$1" "b/$1" || true) > "$ROOT/patches/$2"
  python3 - "$ROOT/patches/$2" "$1" <<'PY'
import sys
p, d = sys.argv[1], sys.argv[2]
s = open(p).read().replace(" a/a/" + d, " a/" + d).replace(" b/b/" + d, " b/" + d)
open(p, "w").write(s)
PY
  rm -rf "$T"
  echo "wrote patches/$2 ($(grep -c '^diff --git' "$ROOT/patches/$2") files, $(wc -l < "$ROOT/patches/$2" | tr -d ' ') lines)"
}
make_patch WinFish winfish.patch
make_patch PvZ-Portable pvz-portable.patch
