#include "HeroesData.h"

namespace Heroes
{
	///////////////////////////////////////////////////////////////////////////
	// Heroes. Ability numbers are at rank 1; see RankScale / RankCooldown.
	// mDamage/mRadius/mDurationS/mExtra meanings are noted per ability.
	///////////////////////////////////////////////////////////////////////////
	static const HeroDef kHeroes[HERO_COUNT] =
	{
		{
			"Itchy", "Assassin", "A swordfish who dives in, cuts a target down and darts away.",
			false, 560, 41, 70, 245, 0.9f, 34, 0,
			"Bloodlust", "Each hit on the same target: +8% attack speed (up to 5).",
			{
				// Lunge: dash mRange toward the aim, mDamage to the first enemy within mRadius.
				{ "Lunge", "Dash forward and strike the first enemy you hit.", 7, AIM_DIR, 260, 70, 45, 0.25f, 0 },
				// Sharpen: the next mExtra attacks within mDurationS deal +mDamage.
				{ "Sharpen", "Your next 3 attacks deal bonus damage.", 10, AIM_SELF, 0, 40, 0, 6, 3 },
				// Evade: untargetable for mDurationS, +mDamage speed for mExtra seconds.
				{ "Evade", "Vanish for a moment and swim away fast.", 14, AIM_SELF, 0, 0.5f, 0, 1, 2 },
				// Swordstorm: mDamage every mExtra seconds to enemies within mRadius for mDurationS.
				{ "Swordstorm", "Spin for 3 s, cutting everything around you.", 45, AIM_SELF, 0, 45, 110, 3, 0.3f },
			},
			{ AB_Q, AB_W, AB_Q, AB_E, AB_Q, AB_W, AB_E, AB_W, AB_E },
			{
				{ { "Bloodrush", "Lunge resets when it hits an enemy below 30% health." }, { "Long Lunge", "Lunge reaches 60% farther." } },
				{ { "Whirlpool", "Swordstorm pulls enemies toward you." }, { "Endless Storm", "Swordstorm lasts 5 s." } },
				{ { "Frenzy", "Bloodlust stacks up to 8 times." }, { "Slippery", "Evade heals 15% of your health." } },
			},
			{ ITEM_SHARP_FIN, ITEM_LEECH_TOOTH, ITEM_SPEED_KELP, ITEM_KRAKEN_TOOTH, ITEM_ELECTRIC_SCALE, ITEM_INK_SAC },
		},
		{
			"Clyde", "Mage", "A jellyfish who zaps from a distance and controls the water.",
			false, 420, 26, 210, 200, 1.1f, 32, 600,
			"Static", "Every 4th attack chains to 2 more enemies.",
			{
				// Zap: a bolt flying mRange at mExtra speed, mDamage to the first enemy (hit radius mRadius).
				{ "Zap", "A lightning bolt that hits the first enemy in its path.", 5, AIM_DIR, 520, 90, 26, 0, 650 },
				// Static Field: zone of mRadius at the aim for mDurationS: mDamage per second, mExtra slow.
				{ "Static Field", "An electric zone that slows and hurts enemies.", 12, AIM_POINT, 380, 28, 120, 3, 0.4f },
				// Drift: teleport up to mRange toward the aim.
				{ "Drift", "Teleport a short distance.", 12, AIM_POINT, 220, 0, 40, 0, 0 },
				// Thunderstorm: mExtra strikes over mDurationS on every enemy in the tank, mDamage each.
				{ "Thunderstorm", "Lightning strikes every enemy in the tank 4 times.", 90, AIM_SELF, 0, 70, 0, 2.0f, 4 },
			},
			{ AB_Q, AB_W, AB_Q, AB_E, AB_Q, AB_W, AB_W, AB_E, AB_E },
			{
				{ { "Overcharge", "Zap jumps on to a second enemy." }, { "Long Fuse", "Zap flies 40% farther and faster." } },
				{ { "Supercell", "Thunderstorm strikes 6 times." }, { "Stunning Storm", "Each Thunderstorm strike stuns for 0.3 s." } },
				{ { "Blink Field", "Drift leaves a Static Field where you were." }, { "Capacitor", "Every 3rd attack chains." } },
			},
			{ ITEM_PEARL_CHARM, ITEM_THICK_SHELL, ITEM_SEA_CROWN, ITEM_ELECTRIC_SCALE, ITEM_INK_SAC, ITEM_GOLDEN_SCALE },
		},
		{
			"Rhubarb", "Tank", "A hermit crab who walks the floor, leaps in and holds the line.",
			true, 740, 44, 80, 190, 0.95f, 38, 0,
			"Shell", "Takes 30% less damage from towers and lasers.",
			{
				// Leap: jump to a floor spot within mRange (flight mDurationS), mDamage and an mExtra-second stun within mRadius.
				{ "Leap", "Jump to a spot, stunning enemies where you land.", 10, AIM_POINT, 320, 70, 100, 0.6f, 0.8f },
				// Claw Grab: the first enemy within mRadius of a line mRange long is pulled to you and takes mDamage.
				{ "Claw Grab", "Pull the first enemy in a line to you.", 12, AIM_DIR, 380, 50, 36, 0, 0 },
				// Shell Up: immune for mDurationS; enemy minions and towers target you.
				{ "Shell Up", "Hide in your shell: immune for 2 s, and enemies attack you.", 16, AIM_SELF, 0, 0, 0, 2, 0 },
				// Tidal Slam: mDamage and an mExtra-second stun within mRadius, knocking enemies away.
				{ "Tidal Slam", "Slam the floor: stun and knock back everything around you.", 75, AIM_SELF, 0, 150, 220, 0, 1.2f },
			},
			{ AB_Q, AB_E, AB_W, AB_Q, AB_E, AB_Q, AB_W, AB_E, AB_W },
			{
				{ { "Heavy Landing", "Leap stuns for 1.5 s." }, { "Spring Legs", "Leap's cooldown is 3 s shorter." } },
				{ { "Aftershock", "Tidal Slam hits again 1 s later for half." }, { "Riptide", "Tidal Slam reaches 40% farther." } },
				{ { "Spiked Shell", "Enemies hitting you during Shell Up take 40." }, { "Iron Claw", "Claw Grab stuns for 1 s." } },
			},
			{ ITEM_THICK_SHELL, ITEM_CORAL_ARMOR, ITEM_TOWER_BUSTER, ITEM_KRAKEN_TOOTH, ITEM_LEECH_TOOTH, ITEM_SHARP_FIN },
		},
		{
			"Angie", "Support", "An angelfish who heals, shields and turns enemy minions to her side.",
			false, 540, 28, 220, 220, 1.0f, 32, 550,
			"Grace", "Fish that die in your tank have a 25% chance to come back.",
			{
				// Heal Beam: heal herself or an own tower/core near the aim by mDamage over mDurationS.
				{ "Heal Beam", "Heal yourself, a tower or your core.", 8, AIM_FRIEND, 360, 180, 0, 2, 0 },
				// Halo: shield of mDamage for mDurationS on herself or an own tower/core near the aim.
				{ "Halo", "A shield on yourself, a tower or your core.", 14, AIM_FRIEND, 360, 240, 0, 4, 0 },
				// Charm: enemy minions within mRadius fight for her for mDurationS.
				{ "Charm", "Nearby enemy minions fight for you for 5 s.", 16, AIM_SELF, 0, 0, 200, 5, 0 },
				// Resurrection: revive your last mExtra dead fish, heal your towers mDamage and yourself fully.
				{ "Resurrection", "Bring back your last 5 dead fish, heal your towers and yourself.", 90, AIM_SELF, 0, 250, 0, 0, 5 },
			},
			{ AB_Q, AB_W, AB_Q, AB_E, AB_W, AB_Q, AB_W, AB_E, AB_E },
			{
				{ { "Radiance", "Heal Beam heals 40% more." }, { "Guardian", "Halo lasts twice as long." } },
				{ { "Miracle", "Resurrection also heals your core 600." }, { "Second Wind", "Resurrection brings back 10 fish." } },
				{ { "Pied Piper", "Charm lasts 8 s and charms big aliens too." }, { "Angelic", "+20% speed; your heals end slows and stuns." } },
			},
			{ ITEM_PEARL_CHARM, ITEM_THICK_SHELL, ITEM_GOLDEN_SCALE, ITEM_SEA_CROWN, ITEM_CORAL_ARMOR, ITEM_INK_SAC },
		},
		{
			"Speedy", "Farmer", "A neon snail who hoovers up coins, spits slime, slimes the floor and gasses towers.",
			true, 800, 40, 230, 175, 1.0f, 34, 520,
			"Scavenger", "Collects your coins he touches, and +$1 per second.",
			{
				// Slime Trail: for mDurationS he leaves slime (mRadius wide) that slows enemies by mExtra.
				{ "Slime Trail", "Leave a trail of slime that slows enemies.", 10, AIM_SELF, 0, 0, 40, 4, 0.5f },
				// Stink Cloud: cloud of mRadius at the aim for mDurationS, mDamage per second; towers inside can't shoot.
				{ "Stink Cloud", "A cloud that hurts enemies and stops towers inside it from shooting.", 11, AIM_POINT, 300, 56, 140, 3, 0 },
				// Shell Up: immune for mDurationS.
				{ "Shell Up", "Hide in your shell: immune for 2 s.", 15, AIM_SELF, 0, 0, 0, 2, 0 },
				// Gold Rush: for mDurationS your fish drop double coins and every coin flies to you (away: into the wallet).
				{ "Gold Rush", "For 8 s your fish drop double coins and every coin in your tank flies to you (or, while you're away, into your wallet).", 75, AIM_SELF, 0, 0, 0, 8, 0 },
			},
			{ AB_W, AB_Q, AB_W, AB_E, AB_W, AB_Q, AB_Q, AB_E, AB_E },
			{
				{ { "Sticky Slime", "Slime Trail slows 70%." }, { "Toxic Slime", "Slime Trail burns for 25 a second (was 10)." } },
				{ { "Mother Lode", "Gold Rush lasts 12 s." }, { "Treasure Trail", "During Gold Rush, coins in the Trench fly to you too." } },
				{ { "Hoarder", "Scavenger pays +$3 a second." }, { "Stink Bomb", "Stink Cloud is 50% wider." } },
			},
			{ ITEM_GOLDEN_SCALE, ITEM_THICK_SHELL, ITEM_TOWER_BUSTER, ITEM_CORAL_ARMOR, ITEM_PEARL_CHARM, ITEM_SPEED_KELP },
		},
		{
			"Presto", "Trickster", "A shapeshifting pet magician: cards, swaps, decoys, and he can become your rival.",
			false, 520, 31, 190, 230, 1.0f, 32, 600,
			"Misdirection", "After an ability, your next attack deals +30 and slows.",
			{
				// Card Trick: 3 cards in a fan (mRadius hit radius, mExtra speed, mRange reach), mDamage each.
				{ "Card Trick", "Throw a fan of 3 cards; each hits the first enemy in its path.", 5, AIM_DIR, 420, 50, 20, 0, 700 },
				// Switcheroo: swap places with the enemy within mRadius of the aim (reach mRange).
				{ "Switcheroo", "Swap places with an enemy near the mouse.", 12, AIM_POINT, 380, 0, 120, 0, 0 },
				// Decoy: hidden for mDurationS (+mExtra speed); a fake Presto stays and explodes for mDamage within mRadius.
				{ "Decoy", "Vanish for 2 s and leave a fake you that explodes.", 14, AIM_SELF, 0, 80, 120, 2, 0.3f },
				// Copycat: become the enemy hero (their stats and Q/E/R) for mDurationS.
				{ "Copycat", "Become the enemy hero for 12 s: their stats and their Q, E and R.", 70, AIM_SELF, 0, 0, 0, 12, 0 },
			},
			{ AB_Q, AB_W, AB_Q, AB_E, AB_Q, AB_W, AB_E, AB_W, AB_E },
			{
				{ { "Full Deck", "Card Trick throws 5 cards." }, { "Marked Cards", "Cards slow by 30% for 1.5 s." } },
				{ { "Method Actor", "Copycat lasts 18 s." }, { "Quick Change", "Copycat's cooldown is 25 s shorter." } },
				{ { "Smoke and Mirrors", "Decoy lasts 3.5 s and its blast stuns." }, { "Bait and Switch", "Switcheroo heals you 120; 7 s cooldown." } },
			},
			{ ITEM_SHARP_FIN, ITEM_PEARL_CHARM, ITEM_SEA_CROWN, ITEM_INK_SAC, ITEM_ELECTRIC_SCALE, ITEM_KRAKEN_TOOTH },
		},
		{
			"Niko", "Builder", "The clam who guards towers, now on the move: pearl turrets, a cannon and a fortress.",
			true, 700, 34, 220, 175, 1.1f, 38, 560,
			"Hard Shell", "Takes 20% less damage while standing still.",
			{
				// Pearl Turret: a turret at the aim for mDurationS, mDamage every mExtra s to an enemy within mRadius.
				{ "Pearl Turret", "Plant a turret that shoots pearls at enemies (2 at a time).", 14, AIM_POINT, 300, 16, 260, 10, 1.1f },
				// Clam Shield: mDamage less damage for mDurationS; the first hero to hit him is stunned mExtra s.
				{ "Clam Shield", "Take half damage for 3 s; the first hero to hit you is stunned.", 13, AIM_SELF, 0, 0.5f, 0, 3, 1.0f },
				// Pearl Cannon: a pearl landing at the aim after mDurationS: mDamage and an mExtra-second stun within mRadius.
				{ "Pearl Cannon", "Lob a giant pearl: damage and a stun where it lands.", 11, AIM_POINT, 450, 75, 75, 0.75f, 0.6f },
				// Fortress: rooted for mDurationS, 45% less damage, a pearl at up to 3 enemies within mRadius every mExtra s for mDamage.
				{ "Fortress", "Become a giant clam for 8 s: rooted, tougher, and firing pearls at everything near.", 90, AIM_SELF, 0, 22, 340, 8, 0.6f },
			},
			{ AB_Q, AB_E, AB_Q, AB_W, AB_Q, AB_E, AB_W, AB_E, AB_W },
			{
				{ { "Pearl Battery", "Up to 3 turrets at a time." }, { "Big Pearls", "Turrets deal 50% more." } },
				{ { "Citadel", "Fortress lasts 12 s." }, { "Mobile Fortress", "You can walk (slowly) during Fortress." } },
				{ { "Cluster Pearl", "Pearl Cannon lands three times." }, { "Mother of Pearl", "Clam Shield also shields your towers 250." } },
			},
			{ ITEM_THICK_SHELL, ITEM_PEARL_CHARM, ITEM_TOWER_BUSTER, ITEM_CORAL_ARMOR, ITEM_SEA_CROWN, ITEM_GOLDEN_SCALE },
		},
		{
			"Meryl", "Singer", "A mermaid diva whose songs put enemies to sleep and rally her side.",
			false, 470, 26, 210, 215, 1.0f, 34, 520,
			"Fan Club", "Your fish drop coins 15% faster.",
			{
				// Lullaby: enemies in a cone (mRange long, mRadius degrees each side) sleep mDurationS (damage wakes them).
				{ "Lullaby", "Sing enemies in front of you to sleep (damage wakes them).", 12, AIM_CONE, 300, 0, 28, 1.6f, 0 },
				// Anthem: for mDurationS +mDamage speed and attack speed; own minions within mRadius +mExtra damage.
				{ "Anthem", "For 5 s: faster moves and attacks; your minions near you hit harder.", 14, AIM_SELF, 0, 0.3f, 300, 5, 0.3f },
				// High Note: a wave mRange long and mRadius wide: mDamage to everything in it, pushing it mExtra.
				{ "High Note", "A piercing note that hurts and pushes back everything in a line.", 8, AIM_DIR, 500, 70, 34, 0, 80 },
				// Siren Song: enemies within mRadius are pulled in and held mDurationS (mDamage); enemy minions switch sides mExtra s.
				{ "Siren Song", "Pull in and hold every enemy near you; their minions switch sides.", 80, AIM_SELF, 0, 40, 320, 1.5f, 6 },
			},
			{ AB_Q, AB_E, AB_Q, AB_W, AB_Q, AB_E, AB_W, AB_E, AB_W },
			{
				{ { "Deep Sleep", "Lullaby lasts 2.4 s." }, { "Wide Lullaby", "Lullaby's cone is twice as wide." } },
				{ { "Encore", "Siren Song's minions stay yours for 10 s." }, { "Showstopper", "Siren Song holds heroes for 2.5 s." } },
				{ { "Power Ballad", "High Note deals 60% more." }, { "Standing Ovation", "Anthem heals you 20% of your health." } },
			},
			{ ITEM_PEARL_CHARM, ITEM_THICK_SHELL, ITEM_SEA_CROWN, ITEM_GOLDEN_SCALE, ITEM_INK_SAC, ITEM_CORAL_ARMOR },
		},
		{
			"Shrapnel", "Artillery", "A walking bomb of a fish: lobbed bombs, hidden mines and a missile barrage.",
			false, 500, 32, 230, 210, 1.15f, 34, 500,
			"Volatile", "+20% damage to towers and the core; explodes for 150 when he dies.",
			{
				// Lob Bomb: lands at the aim after mDurationS: mDamage within mRadius (over walls).
				{ "Lob Bomb", "Lob a bomb over walls: it bursts where it lands.", 7, AIM_POINT, 420, 65, 85, 0.7f, 0 },
				// Sea Mine: a mine near you (reach mRange), armed after 1 s, for mDurationS: mDamage within mRadius, mExtra slow.
				{ "Sea Mine", "Hide a mine (3 at a time): it bursts and slows when an enemy touches it.", 8, AIM_POINT, 150, 110, 80, 40, 0.3f },
				// Blast Jump: mDamage within mRadius at your feet (knockback mExtra), then fly mRange toward the aim.
				{ "Blast Jump", "Blow up at your feet and fly away.", 12, AIM_DIR, 300, 60, 100, 0.3f, 90 },
				// Missile Barrage: mExtra missiles over mDurationS within mRadius of the aim, mDamage each.
				{ "Missile Barrage", "Rain 8 missiles on an area; double damage to towers.", 75, AIM_POINT, 700, 60, 200, 2, 8 },
			},
			{ AB_Q, AB_W, AB_Q, AB_E, AB_Q, AB_W, AB_E, AB_W, AB_E },
			{
				{ { "Big Bang", "Lob Bomb bursts 40% wider." }, { "Quick Fuse", "Lob Bomb: 2 s shorter cooldown, lands faster." } },
				{ { "Carpet Bombing", "Missile Barrage fires 14 missiles." }, { "Bunker Buster", "Missiles deal triple damage to towers." } },
				{ { "Minefield", "Up to 6 mines at a time." }, { "Rocket Jump", "Blast Jump flies twice as far; 6 s cooldown." } },
			},
			{ ITEM_SEA_CROWN, ITEM_PEARL_CHARM, ITEM_TOWER_BUSTER, ITEM_THICK_SHELL, ITEM_INK_SAC, ITEM_GOLDEN_SCALE },
		},
	};

	const HeroDef& HeroDefOf(int theHero) { return kHeroes[std::clamp(theHero, 0, (int)HERO_COUNT - 1)]; }

	int		XpForLevel(int theLevel) { return 130 + 70 * theLevel; }
	float	LevelScale(int theLevel) { return 1.0f + 0.08f * (std::max(1, theLevel) - 1); }
	float	RankScale(int theRank) { return 1.0f + 0.25f * (std::max(1, theRank) - 1); }
	float	RankCooldown(int theRank) { return 1.0f - 0.08f * (std::max(1, theRank) - 1); }
	float	RespawnS(int theLevel) { return 5.0f + 1.6f * theLevel; }

	///////////////////////////////////////////////////////////////////////////
	// Items
	///////////////////////////////////////////////////////////////////////////
	static const ItemDef kItems[ITEM_COUNT] =
	{
		//  name               desc                                                                               price dmg  hp   speed  cdr    struct life   armor gold  ability
		{ "Sharp Fin",      "+15 attack damage.",                                                                    300, 15, 0,   0,     0,     0,    0,     0,    0,    0 },
		{ "Thick Shell",    "+200 health.",                                                                          300, 0,  200, 0,     0,     0,    0,     0,    0,    0 },
		{ "Speed Kelp",     "+15% speed, and another +20% when you haven't been hurt for 3 s.",                      350, 0,  0,   0.15f, 0,     0,    0,     0,    0,    0 },
		{ "Pearl Charm",    "Abilities cool down 20% faster.",                                                       450, 0,  0,   0,     0.20f, 0,    0,     0,    0,    0 },
		{ "Electric Scale", "+10 damage; every 3rd attack chains to 2 more enemies.",                                650, 10, 0,   0,     0,     0,    0,     0,    0,    0 },
		{ "Ink Sac",        "+150 health; below 30% health you vanish in ink for a moment and swim fast (60 s).",    600, 0,  150, 0,     0,     0,    0,     0,    0,    0 },
		{ "Kraken Tooth",   "+10 damage; attacks deal +3% of the target's health and slow it.",                      800, 10, 0,   0,     0,     0,    0,     0,    0,    0 },
		{ "Leech Tooth",    "Heal 15% of the damage your attacks deal (8% from abilities).",                         550, 0,  0,   0,     0,     0,    0.15f, 0,    0,    0 },
		{ "Coral Armor",    "+150 health; 25% less damage from towers, lasers, minions and monsters.",               650, 0,  150, 0,     0,     0,    0,     0.25f, 0,   0 },
		{ "Tower Buster",   "+45% damage to towers and the core.",                                                   600, 0,  0,   0,     0,     0.45f, 0,    0,    0,    0 },
		{ "Sea Crown",      "Abilities deal 25% more and cool down 10% faster.",                                     850, 0,  0,   0,     0.10f, 0,    0,     0,    0,    0.25f },
		{ "Golden Scale",   "+$2 a second; coins you grab in the Trench are worth 50% more; +6 damage, +80 health.", 800, 6,  80,  0,     0,     0,    0,     0,    2.0f, 0 },
	};
	const ItemDef& ItemDefOf(int theItem) { return kItems[std::clamp(theItem, 0, (int)ITEM_COUNT - 1)]; }

	///////////////////////////////////////////////////////////////////////////
	// The farm
	///////////////////////////////////////////////////////////////////////////
	static const FishDef kFish[FISH_KIND_COUNT] =
	{
		{ "Guppy",     100, 60, 20, 15, { 18, 26, 34 } },
		{ "Breeder",   300, 45, 25, 20, { 30, 30, 30 } },
		{ "Carnivore", 900, 75, 30, 30, { 36, 36, 36 } },
	};
	const FishDef& FishDefOf(int theKind) { return kFish[std::clamp(theKind, 0, (int)FISH_KIND_COUNT - 1)]; }

	int CoinValue(int theKind)
	{
		switch (theKind)
		{
		case COIN_SILVER: return 15;
		case COIN_GOLD: return 35;
		default: return 200;
		}
	}

	float LaserDamage(int theLevel)
	{
		static const float kDamage[kLaserLevels] = { 15, 30, 50, 80 };
		return kDamage[std::clamp(theLevel, 0, kLaserLevels - 1)];
	}
	int LaserUpgradePrice(int theLevel)
	{
		static const int kPrice[kLaserLevels] = { 800, 1600, 3200, 0 };
		return kPrice[std::clamp(theLevel, 0, kLaserLevels - 1)];
	}

	///////////////////////////////////////////////////////////////////////////
	// Minions and monsters
	///////////////////////////////////////////////////////////////////////////
	static const MinionDef kMinions[MIN_KIND_COUNT] =
	{
		//  name               hp    struct hero  hitS  speed  r   range bounty  xp   price  eats   coins
		{ "Mini Sylvester",    80,   24,    20,   1.0f, 90,   22, 40,   15,     25,  0,     false, 14 },
		{ "Sylvester",        260,   40,    45,   1.2f, 75,   42, 55,   100,    60,  400,   true,  50 },
		{ "Gus",              600,   25,    40,   1.2f, 55,   55, 60,   180,    60,  700,   true,  70 },
		{ "Balrog",           520,   50,    60,   1.3f, 60,   55, 60,   220,    60,  900,   true,  70 },
		{ "Destructor",       450,   70,    40,   2.0f, 45,   50, 260,  250,    60,  1100,  false, 90 },
		{ "Psychosquid",     2000,   90,    50,   1.4f, 50,   58, 70,   150,    120, 0,     false, 80 },
		{ "Gus (camp)",       500,   0,     30,   1.2f, 80,   55, 60,   0,      120, 0,     false, 80 },
		{ "Balrog (camp)",    450,   0,     34,   1.3f, 85,   55, 60,   0,      120, 0,     false, 80 },
		{ "Psychosquid",     2400,   0,     55,   1.5f, 80,   70, 80,   0,      220, 0,     false, 150 },
		{ "The Boss",        4800,   0,     80,   1.8f, 60,   90, 100,  0,      350, 0,     false, 250 },
	};
	const MinionDef& MinionDefOf(int theKind) { return kMinions[std::clamp(theKind, 0, (int)MIN_KIND_COUNT - 1)]; }

	int TowerRepairPrice() { return 250; }
	int TowerUpgradePrice(int theLevel) { static const int k[kTowerMaxLevel] = { 600, 1200, 2000 }; return k[std::clamp(theLevel, 0, kTowerMaxLevel - 1)]; }
	int WaveSizePrice(int theRank) { static const int k[kWaveUpgradeRanks] = { 500, 800, 1200 }; return k[std::clamp(theRank, 0, kWaveUpgradeRanks - 1)]; }
	int WaveToughPrice(int theRank) { static const int k[kWaveUpgradeRanks] = { 600, 900, 1300 }; return k[std::clamp(theRank, 0, kWaveUpgradeRanks - 1)]; }

	///////////////////////////////////////////////////////////////////////////
	// The shop
	///////////////////////////////////////////////////////////////////////////
	static ShopDef gShop[SHOP_COUNT];
	static bool gShopInit = false;

	const ShopDef& ShopDefOf(int theShop)
	{
		if (!gShopInit)
		{
			gShopInit = true;
			gShop[SHOP_GUPPY] = { TAB_FISH, "Guppy", "Grows as it eats; drops silver, then gold coins." };
			gShop[SHOP_BREEDER] = { TAB_FISH, "Breeder", "When fed, lays a baby guppy every 30 s." };
			gShop[SHOP_CARNIVORE] = { TAB_FISH, "Carnivore", "Eats a small guppy now and then; drops diamonds worth 200." };
			gShop[SHOP_FOOD_QUALITY] = { TAB_UPGRADES, "Food Quality", "Pellets feed more: fish grow faster and stay fed longer." };
			gShop[SHOP_FOOD_COUNT] = { TAB_UPGRADES, "Food Quantity", "One more pellet can be in the water at once." };
			gShop[SHOP_LASER] = { TAB_UPGRADES, "Laser", "Your clicks hit aliens (and the enemy hero) harder." };
			gShop[SHOP_COLLECTOR] = { TAB_UPGRADES, "Stinky", "A pet snail that crawls the floor collecting coins for you. Level 2: twice as fast." };
			for (int i = 0; i < ITEM_COUNT; i++)
				gShop[SHOP_ITEM_FIRST + i] = { TAB_HERO, ItemDefOf(i).mName, ItemDefOf(i).mDesc };
			gShop[SHOP_REPAIR_LEFT] = { TAB_TOWERS, "Repair Left Tower", "+500 health (not while it's under attack)." };
			gShop[SHOP_REPAIR_RIGHT] = { TAB_TOWERS, "Repair Right Tower", "+500 health (not while it's under attack)." };
			gShop[SHOP_TOWER_UPGRADE] = { TAB_TOWERS, "Tower Upgrade", "Both towers: +25% damage and +40 range." };
			gShop[SHOP_WAVE_SIZE] = { TAB_MINIONS, "Bigger Waves", "+1 mini Sylvester in every wave you send." };
			gShop[SHOP_WAVE_TOUGH] = { TAB_MINIONS, "Tougher Minions", "+25% health and damage for your minions." };
			gShop[SHOP_SEND_SYLV] = { TAB_MINIONS, "Send Sylvester", "Joins your next wave." };
			gShop[SHOP_SEND_GUS] = { TAB_MINIONS, "Send Gus", "Very tough; eats fish when he gets there. Joins your next wave." };
			gShop[SHOP_SEND_BALROG] = { TAB_MINIONS, "Send Balrog", "Hits towers hard. Joins your next wave." };
			gShop[SHOP_SEND_DESTRUCTOR] = { TAB_MINIONS, "Send Destructor", "Shells towers from a distance. Joins your next wave." };
		}
		return gShop[std::clamp(theShop, 0, (int)SHOP_COUNT - 1)];
	}

	///////////////////////////////////////////////////////////////////////////
	// The tanks: portal top middle (into the Trench), towers on ledges left and right,
	// the core at the bottom middle; rocks, wreck pieces and coral as walls; kelp at the
	// sides and by the core. Everything is mirrored left/right.
	///////////////////////////////////////////////////////////////////////////
	static std::vector<Vec> MirrorPoly(const std::vector<Vec>& p)
	{
		std::vector<Vec> r;
		for (auto it = p.rbegin(); it != p.rend(); ++it)
			r.push_back(Vec(kWorldW - it->x, it->y));
		return r;
	}

	static MapDef MakeTank()
	{
		MapDef m;
		m.mFloor = {
			{ 0, 800 }, { 80, 800 }, { 160, 705 }, { 340, 705 }, { 420, 800 },
			{ 450, 800 }, { 470, 768 }, { 520, 768 }, { 540, 800 },
			{ 740, 800 }, { 760, 768 }, { 810, 768 }, { 830, 800 },
			{ 860, 800 }, { 940, 705 }, { 1120, 705 }, { 1200, 800 }, { 1280, 800 },
		};
		std::vector<Vec> aRock = { { 340, 270 }, { 400, 248 }, { 462, 262 }, { 478, 305 }, { 440, 338 }, { 372, 334 }, { 336, 308 } };
		std::vector<Vec> aWreck = { { 392, 508 }, { 512, 490 }, { 522, 538 }, { 402, 556 } };
		std::vector<Vec> aCoral = { { 600, 474 }, { 622, 436 }, { 660, 432 }, { 684, 472 }, { 662, 504 }, { 618, 504 } };
		m.mWalls.push_back({ aRock, 0 });
		m.mWalls.push_back({ MirrorPoly(aRock), 1 });
		m.mWalls.push_back({ aWreck, 2 });
		m.mWalls.push_back({ MirrorPoly(aWreck), 3 });
		m.mWalls.push_back({ aCoral, 4 });
		m.mKelp = {
			{ 36, 170, 146, 560 }, { kWorldW - 146, 170, kWorldW - 36, 560 },
			{ 436, 630, 500, 800 }, { kWorldW - 500, 630, kWorldW - 436, 800 },
		};
		m.mPortal = Vec(640, 150);
		m.mPortalR = 58;
		m.mPortalExit = Vec(640, 252);
		m.mPad[0] = Vec(42, 800);
		m.mPad[1] = Vec(kWorldW - 42, 800);
		m.mPadR = 34;
		m.mCore = Vec(640, 745);
		m.mTower[0] = Vec(250, 640);
		m.mTower[1] = Vec(kWorldW - 250, 640);
		m.mLane[0] = { { 640, 170 }, { 520, 200 }, { 330, 232 }, { 216, 390 }, { 250, 560 } };
		for (const Vec& v : m.mLane[0])
			m.mLane[1].push_back(Vec(kWorldW - v.x, v.y));
		return m;
	}

	///////////////////////////////////////////////////////////////////////////
	// The Trench (D36): a gate low at each end, the lane across the lower water, the
	// Gus camp up on the left and the Balrog camp up on the right (each in reach of a
	// walker standing under it), the pit in the middle for the Squid and the Boss, two
	// rocks for cover and kelp in the upper corners and above the pit.
	///////////////////////////////////////////////////////////////////////////
	static MapDef MakeTrench()
	{
		MapDef m;
		m.mTrench = true;
		m.mFloor = {
			{ 0, 790 }, { 190, 790 }, { 290, 768 }, { 380, 768 }, { 470, 795 },
			{ 570, 806 }, { 640, 812 }, { 710, 806 },
			{ 810, 795 }, { 900, 768 }, { 990, 768 }, { 1090, 790 }, { 1280, 790 },
		};
		std::vector<Vec> aRock = { { 452, 268 }, { 512, 246 }, { 560, 262 }, { 572, 306 }, { 530, 336 }, { 470, 330 }, { 446, 302 } };
		std::vector<Vec> aLedge = { { 170, 392 }, { 250, 378 }, { 262, 404 }, { 180, 420 } };
		m.mWalls.push_back({ aRock, 0 });
		m.mWalls.push_back({ MirrorPoly(aRock), 1 });
		m.mWalls.push_back({ aLedge, 2 });
		m.mWalls.push_back({ MirrorPoly(aLedge), 3 });
		m.mKelp = {
			{ 30, 120, 150, 340 }, { kWorldW - 150, 120, kWorldW - 30, 340 },
			{ 585, 96, 695, 250 },
		};
		m.mGate[0] = Vec(64, 700);
		m.mGate[1] = Vec(kWorldW - 64, 700);
		m.mGateR = 62;
		m.mGateExit[0] = Vec(190, 640);
		m.mGateExit[1] = Vec(kWorldW - 190, 640);
		m.mTrenchLane = { { 110, 700 }, { 330, 680 }, { 640, 700 }, { 950, 680 }, { 1170, 700 } };
		m.mCamp[0] = Vec(330, 470);
		m.mCamp[1] = Vec(kWorldW - 330, 470);
		m.mPit = Vec(640, 455);
		return m;
	}

	const MapDef& TankMap()
	{
		static const MapDef kMap = MakeTank();
		return kMap;
	}

	const MapDef& TrenchMap()
	{
		static const MapDef kMap = MakeTrench();
		return kMap;
	}

	Vec MonsterHome(int theSlot)
	{
		const MapDef& m = TrenchMap();
		switch (theSlot)
		{
		case MON_GUS: return m.mCamp[0];
		case MON_BALROG: return m.mCamp[1];
		default: return m.mPit;
		}
	}
}
