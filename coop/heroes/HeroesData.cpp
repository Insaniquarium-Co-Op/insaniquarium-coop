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
			false, 520, 34, 70, 240, 0.9f, 34, 0,
			"Bloodlust", "Each hit on the same target: +8% attack speed (up to 5).",
			{
				// Lunge: dash mRange toward the aim, mDamage to the first enemy within mRadius.
				{ "Lunge", "Dash forward and strike the first enemy you hit.", 7, AIM_DIR, 260, 60, 45, 0.25f, 0 },
				// Sharpen: the next mExtra attacks within mDurationS deal +mDamage.
				{ "Sharpen", "Your next 3 attacks deal bonus damage.", 10, AIM_SELF, 0, 40, 0, 6, 3 },
				// Evade: untargetable for mDurationS, +mDamage speed for mExtra seconds.
				{ "Evade", "Vanish for a moment and swim away fast.", 14, AIM_SELF, 0, 0.5f, 0, 1, 2 },
				// Swordstorm: mDamage every mExtra seconds to enemies within mRadius for mDurationS.
				{ "Swordstorm", "Spin for 3 s, cutting everything around you.", 45, AIM_SELF, 0, 45, 110, 3, 0.3f },
			},
			{ AB_Q, AB_W, AB_Q, AB_E, AB_Q, AB_W, AB_E, AB_W, AB_E },
		},
		{
			"Clyde", "Mage", "A jellyfish who zaps from a distance and controls the water.",
			false, 420, 26, 210, 200, 1.1f, 32, 600,
			"Static", "Every 4th attack chains to 2 more enemies.",
			{
				// Zap: a bolt flying mRange at mExtra speed, mDamage to the first enemy (hit radius mRadius).
				{ "Zap", "A lightning bolt that hits the first enemy in its path.", 5, AIM_DIR, 520, 95, 26, 0, 650 },
				// Static Field: zone of mRadius at the aim for mDurationS: mDamage per second, mExtra slow.
				{ "Static Field", "An electric zone that slows and hurts enemies.", 12, AIM_POINT, 380, 28, 120, 3, 0.4f },
				// Drift: teleport up to mRange toward the aim.
				{ "Drift", "Teleport a short distance.", 12, AIM_POINT, 220, 0, 0, 0, 0 },
				// Thunderstorm: mExtra strikes over mDurationS on every enemy in the tank, mDamage each.
				{ "Thunderstorm", "Lightning strikes every enemy in the tank 4 times.", 90, AIM_SELF, 0, 70, 0, 2.0f, 4 },
			},
			{ AB_Q, AB_W, AB_Q, AB_E, AB_Q, AB_W, AB_W, AB_E, AB_E },
		},
		{
			"Rhubarb", "Tank", "A hermit crab who walks the floor, leaps in and holds the line.",
			true, 780, 46, 80, 190, 0.95f, 38, 0,
			"Shell", "Takes 30% less damage from towers and lasers.",
			{
				// Leap: jump to a floor spot within mRange (flight mDurationS), mDamage and an mExtra-second stun within mRadius.
				{ "Leap", "Jump to a spot, even up onto a ledge, stunning enemies where you land.", 9, AIM_POINT, 320, 70, 100, 0.6f, 1.0f },
				// Claw Grab: the first enemy within mRadius of a line mRange long is pulled to you and takes mDamage.
				{ "Claw Grab", "Pull the first enemy in a line to you.", 12, AIM_DIR, 380, 50, 36, 0, 0 },
				// Shell Up: immune for mDurationS; enemy minions and towers target you.
				{ "Shell Up", "Hide in your shell: immune for 2 s, and enemies attack you.", 16, AIM_SELF, 0, 0, 0, 2, 0 },
				// Tidal Slam: mDamage and an mExtra-second stun within mRadius, knocking enemies away.
				{ "Tidal Slam", "Slam the floor: stun and knock back everything around you.", 70, AIM_SELF, 0, 160, 220, 0, 1.5f },
			},
			{ AB_Q, AB_E, AB_W, AB_Q, AB_E, AB_Q, AB_W, AB_E, AB_W },
		},
		{
			"Angie", "Support", "An angelfish who heals, shields and turns enemy minions to her side.",
			false, 500, 27, 220, 220, 1.0f, 32, 550,
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
		},
		{
			"Stinky", "Farmer", "A snail who hoovers up coins, spits slime, slimes the floor and gasses towers.",
			true, 680, 32, 230, 170, 1.1f, 34, 520,
			"Scavenger", "Collects your coins he touches, and +$1 per second.",
			{
				// Slime Trail: for mDurationS he leaves slime (mRadius wide) that slows enemies by mExtra.
				{ "Slime Trail", "Leave a trail of slime that slows enemies.", 10, AIM_SELF, 0, 0, 40, 4, 0.5f },
				// Stink Cloud: cloud of mRadius at the aim for mDurationS, mDamage per second; towers inside can't shoot.
				{ "Stink Cloud", "A cloud that hurts enemies and stops towers inside it from shooting.", 12, AIM_POINT, 300, 35, 140, 3, 0 },
				// Shell Up: immune for mDurationS.
				{ "Shell Up", "Hide in your shell: immune for 2 s.", 15, AIM_SELF, 0, 0, 0, 2, 0 },
				// Gold Rush: for mDurationS your fish drop double coins and every coin flies to you.
				{ "Gold Rush", "For 8 s your fish drop double coins and every coin flies to you.", 75, AIM_SELF, 0, 0, 0, 8, 0 },
			},
			{ AB_W, AB_Q, AB_W, AB_E, AB_W, AB_Q, AB_Q, AB_E, AB_E },
		},
	};

	const HeroDef& HeroDefOf(int theHero) { return kHeroes[std::clamp(theHero, 0, (int)HERO_COUNT - 1)]; }

	int		XpForLevel(int theLevel) { return 200 + 110 * theLevel; }
	float	LevelScale(int theLevel) { return 1.0f + 0.08f * (std::max(1, theLevel) - 1); }
	float	RankScale(int theRank) { return 1.0f + 0.25f * (std::max(1, theRank) - 1); }
	float	RankCooldown(int theRank) { return 1.0f - 0.08f * (std::max(1, theRank) - 1); }
	float	RespawnS(int theLevel) { return 6.0f + 2.0f * theLevel; }

	///////////////////////////////////////////////////////////////////////////
	// Items
	///////////////////////////////////////////////////////////////////////////
	static const ItemDef kItems[ITEM_COUNT] =
	{
		{ "Sharp Fin",    "+15 attack damage.",                      350, 15, 0,   0,     0,     0,    0,    0,    0 },
		{ "Thick Shell",  "+180 health.",                            350, 0,  180, 0,     0,     0,    0,    0,    0 },
		{ "Speed Kelp",   "+20% move speed.",                        300, 0,  0,   0.20f, 0,     0,    0,    0,    0 },
		{ "Pearl Charm",  "Abilities cool down 15% faster.",         400, 0,  0,   0,     0.15f, 0,    0,    0,    0 },
		{ "Tower Buster", "+40% damage to towers and the core.",     600, 0,  0,   0,     0,     0.4f, 0,    0,    0 },
		{ "Leech Tooth",  "Heal 15% of the damage you deal.",        550, 0,  0,   0,     0,     0,    0.15f, 0,   0 },
		{ "Coral Armor",  "20% less damage from towers and lasers.", 700, 0,  0,   0,     0,     0,    0,    0.2f, 0 },
		{ "Golden Scale", "A little of everything, and +$1.5 a second.", 900, 8, 90, 0.08f, 0.08f, 0.1f, 0.05f, 0.05f, 1.5f },
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
	// Minions and structures
	///////////////////////////////////////////////////////////////////////////
	static const MinionDef kMinions[MIN_KIND_COUNT] =
	{
		//  name            hp   struct hero  hitS  speed  r   range bounty  xp  price  eats
		{ "Mini Sylvester",  80, 12,    20,   1.0f, 90,   22, 40,   15,     25, 0,     false },
		{ "Sylvester",      260, 30,    45,   1.2f, 75,   42, 55,   100,    60, 400,   true },
		{ "Gus",            600, 25,    40,   1.2f, 55,   55, 60,   180,    60, 700,   true },
		{ "Balrog",         520, 50,    60,   1.3f, 60,   55, 60,   220,    60, 900,   true },
		{ "Destructor",     450, 70,    40,   2.0f, 45,   50, 260,  250,    60, 1100,  false },
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
			for (int i = 0; i < ITEM_COUNT; i++)
				gShop[SHOP_ITEM_FIRST + i] = { TAB_HERO, ItemDefOf(i).mName, ItemDefOf(i).mDesc };
			gShop[SHOP_REPAIR_LEFT] = { TAB_TOWERS, "Repair Left Tower", "+500 health (not while it's under attack)." };
			gShop[SHOP_REPAIR_RIGHT] = { TAB_TOWERS, "Repair Right Tower", "+500 health (not while it's under attack)." };
			gShop[SHOP_TOWER_UPGRADE] = { TAB_TOWERS, "Tower Upgrade", "Both towers: +25% damage and +40 range." };
			gShop[SHOP_WAVE_SIZE] = { TAB_MINIONS, "Bigger Waves", "+1 mini Sylvester in every wave you send." };
			gShop[SHOP_WAVE_TOUGH] = { TAB_MINIONS, "Tougher Minions", "+25% health and damage for your minions." };
			gShop[SHOP_SEND_SYLV] = { TAB_MINIONS, "Send Sylvester", "Joins your next wave." };
			gShop[SHOP_SEND_GUS] = { TAB_MINIONS, "Send Gus", "Very tough; eats fish on the way. Joins your next wave." };
			gShop[SHOP_SEND_BALROG] = { TAB_MINIONS, "Send Balrog", "Hits towers hard. Joins your next wave." };
			gShop[SHOP_SEND_DESTRUCTOR] = { TAB_MINIONS, "Send Destructor", "Shells towers from a distance. Joins your next wave." };
		}
		return gShop[std::clamp(theShop, 0, (int)SHOP_COUNT - 1)];
	}

	///////////////////////////////////////////////////////////////////////////
	// The map: portal top middle, towers on ledges left and right, the core at the
	// bottom middle; rocks, wreck pieces and coral as walls; kelp at the sides and by
	// the core. Everything is mirrored left/right.
	///////////////////////////////////////////////////////////////////////////
	static MapDef MakeMap()
	{
		MapDef m;
		m.mFloor = {
			{ 0, 800 }, { 80, 800 }, { 160, 705 }, { 340, 705 }, { 420, 800 },
			{ 450, 800 }, { 470, 768 }, { 520, 768 }, { 540, 800 },
			{ 740, 800 }, { 760, 768 }, { 810, 768 }, { 830, 800 },
			{ 860, 800 }, { 940, 705 }, { 1120, 705 }, { 1200, 800 }, { 1280, 800 },
		};
		auto Mirror = [](const std::vector<Vec>& p) {
			std::vector<Vec> r;
			for (auto it = p.rbegin(); it != p.rend(); ++it)
				r.push_back(Vec(kWorldW - it->x, it->y));
			return r;
		};
		std::vector<Vec> aRock = { { 340, 270 }, { 400, 248 }, { 462, 262 }, { 478, 305 }, { 440, 338 }, { 372, 334 }, { 336, 308 } };
		std::vector<Vec> aWreck = { { 392, 508 }, { 512, 490 }, { 522, 538 }, { 402, 556 } };
		std::vector<Vec> aCoral = { { 600, 474 }, { 622, 436 }, { 660, 432 }, { 684, 472 }, { 662, 504 }, { 618, 504 } };
		m.mWalls.push_back({ aRock, 0 });
		m.mWalls.push_back({ Mirror(aRock), 1 });
		m.mWalls.push_back({ aWreck, 2 });
		m.mWalls.push_back({ Mirror(aWreck), 3 });
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

	const MapDef& TheMap()
	{
		static const MapDef kMap = MakeMap();
		return kMap;
	}
}
