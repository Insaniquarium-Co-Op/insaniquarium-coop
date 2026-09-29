#!/bin/bash
# Gets your own copy of Insaniquarium Deluxe's game files (Steam app 3320) onto
# this Mac or Linux machine, so Insaniquarium Co-op can run here.
#
#   scripts/get-game-files.sh [--dest <folder>] [--user <steam login>] [--qr] [--download]
#
# 1. If the files are already in the destination, there's nothing to do.
# 2. If Steam installed the game here (any Steam library), they're copied over.
# 3. Otherwise they're downloaded from Steam with DepotDownloader
#    (github.com/SteamRE/DepotDownloader), using your Steam login and license.
#    Steam's own app refuses to install this Windows-only game on a Mac; this
#    downloads the same files directly. You log in either by scanning a QR code
#    with the Steam mobile app (--qr, the default) or with your username and
#    password (--user <name>; Steam Guard asks you to approve on your phone or
#    for a code). Nothing about your login is stored by this script.
#    --download skips steps 1 and 2 and always downloads.
#
# The destination defaults to the folder the game looks in first:
#   macOS: ~/Library/Application Support/InsaniquariumCoop/Game
#   Linux: ~/.local/share/InsaniquariumCoop/Game
# Only the game's data is kept (images, sounds, music, fishsongs, data, properties,
# about 11 MB); the Windows program itself isn't needed.
#
# This is also shipped next to the Mac app as "Get Game Files.command".
set -euo pipefail

APP_ID=3320
DD_VERSION_URL=https://github.com/SteamRE/DepotDownloader/releases/latest/download
DATA_DIRS="images sounds music fishsongs data properties"

case "$(uname -s)" in
  Darwin) OS=mac; DEFAULT_DEST="$HOME/Library/Application Support/InsaniquariumCoop/Game"; CACHE="$HOME/Library/Caches/InsaniquariumCoop" ;;
  Linux)  OS=linux; DEFAULT_DEST="${XDG_DATA_HOME:-$HOME/.local/share}/InsaniquariumCoop/Game"; CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/InsaniquariumCoop" ;;
  *) echo "This script is for macOS and Linux. On Windows, install the game with Steam."; exit 1 ;;
esac

DEST="$DEFAULT_DEST"
LOGIN=qr
STEAM_USER=""
FORCE_DOWNLOAD=0
while [ $# -gt 0 ]; do
  case "$1" in
    --dest) DEST="$2"; shift 2 ;;
    --user) LOGIN=user; STEAM_USER="$2"; shift 2 ;;
    --qr) LOGIN=qr; shift ;;
    --download) FORCE_DOWNLOAD=1; shift ;;
    -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
    *) echo "unknown option: $1 (see --help)"; exit 1 ;;
  esac
done

is_game_dir() { [ -f "$1/properties/resources.xml" ] && [ -d "$1/images" ] && [ -d "$1/sounds" ]; }

copy_data() { # src dest
  mkdir -p "$2"
  for d in $DATA_DIRS; do
    if [ -d "$1/$d" ]; then
      rm -rf "${2:?}/$d"
      cp -R "$1/$d" "$2/$d"
    fi
  done
}

finish() {
  echo
  echo "Game files are ready in:"
  echo "  $DEST"
  if [ "$OS" = mac ] && [ "$DEST" = "$DEFAULT_DEST" ]; then
    echo "Open Insaniquarium Co-op and it will find them."
  elif [ "$OS" = mac ]; then
    echo "Pick this folder when Insaniquarium Co-op asks for the game, or move it to:"
    echo "  $DEFAULT_DEST"
  else
    echo "Use them with:  INSANIQ_RESDIR=\"$DEST/\" build-linux/InsaniquariumCoop -windowed"
    echo "            or: GAME=\"$DEST\" scripts/setup-test-env.sh"
  fi
  exit 0
}

if [ "$FORCE_DOWNLOAD" = 0 ] && is_game_dir "$DEST"; then
  echo "Already there: $DEST"
  finish
fi

# ---- 2. An existing Steam install on this machine ---------------------------
STEAM_ROOTS=()
if [ "$OS" = mac ]; then
  STEAM_ROOTS+=("$HOME/Library/Application Support/Steam")
else
  STEAM_ROOTS+=("$HOME/.steam/steam" "$HOME/.local/share/Steam" "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam")
fi
if [ "$FORCE_DOWNLOAD" = 1 ]; then STEAM_ROOTS=(); fi
for root in "${STEAM_ROOTS[@]+"${STEAM_ROOTS[@]}"}"; do
  libs=("$root")
  vdf="$root/steamapps/libraryfolders.vdf"
  if [ -f "$vdf" ]; then
    while IFS= read -r p; do libs+=("$p"); done < <(sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)".*/\1/p' "$vdf")
  fi
  for lib in "${libs[@]}"; do
    cand="$lib/steamapps/common/Insaniquarium Deluxe"
    if is_game_dir "$cand"; then
      echo "Found Steam's copy: $cand"
      copy_data "$cand" "$DEST"
      is_game_dir "$DEST" && finish
    fi
  done
done

# ---- 3. Download from Steam with DepotDownloader ----------------------------
if [ "$OS" = mac ]; then
  [ "$(uname -m)" = arm64 ] && DD_ASSET=DepotDownloader-macos-arm64.zip || DD_ASSET=DepotDownloader-macos-x64.zip
else
  [ "$(uname -m)" = aarch64 ] && DD_ASSET=DepotDownloader-linux-arm64.zip || DD_ASSET=DepotDownloader-linux-x64.zip
fi
DD_DIR="$CACHE/DepotDownloader"
DD="$DD_DIR/DepotDownloader"
if [ ! -x "$DD" ]; then
  echo "Fetching DepotDownloader (Steam content downloader, about 30 MB)..."
  mkdir -p "$DD_DIR"
  curl -fsSL --retry 3 -o "$DD_DIR/dd.zip" "$DD_VERSION_URL/$DD_ASSET"
  (cd "$DD_DIR" && unzip -o -q dd.zip && rm dd.zip)
  chmod +x "$DD"
fi

echo
echo "Downloading Insaniquarium Deluxe from Steam (app $APP_ID) with your account."
echo "The game must be in your Steam library (bought on Steam)."
if [ "$LOGIN" = qr ]; then
  echo "A QR code will appear: open the Steam app on your phone, tap the shield"
  echo "(Steam Guard) > 'Scan a QR code', and scan it within a minute."
  LOGIN_ARGS=(-qr)
else
  echo "Log in as '$STEAM_USER'. Steam Guard will ask you to approve on your phone or for a code."
  LOGIN_ARGS=(-username "$STEAM_USER")
fi
echo

WORK="$CACHE/download"
LOG="$CACHE/download.log"
# About a minute into a sign-in, Steam often moves DepotDownloader to another server,
# which kills a QR or password sign-in still in progress; a fresh attempt usually works.
attempt=1
while :; do
  rm -rf "$WORK"; mkdir -p "$WORK"
  set +e
  "$DD" -app "$APP_ID" -os windows -dir "$WORK" "${LOGIN_ARGS[@]}" 2>&1 | tee "$LOG"
  rc=${PIPESTATUS[0]}
  set -e
  FOUND=$(find "$WORK" -maxdepth 3 -type f -path "*/properties/resources.xml" | head -1)
  [ -n "$FOUND" ] && break
  if grep -qE "Failed to authenticate with Steam|Unable to get steam3 credentials" "$LOG" && [ "$attempt" -lt 3 ]; then
    attempt=$((attempt + 1))
    echo
    echo "Steam dropped the sign-in before it finished. Trying again ($attempt of 3)"
    echo "with a new code: scan it as soon as it appears."
    echo
    continue
  fi
  echo
  echo "The download didn't produce the game files (DepotDownloader exit code $rc)."
  if grep -q "is not available from this account" "$LOG"; then
    echo "Signed in, but this Steam account doesn't own Insaniquarium Deluxe. Games borrowed"
    echo "through Steam Family Sharing can't be downloaded this way. If you own the game on"
    echo "another account or on a PC, sign in with that account, or copy the folder from"
    echo "your own PC (Steam: right-click the game > Manage > Browse local files) to:"
    echo "  $DEST"
  elif grep -qE "Failed to authenticate with Steam|Unable to get steam3 credentials" "$LOG"; then
    echo "Steam sign-in didn't complete (it was cancelled, timed out, or Steam dropped the"
    echo "connection). Run this again and scan the code right away, or sign in with your"
    echo "username instead:  $0 --user <steam login>"
  else
    echo "Check your internet connection and run this again. DepotDownloader's output is in:"
    echo "  $LOG"
  fi
  exit 1
done
SRC=$(dirname "$(dirname "$FOUND")")
copy_data "$SRC" "$DEST"
rm -rf "$WORK"
is_game_dir "$DEST" || { echo "Copy to $DEST failed."; exit 1; }
finish
