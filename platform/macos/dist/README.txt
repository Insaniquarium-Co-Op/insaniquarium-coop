INSANIQUARIUM CO-OP FOR MAC  -  two players, one tank (or two!)
===============================================================

A fan-made online multiplayer mod for Insaniquarium! Deluxe, running natively
on Apple Silicon Macs (M1 or newer) with macOS 14 Sonoma or newer. Plays with the Windows version too: a Mac
and a PC can share a tank or race each other.

No game files are included. You need Insaniquarium Deluxe in your own Steam
library; the "Get Game Files" helper downloads it for you.


FIRST TIME
----------
1. Drag "Insaniquarium Co-op.app" into your Applications folder.

2. Double-click "Get Game Files.command".
   * macOS may say it can't check the file for malicious software, because
     this fan project isn't registered with Apple. Open System Settings >
     Privacy & Security, scroll down and click "Open Anyway" (or right-click
     the file > Open). Same for the app the first time you open it.
   * A Terminal window downloads the game's files from Steam with your
     account. Scan the QR code it shows with the Steam app on your phone
     (Steam Guard > Scan a QR code) right away: Steam cancels a sign-in that
     takes more than about a minute, and the helper then tries again with a
     new code. The download itself takes under a minute.
   * If Steam already put the game on this Mac, the helper copies it from
     there instead, with no sign-in.
   * The helper can only download games your own Steam account owns (not
     ones borrowed through Family Sharing). If you own the game on a PC, you
     can copy its folder from there (Steam: right-click the game > Manage >
     Browse local files) into
     ~/Library/Application Support/InsaniquariumCoop/Game
     or pick that folder when the game asks.

3. Open Insaniquarium Co-op. If you played the insaniquarium-mac port, your progress
   is copied over the first time. Your saves live in
   ~/Library/Application Support/PopCap/InsaniquariumCoop


PLAYING TOGETHER
----------------
Everything works the same as on Windows: Main menu > Co-op > Host a Game or
Join a Game. Once connected, the host's main menu shows a multiplayer panel:
Co-op or Coin Rivals for Adventure and Time Trial, and VERSUS! for Tank Race,
Alien Keeper and Pet Heroes (the MOBA: practice it against the bot from
Co-op > "Heroes Practice", where "Tutorial" teaches it step by step; hold H
in a match for the controls).
See the README on GitHub for the full rules.

* The first time you host, macOS may ask to let the app accept incoming
  network connections, and newer macOS versions ask about "devices on your
  local network". Allow both, or your partner can't find or join your game.
* Macs have no middle mouse button for the "look here!" ping: use a mouse
  with one, or skip it.


TROUBLESHOOTING
---------------
* "Couldn't reach the host": check the address and that the host allowed the
  network prompts above. Across the internet, see the README (port 24050,
  or Tailscale).
* Nobody can find or join you: if the Co-op window says a VPN is on, switch
  it off while playing.
* "Version mismatch": both players need the same version of this mod.
* A log is kept in ~/Library/Application Support/PopCap/InsaniquariumCoop/coop_log.txt


UPDATES
-------
Both players need the same version. If you join a friend whose version is
different but still compatible, a note says who should update. Once per launch
the game can also check online for a newer version and offer to open the
download page; it never downloads or installs anything itself. Turn this off
in the Co-op window ("Update Check: On/Off").


Unofficial fan project: not affiliated with or endorsed by PopCap Games or
Electronic Arts. Insaniquarium is a trademark of Electronic Arts Inc.
Insaniquarium! Deluxe (c) PopCap Games / Electronic Arts. This mod contains no
game files: every player needs their own copy of the game.
The app icon is original art made for this mod.
Built on the WinFish decompilation, the insaniquarium-mac port and
PvZ-Portable's SexyAppFramework. See THIRD_PARTY_NOTICES.txt.
