# Insaniquarium Co-op

*An unofficial fan mod for Insaniquarium! Deluxe: play online with a friend, on Windows
and Apple Silicon Macs (a Mac and a PC can play together).*

> **Not affiliated with or endorsed by PopCap Games or Electronic Arts.** Insaniquarium
> is a trademark of Electronic Arts Inc. This project contains **no game files** and no
> PopCap code: each player needs their own copy of
> [Insaniquarium Deluxe](https://store.steampowered.com/app/3320/Insaniquarium_Deluxe/)
> (or the PopCap release), and the build downloads the community decompilation it is
> based on straight from its public repository.

Two people play **the same tank at the same time**, each on their own computer, with
the chaos retuned for two (tougher, more frequent aliens, hungrier fish). Or compete:

- **Coin Rivals**: same tank, separate wallets; first to buy all 3 egg pieces wins.
- **Tank Race**: a tank each, sabotage your friend (murky water, coin thieves, aliens,
  poison raids...), first egg wins.
- **Alien Keeper**: one keeps the fish, the other raises aliens in a lair tank and
  sends them in; roles swap.
- **Pet Heroes**: a MOBA on two tanks. Play a pet as a hero, farm your tank, push
  minion waves and break your friend's core. Practice against a bot on your own.

---

## Install (build it yourself, about 20 minutes the first time)

The mod is shared as source code, and a script builds it for you. Both players need
the **same version**.

### Windows 10 or 11 (64-bit)

1. Get the code: on this page click **Code → Download ZIP** and unzip it somewhere
   with a short path, like `C:\Games\InsaniquariumCoop` (or `git clone` it).
2. Double-click **`Build.bat`**. The first time it downloads about 250 MB of build
   tools (a compiler, CMake, Ninja and a small Git) into
   `%LOCALAPPDATA%\InsaniquariumCoop\BuildTools`, from their official releases, each
   checked against a pinned SHA-256. Nothing is installed on your system and no admin
   rights are needed. It then downloads the game's source (the decompilation), applies
   this mod's changes and builds everything: 15-25 minutes, then a minute or two for
   later builds.
3. Open the **`Built Game`** folder and double-click **`InsaniquariumCoop.exe`**. It
   finds your Steam copy of the game by itself, or asks you to pick the game's folder
   once (Steam: right-click Insaniquarium Deluxe → Manage → Browse local files). Your
   progress from the original game is copied over the first time; the mod keeps its
   own saves, so the original game is never touched.

### Mac (Apple Silicon, macOS 14 or newer)

In Terminal:

```bash
xcode-select --install
```

```bash
git clone https://github.com/Insaniquarium-Co-Op/insaniquarium-coop.git ~/InsaniquariumCoop
```

```bash
~/InsaniquariumCoop/scripts/setup-macos.sh
```

The setup installs Homebrew if needed, builds the app, puts it in Applications and
fetches the game's files from Steam with **your own** Steam account (scan the QR code
with the Steam phone app). If you own the game on a PC instead, copy its folder into
`~/Library/Application Support/InsaniquariumCoop/Game`.

### Updating

The game tells you when a newer version is out (you can turn that off in the Co-op
window). To update: on Windows download the new ZIP (or `git pull`) and run
`Build.bat` again; on a Mac run `git -C ~/InsaniquariumCoop pull` and
`~/InsaniquariumCoop/scripts/build-macos.sh`. If you and your friend run different
versions, the game says who should update.

---

## Playing
### Start a game

**Player 1 (host)**: on the main menu click **Co-op → Host a Game**. Windows may
ask whether to allow network access: click **Allow**. (Or run
`Allow through Windows Firewall.bat` once.) Then close the Co-op window. Once
player 2 is in, a **multiplayer panel** appears on the left of the main menu:
- **Co-op / Coin Rivals** picks how you play **Adventure** and **Time Trial**
  together (Challenge and Virtual Tank are always co-op).
- **VERSUS!** opens the competitive modes: **Tank Race**, **Alien Keeper** and
  **Pet Heroes**.

On your own? **Co-op → Heroes Practice** plays Pet Heroes against the bot.

**Player 2 (guest)**: click **Co-op → Join a Game**.
- **Same house / same Wi-Fi:** the host's tank shows up in the list. Click it.
- **Different houses:** type the address shown in the host's Co-op window (the
  host can press **Copy Address** and paste it to you in a chat app).

Player 2 can join at any time, even in the middle of a level.

### Playing from different houses

The host's Co-op window shows the addresses a friend can use:

| What the host's window says | What to do |
|---|---|
| **Internet: 73.x.x.x** *(your router opened the door automatically)* | Player 2 types that address. Nothing else to set up. |
| **Tailscale: 100.x.x.x** | Player 2 types that address (both of you must be signed in to Tailscale). |
| *Internet: open TCP port 24050 on your router or use Tailscale* | Use Tailscale (below), or forward TCP port 24050 on the host's router to the host's PC. |

**Tailscale** (free, about five minutes, works through any router):
install it from tailscale.com on both PCs, sign in, and have the host share
their machine with player 2 (or both sign in to the same account). The host's
Co-op window then shows a *Tailscale: 100.x.x.x* address for player 2 to type.
ZeroTier, Radmin VPN and Hamachi also work the same way.

### Coin Rivals (competitive mode)

The host picks **Coin Rivals** on the main menu's multiplayer panel, then starts
Adventure or Time Trial. It starts with the next level, or right away if player 2
joins mid-level.

- Same tank, **separate wallets**: a panel at the top right shows each
  player's money and egg pieces.
- Whoever clicks a coin gets it; whoever clicks a shop button pays.
- Fish you buy are yours (a small square in your color). Coins from your fish
  are **locked to you for 1.5 seconds** (colored corners), then anyone can grab them.
- The player who lands the killing shot on an alien owns its treasure.
- Coins picked up by pets go to the owner of the fish that dropped them;
  unowned ones are split 50/50.
- Tank upgrades help both players, but only the buyer pays. Food costs the dropper.
- Egg pieces cost their normal price; **the first to buy all 3 wins** and completes
  the level (it counts for the host's Adventure only because it was really
  finished). In Time Trial the richest player when time runs out wins. If all the
  fish die, nobody wins.
- If player 2 leaves, the host carries on with their own wallet.

### Tank Race (competitive mode)

The host presses **VERSUS!** on the main menu, picks *Tank Race*, a **Tank** (1-4)
and **Catch-up**, then **Start Race!**. Tank N plays as that tank's final Adventure
level (N-5); races never touch Adventure saves. Each player picks 3 pets from all
the ones they'd have at that point in Adventure (4 for Tank 1 up to 19 for Tank 4),
whatever their profile has unlocked. Each plays their own tank on their own
computer after a 3-2-1 countdown. First to buy all 3 egg pieces wins (Versus eggs
cost 1,500 / 3,000 / 5,000 / 10,000 a piece on Tanks 1-4, well below those levels'
Adventure prices, so a race fits an evening).

| Sabotage (key) | What your rival gets | Counter |
|---|---|---|
| Alien (1) | an alien warps in (unlocks after 45 s) | shoot it: half its price back |
| Hunger (2) | all their guppies get hungry | feed fast |
| Murk (3) | murky water for 10 s | wait it out |
| Thief (4) | a mini alien that eats coins | click it 3 times: half its price back |
| Hike (5) | shop and sabotage prices +30% for 20 s | save up first |
| Poison (6) | a row of green poison potions falls across the tank; a guppy that eats one dies (unlocks after 45 s) | click potions to pop them; fed fish leave them alone |
| Block (7) | for 10 s they can't collect any money (coins, diamonds, pets too) | wait it out |

Prices scale with the level's egg price and each attack has a cooldown. With
**Catch-up** on, whoever has fewer egg pieces pays 40% less. The corner right of
the sabotage buttons shows the rival's money, egg pieces and fish; a faint live
mini-map of their tank sits top right (hold **Tab** to see it clearly). Losing all
your fish or leaving the level hands your rival the win.

### Alien Keeper (competitive mode)

One of you keeps the fish; the other raises aliens in **their own alien tank** and
sends them through a portal to invade. The host presses **VERSUS!**, picks *Alien
Keeper* and a **Tank** (1-4), and presses **Start Round!**. Roles swap every round
(the host keeps the fish first).

- **Fish keeper:** plays the tank's final Adventure level with their chosen pets. There
  are no normal invasions: every alien is your friend's, and a crosshair plus siren
  shows where each one will land. You start with two extra guppies (a breeder on Tank
  4), and the portal stays shut for the first 45 seconds. Win by finishing the egg or
  holding out **8 minutes**.
- **Alien keeper:** your lair is the aliens' home in space, with a portal on the right.
  - **Buy babies** in the top bar (Tank 1: Sylvester; Tank 2 adds Big Sylvester and
    Gus; Tank 3 adds Balrog and Destructor; Tank 4 adds Psychosquid).
  - **Click the lair to drop space goo** (5 each). Babies grow into juveniles and then
    adults as they eat; hungry aliens that aren't fed die.
  - Grown aliens drop **crystals**: click them for money. Each fish your aliens eat
    in the fish tank pays a bounty too.
  - **Click grown aliens to select them** (or press **A** for all), optionally buy
    **Shield** (can't be hurt for 5 s after landing) or **Frenzy** (faster for 8 s) for
    them (one power-up per alien), then press **SEND** or **Space** (a portal fee each). Send one at a time or a
    whole wave (the portal lets one through every 3 s). Every alien comes through
    shielded for 1 s. Juveniles
    land at about half strength; adults at full.
  - Pick where they land by clicking the **mini-map** of the fish tank (bottom right),
    or hold **Tab** to watch the fish tank live and click there. While holding Tab,
    hold the mouse to make your aliens **hunt** the fish nearest your cursor.
  - **Blackout** (top bar) darkens the fish keeper's screen for 6 s, except a
    flashlight around their cursor.
  - You win by wiping out every fish. You lose if you have no aliens left anywhere
    and can't afford a new baby. Esc twice gives up the round.

### Pet Heroes (3.0, competitive MOBA)

Each of you keeps your own tank (twice the usual size) and plays one pet as a **hero**.
Between your tanks lies **the Trench**: both sides' minion waves meet there and fight.
Win by breaking your friend's **treasure-chest core**. The host presses
**VERSUS! → Pet Heroes → Start Match!**; both pick a hero and press **Lock in!**. Practice
against the bot from the Co-op window, or press **Tutorial** there for a guided first
match. Matches run about 10-12 minutes. Hold **H** in a match for the controls.

| Hero | Role | Ultimate (F, from level 6) |
|---|---|---|
| **Itchy** (swordfish) | Assassin: dashes in and cuts targets down | Swordstorm: spins and cuts everything around |
| **Clyde** (jellyfish) | Mage: zaps from range, slowing fields, teleports | Thunderstorm: lightning hits every enemy in the tank |
| **Rhubarb** (hermit crab) | Tank: leaps in, grabs, stuns | Tidal Slam: stuns and knocks back everything nearby |
| **Angie** (angelfish) | Support: heals and shields you, towers and the core, charms minions | Resurrection: revives dead fish, heals towers and herself |
| **Speedy** (neon snail) | Farmer: collects coins, slows, gasses towers | Gold Rush: double coins that fly to him (or home while he's away) |
| **Presto** (shapeshifter) | Trickster: cards, swaps places, leaves a decoy | Copycat: becomes the enemy hero, with their abilities |
| **Niko** (clam) | Builder: pearl turrets, a pearl cannon, a clam shield | Fortress: a giant clam that fires at everything near |
| **Meryl** (mermaid) | Singer: sings enemies to sleep, rallies her minions | Siren Song: pulls everyone in and steals their minions |
| **Shrapnel** (bomb fish) | Artillery: lobbed bombs, hidden mines, blast jumps | Missile Barrage: rains missiles on an area |

- **W A S D** to move; your hero **attacks on its own**. Speedy, Rhubarb and Niko walk the
  floor: **A/D** walk, **W** or **Space** hops, **S** near a portal or pad crosses.
  **Right-click** an enemy (or a monster) to attack it, or the ground to move there.
- **Q E R F**: tap to cast at the mouse, or **hold to see where it goes** and let go
  (right-click cancels). At levels 3, 6 and 9 pick a **talent** with **Z** or **X**; at
  level 6 your pet **evolves** and F unlocks.
- **The portal** (top middle) takes you into the Trench; the gate at its far end leads
  into your friend's tank. The corner floor pads go straight there too.
- **The Trench:** minions that die there drop coins, and whoever touches them gets them.
  The Gus and Balrog camps give buffs; the **Psychosquid** (4:00) joins your next wave
  and the **Boss** (8:00) makes you huge for two minutes.
- **Left-click** in your own tank, or in the **home window** (top right while you're away):
  coins, food ($5), your laser. **1-4** quick-buy (guppy, food, breeder, carnivore), **5**
  buys the item suggested for your hero, **B** opens the shop.
- Break both of your friend's **towers** (giant clams), then the core; push with your
  minions. Kill streaks raise your bounty. Sudden death from 12:00.

The full rules and every number: [docs/PET_HEROES.md](docs/PET_HEROES.md).

### Controls & co-op features

- Everything works exactly like the original game, for both players.
- **Middle-click** in the tank to drop a **"look here!" ping** your partner sees.
- Each player's cursor is visible to the other with their name
  (player 1 gold, player 2 cyan).
- Player 2: press **Esc** for the co-op menu (Keep Playing / Options / Leave Game).
- Host: **Options → Co-op** in a level shows who's connected and the ping, lets
  you change difficulty or remove player 2.
- When a level is won, a **co-op report** shows who collected how much, who
  landed the most laser hits, who fed the fish, and crowns a **Tank MVP**.
- The game no longer pauses when the host clicks away from the window, and
  keeps running if the host minimizes it, because player 2 is still playing.

### Difficulty (chosen by the host in the Co-op window)

| | Alien health | Time between invasions | Surprise second alien | Fish hunger | Egg price |
|---|---|---|---|---|---|
| **Classic** | original | original | never | original | original |
| **Double Trouble** *(default)* | x1.6 | x0.8 | 25% of invasions | +15% | original |
| **Insane** | x2.2 | x0.6 | 55% of invasions | +35% | +20% |

Scaling only applies while a second player is connected; alone, the mod plays
exactly like the original. Bosses and scripted double-alien waves are never
doubled up.

### Troubleshooting

- **"Couldn't reach the host"**: check the address, make sure the host clicked
  *Host a Game* and allowed the program through the Windows Firewall
  (`Allow through Windows Firewall.bat`). Across the internet, see above.
- **"...forbidden by its access permissions"**: something on the joining PC
  (usually antivirus software or a VPN) blocks the connection. Allow
  `InsaniquariumCoop.exe` in the antivirus, or switch the VPN off.
- **"Version mismatch"**: both players need the same version of the mod.
- **Choppy picture for player 2**: player 2 sees the host's game streamed at up
  to 60 frames a second; it needs very little bandwidth (typically well under
  0.5 Mbit/s), so choppiness usually means Wi-Fi trouble or a far-away VPN relay.
  The ping is shown in player 2's Esc menu and the host's Co-op window.
- A log is written to `%APPDATA%\PopCap\InsaniquariumCoop\coop_log.txt`.
- Command-line options: `-windowed`, `-host`, `-join=<address>`,
  `-resdir=<game folder>`, `-savedir=<folder>`.

---

---

## For developers

- `coop/`: everything multiplayer (the connection, the drawing stream, the modes).
  [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) explains how it fits together.
- `patches/`: this mod's changes to the game's source (`winfish.patch`) and to its
  SDL2/OpenGL framework (`pvz-portable.patch`), applied on top of the commit pinned in
  `scripts/upstream.lock` by `scripts/fetch-upstream.sh` (Mac/Linux) or `Build.bat`.
  Neither the decompiled source nor the framework is part of this repository;
  `scripts/make-patches.sh` regenerates the patches after you edit the fetched folders.
- Mac rebuild-and-run: `scripts/build-macos.sh --run -windowed`.

Contributions are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md). Please never attach
game files, or extracted game data, to issues or pull requests.

## How this was made

Most of this mod (its code, docs, app icon and trailer music) was written with
[Claude](https://www.anthropic.com/claude), Anthropic's AI coding assistant, directed,
reviewed and playtested by the project's maintainer. Bug reports are very welcome.

## Credits & legal

- *Insaniquarium! Deluxe* © PopCap Games / Electronic Arts. Buy it on
  [Steam](https://store.steampowered.com/app/3320/Insaniquarium_Deluxe/). This project
  contains no game assets.
- Game code: the community [WinFish](https://github.com/Vindirect/WinFish) decompilation
  and the [insaniquarium-mac](https://github.com/Jake-Vellora/insaniquarium-mac) port
  (MIT, Jake Vellora and contributors), downloaded at build time.
- Framework: [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable)'s SexyAppFramework
  (LGPL-3.0-or-later + PopCap Games Framework License), downloaded at build time.
- The mod's own code (`coop/`, `platform/`, `scripts/`, `patches/`) is MIT licensed (see
  `LICENSE`). Third-party library notices are in `THIRD_PARTY_NOTICES.txt`.
- The app icon is original art made for this mod (`platform/icon/icon.swift`).
- An unofficial fan mod, not affiliated with or endorsed by PopCap Games or Electronic
  Arts. Insaniquarium is a trademark of Electronic Arts Inc.
