#!/bin/bash
# Builds Insaniquarium Co-op for macOS (Apple Silicon) with Homebrew's libraries.
#
#   scripts/build-macos.sh                 build + package dist/InsaniquariumCoop-<ver>-macos-arm64.zip
#   scripts/build-macos.sh --no-package    just build build-macos/InsaniquariumCoop.app (fast dev loop)
#   scripts/build-macos.sh --run [args]    build, then run it (e.g. --run -windowed)
#
# Needs the Xcode Command Line Tools and Homebrew (scripts/setup-macos.sh sets
# both up). The first run installs the Homebrew packages below; later runs only
# recompile what changed.
#
# Packaging copies every Homebrew library the game uses into the app
# (Contents/Frameworks, via dylibbundler), signs it ad hoc, and zips it with
# "Get Game Files.command" and a README. The app then runs on Macs without
# Homebrew, on this macOS version or newer (set MACOSX_DEPLOYMENT_TARGET to
# claim an older one only if Homebrew's libraries support it).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$ROOT/build-macos"
MODE=package
RUN_ARGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --no-package) MODE=build; shift ;;
    --run) MODE=run; shift; RUN_ARGS=("$@"); break ;;
    -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
    *) echo "unknown option: $1"; exit 1 ;;
  esac
done

[ "$(uname -s)" = Darwin ] || { echo "This builds the Mac app; run it on a Mac."; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Install the Xcode Command Line Tools first:  xcode-select --install"; exit 1; }
command -v brew >/dev/null || { echo "Homebrew is needed (https://brew.sh), or run scripts/setup-macos.sh."; exit 1; }

"$ROOT/scripts/fetch-upstream.sh"		# the game's source and framework (skipped for developer copies)

BREW_DEPS="cmake ninja pkg-config sdl2 libpng jpeg-turbo libogg libvorbis libopenmpt miniupnpc dylibbundler"
MISSING=""
for p in $BREW_DEPS; do brew list --formula "$p" >/dev/null 2>&1 || MISSING="$MISSING $p"; done
if [ -n "$MISSING" ]; then
  echo "Installing Homebrew packages:$MISSING"
  brew install $MISSING
fi
PREFIX="$(brew --prefix)"
MIN="${MACOSX_DEPLOYMENT_TARGET:-$(sw_vers -productVersion | cut -d. -f1).0}"

cmake -G Ninja -S "$ROOT" -B "$B" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN" \
  -DCMAKE_PREFIX_PATH="$PREFIX;$PREFIX/opt/jpeg-turbo;$PREFIX/opt/libpng;$PREFIX/opt/sdl2" > "$B.configure.log" 2>&1 \
  || { tail -30 "$B.configure.log"; exit 1; }
ninja -C "$B"
APP="$B/InsaniquariumCoop.app"

if [ "$MODE" = build ]; then
  echo "Built $APP"
  exit 0
fi
if [ "$MODE" = run ]; then
  exec "$APP/Contents/MacOS/InsaniquariumCoop" "${RUN_ARGS[@]+"${RUN_ARGS[@]}"}"
fi

VER="$(tr -d '[:space:]' < "$ROOT/COOP_VERSION")"
NAME="Insaniquarium Co-op"
STAGE="$B/package/$NAME"
rm -rf "$B/package"; mkdir -p "$STAGE"
OUT_APP="$STAGE/$NAME.app"
cp -R "$APP" "$OUT_APP"
EXE="$OUT_APP/Contents/MacOS/InsaniquariumCoop"

echo "Bundling libraries..."
dylibbundler -of -cd -b -x "$EXE" -d "$OUT_APP/Contents/Frameworks" -p @executable_path/../Frameworks/ -s "$PREFIX/lib" > "$B/package.log" 2>&1 \
  || { tail -30 "$B/package.log"; exit 1; }
# Homebrew's SDL2 is sdl2-compat, which dlopens SDL3 (dylibbundler can't see that);
# it looks for libSDL3.dylib next to itself first.
if [ -f "$OUT_APP/Contents/Frameworks/libSDL2-2.0.0.dylib" ] \
   && strings "$OUT_APP/Contents/Frameworks/libSDL2-2.0.0.dylib" | grep -q '@loader_path/libSDL3.dylib'; then
  cp "$PREFIX/lib/libSDL3.0.dylib" "$OUT_APP/Contents/Frameworks/libSDL3.dylib"
  chmod u+w "$OUT_APP/Contents/Frameworks/libSDL3.dylib"
  install_name_tool -id @loader_path/libSDL3.dylib "$OUT_APP/Contents/Frameworks/libSDL3.dylib" 2>/dev/null
fi
# Nothing may still point at Homebrew, or the app only works on this Mac.
LEFT=$( { otool -L "$EXE"; for f in "$OUT_APP/Contents/Frameworks/"*.dylib; do otool -L "$f"; done; } | grep -E "^[[:space:]]+($PREFIX|/usr/local)/" || true)
if [ -n "$LEFT" ]; then
  echo "These libraries still point outside the app:"; echo "$LEFT"; exit 1
fi
codesign --force --deep --sign - "$OUT_APP"
codesign --verify --deep --strict "$OUT_APP"

cp "$ROOT/scripts/get-game-files.sh" "$STAGE/Get Game Files.command"
chmod +x "$STAGE/Get Game Files.command"
cp "$ROOT/platform/macos/dist/README.txt" "$STAGE/README.txt"
cp "$ROOT/THIRD_PARTY_NOTICES.txt" "$STAGE/"
cp "$ROOT/LICENSE" "$STAGE/LICENSE.txt"
mkdir -p "$ROOT/dist"
ZIP="$ROOT/dist/InsaniquariumCoop-$VER-macos-$(uname -m).zip"
rm -f "$ZIP"
(cd "$B/package" && ditto -c -k --keepParent "$NAME" "$ZIP")
echo "Packaged $ZIP (runs on macOS $MIN or newer)"
