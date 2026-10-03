# Architecture

How the mod works, for someone about to change it. File paths are relative to the repo.

## Code base

- `WinFish/source/WinFish/`: the decompiled game (classes like `Board`, `Fish`, `Coin`,
  `Alien`, `WinFishApp`). Many members still have decompiler names (`m0x43c` is the egg
  piece counter, 1 = none, 4 = done; `Unk07(v)` adds money; `Unk10()` sums coins still
  flying to the counter; `Unk04()` flushes them into the wallet).
- `PvZ-Portable/src/SexyAppFramework/`: the SDL2 + OpenGL port of PopCap's framework.
- `coop/`: all multiplayer code (namespace `Coop`). `platform/`: entry point, finding the
  game files, save import, per-OS resources.
- Game and framework edits are small and call into `Coop::`; search for `Coop::` or
  `Coop` comments to find them.

## Five process models

| Mode | Who simulates | What crosses the network |
|---|---|---|
| Co-op, Coin Rivals | Host only | Host → guest: drawing commands (one deflate stream), audio/music events. Guest → host: mouse/keyboard |
| Tank Race | Each computer runs its own board | Setup/ready/go, 5 Hz status snapshots, attacks, finish/result (host decides) |
| Alien Keeper | The fish keeper's computer only (host or guest; swaps each round) | Tank Race's round messages, plus the drawing stream fish keeper → alien keeper (either direction) and small lair messages back |
| Pet Heroes | Each computer runs its own side (its tank and its hero), in the mode's own simulation (no `Board`); the host also runs the Trench between the tanks | Draft/go/cancel, then `MSG_HEROES_DATA` wrapping the sides' messages: hero, tank and Trench snapshots, hits, rewards, events, waves into the Trench and minions arriving from it (see [PET_HEROES.md](PET_HEROES.md)) |

The guest in co-op still runs the full game program, but its screen is covered by
`RemoteView` (`coop/CoopView.*`), which draws the host's frames and forwards input. In a
race the guest leaves that view, plays its own board, and returns to it afterwards.

## Wire protocol (`coop/CoopProtocol.h`, version 8)

One TCP connection, port 24050 (`TCP_NODELAY`; `Send` writes at once without blocking, and
what doesn't fit goes out at the next `Poll`, D32). Every message: `[u32 length][u8 type][payload]`,
little-endian. LAN discovery: UDP beacons on port 24051 with the magic `INSQCOOP`.

| Type | Direction | Meaning |
|---|---|---|
| HELLO 1 | guest → host | magic, protocol, version string, name, hashes of the guest's images |
| INPUT 2 | guest → host | input events (move/down/up/wheel/key/char/leave) |
| FRAME_ACK 3 | guest → host | frame sequence number |
| NEED_PIXELS 7 | guest → host | an image id the guest couldn't resolve |
| PING_MARKER 8 | guest → host | "look here!" position |
| PING/PONG 10/11, BYE 12 | both | round-trip time, leaving with a reason |
| WELCOME 20, REJECT 21 | host → guest | accepted (host name, difficulty) / refused (reason, e.g. version mismatch) |
| FRAME 22, AUDIO 23 | host → guest | one frame of the drawing stream / sound and music events |
| RACE_* 30-38 | see Tank Race | setup, ready, go, state, attack, finish, result, event, cancel |
| STREAM_START 39 | viewer → sender | Alien Keeper: the viewer's image hashes; the sender resets its encoder and starts streaming |
| KEEPER_* 40-44 | see Alien Keeper | launch, power, cursor (hunt point), lair status, events (bounty, alien down, landed, clock) |
| HEROES_* 45-49 | see Pet Heroes | setup (host opens the draft), pick (locked in), go (seed and heroes), data (the sides' messages), cancel |

Bump `kProtocolVersion` whenever messages change; hosts reject other versions. The
exceptions so far add an optional field at the end that older copies never read: since
2.1, WELCOME ends with the host's version string (2.0 and 2.1 still play together); since
2.2.1, Pet Heroes' hero and tank snapshots end with the sender's clock (D34; without it
the other side is drawn a fixed 90 ms behind, as before).

### Versions and the update check (`coop/CoopUpdate.*`)

- Both sides know each other's version (HELLO carries the guest's, WELCOME the host's).
  A different protocol is rejected with a message saying who should update; a different
  release on the same protocol gets a shared toast ("Ann has 2.1.5, Ben has 2.1.0: Ben
  should update"). The test harness's `fakeversion <v>` changes what this copy reports.
- Once per launch, on the main menu, a background thread fetches `COOP_UPDATE_URL` (the
  GitHub latest-release API; `INSANIQ_UPDATE_URL` overrides it) with WinHTTP on Windows
  and `curl` elsewhere, and reads either `tag_name`/`html_url` or a manifest's
  `version`/`url`. A newer version opens a dialog whose Open Page button calls
  `SDL_OpenURL`; Not Now remembers that version. Nothing is downloaded or installed.
  Failures (offline, or 404 while the repo is private) are logged only. The Co-op
  window's Update Check button turns it off (`CoopUpdateCheck` in the settings); test
  scripts stay offline unless `INSANIQ_UPDATE_URL` is set.

### The drawing stream (co-op)

- `CoopHooks` gives the framework a `DrawRecorder`: every `GLImage` primitive (fill,
  line, polygon, blit, mirrored/float/rotated/stretched blit, matrix blit, triangles)
  is reported while a frame is being captured.
- `CoopStream` (`FrameEncoder`) turns them into commands (`FC_*` in `CoopProtocol.h`).
  Images are defined once per session: `FC_DEF_REF` (a 64-bit content hash the guest
  already has) or `FC_DEF_PIX` (raw pixels for runtime images), freed with `FC_FREE`
  (LRU, 48 MB budget).
- The stream is a single zlib stream (level 3), `Z_SYNC_FLUSH` per frame.
- Flow control: a new frame is captured only when fewer than
  `clamp((rtt + 80) / 16, 4, 20)` frames are unacknowledged and 15 ms have passed.
- The guest's `FrameDecoder` replays commands onto its screen; unknown hashes trigger
  NEED_PIXELS.

### Audio

`AudioHook` sees every sound played/stopped and every music call on the host
(`SDLSoundInstance`, `SDLMusicInterface`); they're sent as events and replayed on the guest.

### Input injection

`Session::InjectGuestEvent` swaps the widget manager's mouse state for player 2's
(`WidgetManager::SwapMouseState`, `MouseState`), sets `gRemoteDispatch` and
`mCurrentPlayer = 1`, dispatches, and swaps back. `Session::CurrentPlayer()` tells game
code who is acting. Holds (feed/fire) are tracked per player in `Board::CoopUpdateHolds`.

## Framework hooks (`PvZ-Portable/src/SexyAppFramework/CoopHooks.*`)

| Hook | Used for |
|---|---|
| `gDrawRecorder` | capturing drawing primitives (co-op stream) |
| `gAudioHook` | mirroring sound and music |
| `gAppHook` (`PreUpdateFrames`, `PreDrawScreen`, `PostDrawScreen`, `AllowLostFocusPause`, `KeepRunningWhenMinimized`, `IdleWait`) | session polling, frame capture, overlays, not pausing while someone else plays; `IdleWait` replaces the sleep between updates while connected, so messages are handled and frames drawn as they arrive (D33) |
| `gImageDestroyedHook` | freeing streamed images |
| `gRemoteDispatch`, `gRemoteCursor` | input from player 2, and the cursor shape they should see |
| `gUpdatingWidget` | which widget's `Update()` is running (who spawned a coin or a baby fish) |
| `ForEachMemoryImage` | hashing images |

## Coin Rivals internals (`coop/CoopGame.cpp`)

- `RivalsScope(p)` saves the acting player's wallet (`Board::mMoney`) and egg counter
  (`m0x43c`) and loads player `p`'s; the destructor swaps back. Used around player 2's
  input, each player's held buttons, and coin/larva/food money arriving.
- `GameObject::mCoopOwner` is stamped in `Board::AddGameObject` from
  `RivalsSpawnOwner()`: the updating creature's owner, else the acting player during
  input, else nobody (aliens). `mCoopCollector` records who clicked a coin.
- `Coin::MouseDown` refuses clicks on another player's locked coin; `ReceiveMoney`
  credits the collector, the owner, or splits.
- The HUD (wallets, egg pips, fish tags, coin locks) is drawn in the host's overlay, so
  it's streamed to the guest without protocol changes.

## Tank Race internals (`coop/CoopRace.cpp`)

- Phases: `IDLE → SETUP` (maybe the pets screen) `→ READY` (board built, paused) `→
  COUNTDOWN` (3 s) `→ RUNNING → FINISHED` (waiting for the host) `→ RESULT` (7 s) `→ IDLE`.
- Starting: the host sends `RACE_SETUP`; both call `BeginLocal()`, which clears screens and
  starts an Adventure game. `RaceOverrideLevel` (called from `Board::InitLevel`) swaps in
  the race's tank and level. `WinFishApp::StartGame` calls `RaceOnGameStarted`, which
  pauses the board and sends `RACE_READY`. When both are ready the host sends `RACE_GO`.
- Safety: `Board::SaveCurrentGame` returns early for the race board (`RaceOwnsBoard`), so a
  race never saves or erases a real Adventure save; `WinFishApp::IsBonusLevel` returns
  false during a race.
- Finishing: `Board::HandleBuyEgg` (third piece) and the all-fish-dead branch of
  `Board::Update` call `RaceOnEggComplete` / `RaceOnTankLost`; destroying the board early
  is a forfeit. The guest reports with `RACE_FINISH`; the host sends `RACE_RESULT`.
- Attacks: `RaceBar` (bottom buttons, keys 1-7 via `Board::KeyDown`) → `RACE_ATTACK` →
  `ReceiveAttack`. The coin thief is a `ThiefWidget` (mini Sylvester image). The poison
  raid drops `Food` with `mFoodType = 3` (star potion) and `mCoopPoison` set: `Fish` and
  `Breeder` die on eating it, Nimbus ignores it, it doesn't count toward the food limit
  (`Board::DropFood`), and clicking it calls `RacePoisonClicked`.
  The coin blocker is `RaceCoinsBlocked()`, checked in `Coin::MouseDown`,
  `Larva::MouseDown` and the coin-collecting pets (`OtherTypePet`).
  `Board::Buy` applies the price hike via `RaceAdjustCost`; `Alien::Remove` reports
  beaten aliens for refunds.
- During a race the host stops streaming (`Session::SuspendStreaming`) and both sides draw
  their own overlay (`DrawRaceOverlay`, rival panel, toasts).

## Finding the game files (`platform/AssetLocator.cpp`)

Order: `-resdir=` argument (or `INSANIQ_RESDIR`), the folder remembered in
`InsaniquariumCoop.cfg` (next to the exe on Windows; in the save folder elsewhere), the
exe's folder and its parents, the folder `scripts/get-game-files.sh` downloads to (Mac,
Linux), every Steam library, PopCap folders (Windows). If nothing is found, Windows shows
a folder picker, macOS an AppleScript dialog with "Choose Folder...".

## Menus and Versus settings (`coop/CoopVersus.*`)

- The game selector attaches a `MenuPanel` (`AttachMenuPanel` / `DetachMenuPanel` /
  `MenuPanelToFront`), shown to the host while a guest is connected: Co-op / Coin
  Rivals (`Session::SetMode`, now only those two) and VERSUS!, which opens the
  `VersusDialog` (mode via `SetVersusKeeper`, Tank 1-4 via `SetRaceTank`, Catch-up,
  Start). Player 2's clicks (`CurrentPlayer() == 1`) are ignored on both.
- Versus rounds always play level 5 of the chosen tank (`gRace.mSetLevel`, not saved;
  test scripts may still pick a level with `race <tank> <level>`).
- Stage pets: `RaceStagePetOverride` / `RaceStagePetCount` / `RaceStagePetSlots` are
  asked by `UserProfile::IsPetUnlocked`, `Board`, `PetsScreen` and the race setup while
  a Versus round runs on this computer's tank (pets 0..5*tank-2, 3 slots). Test
  scripts keep their profiles' pets (`INSANIQ_PROFILE_PETS`, set by the test runner)
  except the `pets` variants.

## Alien Keeper (`coop/CoopKeeper.*`)

- Rounds are Tank Race rounds with `gRace.mKeeper` set (`MSG_RACE_SETUP` carries the mode
  and who keeps the fish; the host alternates it). The fish keeper builds a tank as in
  a race (`BeginLocal`); the alien keeper runs `BeginLair`: no `Board`, a `LairTank`
  widget (the lair screen) and `Session::StartKeeperView` (a passive `RemoteView`,
  hidden except while Tab is held; its sounds only play while shown, its music never).
  Finish reasons: `FIN_SURVIVED` (the clock ran out) wins for the fish keeper;
  `FIN_LOST` from the fish keeper means the tank was wiped, from the alien keeper
  (`RaceKeeperLairDefeated`) that the lair ran out of aliens and money.
- The lair is simulated only on the alien keeper's computer: `LairAlien`s (growth by
  meals of goo, hunger, crystals, selection, per-alien Shield/Frenzy), goo, crystals,
  money, the portal. Sending flies the aliens into the portal and sends one
  `MSG_KEEPER_LAUNCH` each (kind, landing spot or -1, buff flags). It also sends its
  cursor while hunting (20 Hz) and a 4 Hz summary (money, aliens, grown).
- The fish keeper's computer spawns each alien 2.5 s after a warp warning, keeps its
  buffs in `mBuffs` (by `Alien*`), holds natural invasions off (`mAlienTimer`), ends the
  round on the clock, applies Blackout (drawn after `FinishFrame`, so the lair never
  sees it) and reports bounties and aliens shot down.
- Streaming either way: `StartKeeperView` sends `MSG_STREAM_START` with the viewer's image
  hashes; the fish keeper's `HandleStreamStart` resets its encoder and sets
  `mKeeperSending`, which `PreDrawScreen`/`PostDrawOverlays` use instead of "host with a
  guest". Frame/ack/pixel/audio messages are handled by whichever side is sending or
  viewing. After a round both sides go back to the normal host -> guest co-op stream.
- Game hooks: `Alien::FindNearestFood` ranks fish by distance from the hunt point
  (`KeeperHuntPoint`), `Alien::Update` scales movement by `KeeperAlienSpeed(this)`,
  `Alien::Shot` bounces off while `KeeperAlienShielded(this)`, `Alien::CheckCollision`
  reports `KeeperAlienAte`, `RaceOnAlienRemoved` forwards to `KeeperAlienRemoved`.

## Pet Heroes (`coop/CoopHeroes*`, `coop/heroes/`)

- The simulation (`coop/heroes/`, plain C++, no framework) builds into the game and into
  a headless test program (checks, and bot-vs-bot balance tournaments). The file headers
  in `coop/heroes/` describe each part.
- `CoopHeroes.cpp` owns the `HeroesScreen` widget (draft, countdown, match, result),
  input, practice (a `Heroes::Match`: two sides and an in-memory link, the bot on one)
  and the network lifecycle (a `Heroes::Side` plus a `SessionLink` that sends the
  side's messages as `MSG_HEROES_DATA`). The host suspends the co-op stream and the guest
  its view while a network match is up (`Session::IsRacing` counts it as a race).
- `CoopHeroesHud.cpp` draws the HUD, shop, banners, talent cards, death recap, tutorial
  panel, draft, help and result screens.
- `CoopHeroesDraw.cpp` draws the world (a tank or the Trench) through a movable transform
  (`gView`: the main view, the home window, the world map) at 0.5 scale with `DrawImageTransformF`
  (float positions, so a big window stays sharp); walls and the floor are textured
  triangles from small patches cut out of the tank paintings (`Cut`: the 640x480 paintings
  are several GPU textures, which textured triangles can't use); a few sprite sheets have no
  rows/cols in resources.xml and get 80x80 cels (`Cel`).
- During a windowed match the window grows to a 4:3 size up to 1280x960 and is restored
  afterwards (not under test scripts unless `INSANIQ_HEROES_GROW=1`).

## Test harness (`coop/CoopTest.cpp`)

`INSANIQ_TESTSCRIPT=<file>` makes the game follow a timed script (clicks, keys and test
commands, one per line) for automated two-player tests; without it the harness does
nothing. The commands are listed in `coop/CoopTest.cpp`.

## Logs and switches

- Log: `coop_log.txt` in the save folder (also printed with `INSANIQ_COOPLOG=1`).
- Environment: `INSANIQ_RESDIR` (game files), `INSANIQ_TESTSCRIPT`, `INSANIQ_COOPLOG`,
  `INSANIQ_COOPSTATS` (stream statistics: `Stream:` from the sender, `Recv:` every 2 s
  from the receiver, `Heroes view:` in Pet Heroes matches), `INSANIQ_FAKE_VPN` (the host window's VPN note), `INSANIQ_NO_IDLEWAIT` (sleep between
  updates as before D33), `INSANIQ_NO_UPNP`, `INSANIQ_TEST_HASHDROP=<n>`
  (guest pretends to lack every n-th image, forcing pixel fallback).
- Command line: `-windowed`, `-host`, `-join=<address>` (retries for 30 s),
  `-resdir=<folder>`, `-savedir=<folder>`.
