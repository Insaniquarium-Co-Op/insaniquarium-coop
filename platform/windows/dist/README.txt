INSANIQUARIUM CO-OP  -  two players, one tank
=============================================

A fan-made online co-op mod for Insaniquarium! Deluxe. Two people play the
same tank at the same time from their own computers: both can collect coins,
feed, zap aliens, shop and click through the menus. Aliens get tougher and
come more often so two players still feel the chaos.

No game files are included. Each player needs their own Insaniquarium Deluxe
(in their Steam library, including through Steam Family Sharing, or the PopCap
version).


INSTALL (both players)
----------------------
1. Got a zip? Unzip this folder anywhere (the Desktop is fine). Built it yourself
   with Build.bat? It's already done: this is the "Built Game" folder.
2. Double-click InsaniquariumCoop.exe.
   * "Windows protected your PC"?  Click "More info", then "Run anyway".
     (The mod isn't code-signed.)
   * It finds your Steam copy of the game by itself. If it can't, it asks you
     to pick the game's folder once. On Steam: right-click Insaniquarium
     Deluxe > Manage > Browse local files.
   * Your progress from the original game is copied over the first time, if
     it can be found. The mod keeps its own saves (in
     %APPDATA%\PopCap\InsaniquariumCoop), so the original game is untouched.


PLAY TOGETHER
-------------
Player 1 (the host):
   Main menu > Co-op > Host a Game.
   If Windows asks about network access, click "Allow".
   (You can also run "Allow through Windows Firewall.bat" once.)
   Close the Co-op window. Once player 2 is in, a multiplayer panel appears on
   the left of the main menu: "Co-op / Coin Rivals" picks how you play
   Adventure and Time Trial together (Challenge and Virtual Tank are always
   co-op), and "VERSUS!" opens Tank Race, Alien Keeper and Pet Heroes.
   Progress is saved to the host's profile.

On your own: Main menu > Co-op > "Heroes Practice" (Pet Heroes against a bot).

Player 2 (the guest):
   Main menu > Co-op > Join a Game.
   * Same house / same Wi-Fi: click the host's tank in the list.
   * Different houses: type the address shown in the host's Co-op window
     (the host can click "Copy Address" and paste it to you in a chat).
   You can join at any time, even in the middle of a level.


PLAYING FROM DIFFERENT HOUSES
-----------------------------
Look at the host's Co-op window:

* "Internet: 73.x.x.x (your router opened the door automatically)"
     -> player 2 types that address. Done.

* "Tailscale: 100.x.x.x"
     -> player 2 types that address (both must be signed in to Tailscale).

* "Internet: open TCP port 24050 on your router or use Tailscale"
     -> easiest fix: install Tailscale (free) from tailscale.com on BOTH
        computers and sign in (same account, or have the host share their
        PC with player 2). Restart the mod; the host's window then shows a
        "Tailscale: 100.x.x.x" address for player 2 to type.
        (ZeroTier, Radmin VPN and Hamachi work the same way.)
     -> or forward TCP port 24050 on the host's router to the host's PC.


COIN RIVALS (competitive mode)
------------------------------
Host: main menu > multiplayer panel > "Coin Rivals", then start Adventure or
Time Trial. It starts with the next level (or right away if player 2 joins
mid-level).

* Same tank, separate wallets. Your money and egg pieces are shown in the
  panel at the top right (player 1 gold, player 2 cyan).
* Whoever clicks a coin gets it. Whoever clicks a shop button pays.
* Fish you buy are yours (a small square in your color under them). Coins
  from your fish are locked to you for 1.5 seconds (colored corners), then
  anyone can grab them.
* The player who lands the killing shot on an alien owns its treasure.
* Coins picked up by pets go to the owner of the fish that dropped them;
  unowned ones are split 50/50.
* Tank upgrades help both players, but only the buyer pays. Food costs the
  one who drops it.
* Egg pieces cost their normal price. First to buy all 3 wins the level
  (and only a level really finished counts for Adventure).
  In Time Trial the richest player when time runs out wins.
* If player 2 leaves, the host carries on with their own wallet.


TANK RACE (competitive mode)
----------------------------
Each of you plays your OWN tank on the same level. First to buy all 3 egg
pieces wins. Spare money buys sabotage against your rival.

Host: main menu > VERSUS! > Tank Race. Pick Tank 1-4 (it plays as that tank's
final Adventure level) and Catch-up, then "Start Race!". Each player picks 3
pets from all the ones they'd have at that point in Adventure, whatever their
profile has unlocked. Both games start together after a 3-2-1 countdown.
Races never touch anyone's Adventure save.

* Your own pets come along (if you have more than 3, you pick them first).
* Sabotage buttons along the bottom (or keys 1-7):
    1 Alien   an alien warps into their tank (unlocks after 45 s)
    2 Hunger  all their guppies get hungry at once
    3 Murk    their water goes murky for 10 s
    4 Thief   a coin-eating mini alien - click it 3 times to squash it
    5 Hike    their shop and sabotage prices go up 30% for 20 s
    6 Poison  a row of green poison potions falls across their tank; a
              guppy that eats one dies. Click potions to pop them.
              (Unlocks after 45 s, like the alien.)
    7 Block   for 10 s they can't collect any money (coins, diamonds,
              pearls; their pets can't either)
  Prices scale with the level's egg price; each has a cooldown.
* Beat an alien someone sent you, or squash their thief, and you get half
  its price back.
* Catch-up (host's choice): whoever has fewer egg pieces pays 40% less for
  sabotage.
* Right of the sabotage buttons: your rival's money, egg pieces and fish.
  Top right: a faint live mini-map of their tank (aliens flash purple).
  Hold Tab to see it clearly.
* If all your fish die, or you leave the level, your rival wins.


ALIEN KEEPER (new in 1.5)
-------------------------
One of you keeps the fish; the other raises aliens in their own alien tank and
sends them through a portal to invade. Host: main menu > VERSUS! > Alien
Keeper, pick a tank, "Start Round!". Roles swap every round (host keeps the
fish first).

* Fish keeper: play the tank's final Adventure level. Every alien is your
  friend's: a crosshair and siren show where each one will land. You start
  with two extra guppies (a breeder on Tank 4) and 45 seconds before the
  portal opens. Win by finishing the egg or holding out 8 minutes.
* Alien keeper: your lair is the aliens' home in space.
    - Buy babies in the top bar (more kinds on later tanks).
    - Click the lair to drop space goo (5 each): babies grow into juveniles and
      adults as they eat. Hungry aliens that aren't fed die.
    - Grown aliens drop crystals: click them for money. Fish your aliens eat in
      the fish tank pay a bounty too.
    - Click grown aliens to select them (A: all), optionally buy Shield (can't
      be hurt for 5 s after landing) or Frenzy (faster for 8 s; one power-up
      per alien), then SEND or Space (a portal fee each). One at a time or a
      whole wave (the portal lets one through every 3 s). Every alien comes
      through shielded for 1 s. Juveniles land at about half strength, adults
      at full.
    - Pick where they land on the mini-map (bottom right), or hold Tab to watch
      the fish tank live and click there; hold the mouse to make your aliens
      hunt there.
    - Blackout darkens the fish keeper's screen for 6 s but a flashlight.
    - You win by wiping out every fish; you lose with no aliens left anywhere
      and no money for a baby. Esc twice gives up the round.


PET HEROES (new in 2.0)
-----------------------
A MOBA on two tanks. Each of you keeps your own tank (twice the usual size)
and plays one pet as a hero: Itchy (assassin), Clyde (mage), Rhubarb (tank),
Angie (support) or Stinky (farmer). Break your friend's treasure-chest core
to win. Host: VERSUS! > Pet Heroes > Start Match!, then both pick a hero and
"Lock in!". Practice against the bot from the Co-op window. Hold H in a match
for the controls.

* Right-click: move or attack (hold to keep moving). Q W E R: abilities at
  the mouse (R at level 5). Ctrl+Q/W/E: pick your next upgrade.
* Left-click in your own tank: coins, food ($5), laser. 1-4: quick-buy
  (guppy, more food, breeder, carnivore). B: the shop (items, towers,
  minions). Tab (hold): look at your tank while your hero is away.
* The portal (top middle) goes to your friend's tank and back; Stinky and
  Rhubarb ride its beam from the floor. The corner floor pads go there too.
  After crossing you can't cross back for 8 s.
* Every 30 s mini Sylvesters come through each portal at the other's two
  towers (giant clams). Break both towers, then the core. Towers shrug off a
  hero unless that hero's minions are close.
* You're stronger in your own tank; kelp hides you. Sudden death at 15:00.


CONTROLS & CO-OP EXTRAS
-----------------------
* Everything works like the original game, for both players.
* Middle-click in the tank: drop a "look here!" ping your partner sees.
* You see your partner's cursor with their name (player 1 gold, player 2 cyan).
* Player 2: press Esc for the co-op menu (Keep Playing / Options / Leave Game).
* Host: Options > Co-op (in a level) shows who's connected and the ping, and
  lets you change the difficulty or remove player 2.
* Win a level and you get a co-op report: coins collected, alien hits and
  feedings per player, plus a Tank MVP.
* The game no longer pauses when the host clicks into another window, and it
  keeps running if the host minimizes it, since player 2 is still playing.


DIFFICULTY (host picks it in the Co-op window)
----------------------------------------------
                   Alien    Time between   Surprise       Fish     Egg
                   health   invasions      2nd alien      hunger   price
  Classic          x1.0     x1.0           never          normal   normal
  Double Trouble   x1.6     x0.8           25% of waves   +15%     normal
  Insane           x2.2     x0.6           55% of waves   +35%     +20%

Scaling only applies while a second player is connected. Bosses and the
scripted double-alien waves are never doubled up.


TROUBLESHOOTING
---------------
* "Couldn't reach the host": check the address; make sure the host clicked
  "Host a Game" and allowed the program through the firewall (run
  "Allow through Windows Firewall.bat"). Across the internet, see above.
* "An attempt was made to access a socket in a way forbidden by its access
  permissions": something on the JOINING computer blocks the connection,
  usually antivirus software or a VPN. Allow InsaniquariumCoop.exe in the
  antivirus, or switch the VPN off while playing.
* "Version mismatch": both players need the same version of this mod.
* Choppy picture for player 2: the stream needs very little bandwidth
  (usually well under 0.5 Mbit/s), so it's normally Wi-Fi trouble or a
  far-away VPN relay. The ping shows in player 2's Esc menu.
* A log is kept in %APPDATA%\PopCap\InsaniquariumCoop\coop_log.txt
* Command line options: -windowed  -host  -join=<address>
                        -resdir=<game folder>  -savedir=<folder>


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
