#!/bin/bash
# Rebuilds the app icons from platform/icon/icon.swift (original art; macOS only):
#   platform/macos/InsaniquariumCoop.icns and platform/windows/InsaniquariumCoop.ico
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
W="$ROOT/build-icon"
mkdir -p "$W/InsaniquariumCoop.iconset"
swiftc -O "$ROOT/platform/icon/icon.swift" -o "$W/icon"
# Each size is drawn directly (sharper than scaling one big image down).
for s in 16 24 32 48 64 128 256 512 1024; do "$W/icon" "$W/icon-$s.png" $s; done
for s in 16 32 128 256 512; do
  cp "$W/icon-$s.png" "$W/InsaniquariumCoop.iconset/icon_${s}x${s}.png"
  cp "$W/icon-$((s * 2)).png" "$W/InsaniquariumCoop.iconset/icon_${s}x${s}@2x.png"
done
iconutil -c icns "$W/InsaniquariumCoop.iconset" -o "$ROOT/platform/macos/InsaniquariumCoop.icns"
# .ico with PNG images inside (Windows Vista and later read these).
python3 - "$W" "$ROOT/platform/windows/InsaniquariumCoop.ico" <<'PY'
import struct, sys
w, out = sys.argv[1], sys.argv[2]
sizes = [16, 24, 32, 48, 64, 128, 256]
pngs = [open("%s/icon-%d.png" % (w, s), "rb").read() for s in sizes]
head = struct.pack("<HHH", 0, 1, len(sizes))
offset = 6 + 16 * len(sizes)
dirs, data = b"", b""
for s, p in zip(sizes, pngs):
    dirs += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(p), offset + len(data))
    data += p
open(out, "wb").write(head + dirs + data)
PY
echo "icons written"
