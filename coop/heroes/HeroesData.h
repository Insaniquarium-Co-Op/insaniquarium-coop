// Pet Heroes - every number in the mode: heroes and their abilities, items, fish,
// coins, minions, towers, the core, the shop and the arena's layout. First guesses
// from docs/PET_HEROES.md, tuned with bot-vs-bot matches (tests/heroes).

#ifndef __HEROES_DATA_H__
#define __HEROES_DATA_H__

#include "HeroesMath.h"
#include <cstdint>
#include <vector>

namespace Heroes
{
	static const int	kTickMs = 28;				// the game's update period
	static const float	kWorldW = 1280, kWorldH = 848;
	static const float	kSurfaceY = 56;				// swimmers stay below the water line
	static const int	kTeams = 2;
	static const int	kMaxPlayers = 4;			// 2v2 later; 1v1 uses players 0 and 1

	///////////////////////////////////////////////////////////////////////////
	// Heroes
	///////////////////////////////////////////////////////////////////////////
	enum HeroId : uint8_t { HERO_ITCHY, HERO_CLYDE, HERO_RHUBARB, HERO_ANGIE, HERO_STINKY, HERO_COUNT };
	static const int AB_Q = 0, AB_W = 1, AB_E = 2, AB_R = 3, AB_COUNT = 4;	// ability slots

	// How an ability is aimed.
	enum AimKind : uint8_t
	{
		AIM_SELF,		// no aim
		AIM_POINT,		// a spot (clamped to range)
		AIM_DIR,		// a direction from the hero
		AIM_FRIEND,		// the hero, or an own tower/core near the mouse
	};

	struct AbilityDef
	{
		const char*	mName;
		const char*	mDesc;
		float		mCooldownS;
		AimKind		mAim;
		float		mRange;		// reach of the aim
		float		mDamage;	// main number (damage, heal, shield...)
		float		mRadius;	// area or hit radius
		float		mDurationS;	// how long it lasts
		float		mExtra;		// ability-specific (slow %, stun s, ...)
	};

	struct HeroDef
	{
		const char*	mName;
		const char*	mRole;
		const char*	mBlurb;
		bool		mWalker;		// walks the floor (Rhubarb, Stinky); else swims
		float		mHealth;
		float		mDamage;		// auto-attack
		float		mRange;			// auto-attack reach (walkers: horizontal)
		float		mSpeed;			// units per second
		float		mAttackS;		// seconds between auto-attacks
		float		mRadius;		// body
		float		mProjectile;	// auto-attack projectile speed (0 = melee)
		const char*	mPassiveName;
		const char*	mPassive;
		AbilityDef	mAb[AB_COUNT];
		uint8_t		mRankOrder[9];	// which of Q/W/E gets each level-up point
	};

	const HeroDef& HeroDefOf(int theHero);

	// Leveling.
	static const int	kMaxLevel = 10;
	static const int	kMaxRank = 4;				// Q/W/E: rank 1 at level 1, +3 from level-ups
	static const int	kUltLevel = 5, kUlt2Level = 8;
	int		XpForLevel(int theLevel);				// XP to go from theLevel to theLevel+1
	float	LevelScale(int theLevel);				// health and damage multiplier
	float	RankScale(int theRank);					// ability damage multiplier per rank
	float	RankCooldown(int theRank);				// ability cooldown multiplier per rank
	static const float	kXpPerSecond = 2.0f;
	static const float	kXpMinion = 25, kXpBigAlien = 60, kXpHeroKill = 150, kXpHeroKillPerLevel = 30, kXpTower = 250, kXpFish = 6;
	static const int	kBountyHeroKill = 150, kBountyHeroKillPerLevel = 30, kBountyTower = 300, kBountyFish = 10;
	float	RespawnS(int theLevel);
	static const float	kFountainRadius = 230, kFountainHealPct = 0.06f;	// of max health per second
	static const float	kWarpSicknessS = 8;			// after crossing, no crossing back for this long
	static const float	kHomeDamage = 1.2f, kHomeArmor = 0.8f;	// home waters: a hero fights better in its own tank
	static const float	kHeroVsFish = 0.6f;						// heroes are slow fishermen
	static const float	kAutoRankS = 4;				// an unspent point spends itself after this

	///////////////////////////////////////////////////////////////////////////
	// Items
	///////////////////////////////////////////////////////////////////////////
	enum ItemId : uint8_t
	{
		ITEM_SHARP_FIN, ITEM_THICK_SHELL, ITEM_SPEED_KELP, ITEM_PEARL_CHARM,
		ITEM_TOWER_BUSTER, ITEM_LEECH_TOOTH, ITEM_CORAL_ARMOR, ITEM_GOLDEN_SCALE,
		ITEM_COUNT, ITEM_NONE = 255
	};
	static const int kItemSlots = 6;

	struct ItemDef
	{
		const char*	mName;
		const char*	mDesc;
		int			mPrice;
		float		mDamage;		// + attack damage
		float		mHealth;		// + max health
		float		mSpeedPct;		// + move speed
		float		mCdrPct;		// abilities cool down faster
		float		mStructPct;		// + damage to towers and the core
		float		mLifestealPct;	// heal this share of damage dealt
		float		mArmorPct;		// - damage from towers and lasers
		float		mGoldPerS;		// + money per second
	};
	const ItemDef& ItemDefOf(int theItem);

	///////////////////////////////////////////////////////////////////////////
	// The farm
	///////////////////////////////////////////////////////////////////////////
	enum FishKind : uint8_t { FISH_GUPPY, FISH_BREEDER, FISH_CARNIVORE, FISH_KIND_COUNT };
	enum FishSize : uint8_t { SIZE_SMALL, SIZE_MEDIUM, SIZE_LARGE };

	struct FishDef
	{
		const char*	mName;
		int			mPrice;
		float		mSpeed;
		float		mHungryAfterS;	// fed -> hungry
		float		mStarveS;		// hungry -> dead
		float		mRadius[3];		// by size
	};
	const FishDef& FishDefOf(int theKind);
	static const float	kGuppyMealsMedium = 2, kGuppyMealsLarge = 5;
	static const float	kCoinEveryS = 13, kDiamondEveryS = 24, kBreedEveryS = 30, kCarnivoreEatEveryS = 30;
	static const int	kMaxFish = 16;

	enum CoinKind : uint8_t { COIN_SILVER, COIN_GOLD, COIN_DIAMOND, COIN_KIND_COUNT };
	int		CoinValue(int theKind);
	static const float	kCoinFallSpeed = 55, kCoinFloorS = 8, kCoinClickR = 38;
	static const float	kFoodFallSpeed = 70, kFoodFloorS = 3;
	static const int	kStartMoney = 300;
	static const int	kStartGuppies = 2;
	static const int	kStartPellets = 2, kMaxPellets = 6;

	// Laser (your own clicks in your own tank).
	static const int	kLaserLevels = 4;
	float	LaserDamage(int theLevel);			// 15 / 30 / 50 / 80
	int		LaserUpgradePrice(int theLevel);	// to reach theLevel+1
	static const float	kLaserPeriodS = 0.28f, kLaserVsHero = 0.8f, kLaserClickR = 44;

	///////////////////////////////////////////////////////////////////////////
	// Minions, towers, core
	///////////////////////////////////////////////////////////////////////////
	enum MinionKind : uint8_t { MIN_MINI, MIN_SYLV, MIN_GUS, MIN_BALROG, MIN_DESTRUCTOR, MIN_KIND_COUNT };

	struct MinionDef
	{
		const char*	mName;
		float		mHealth;
		float		mStructHit;		// damage per hit to towers/core
		float		mHeroHit;		// damage per hit to heroes
		float		mHitS;			// seconds between hits
		float		mSpeed;
		float		mRadius;
		float		mRange;			// attack reach
		int			mBounty;		// money to whoever kills it
		float		mXp;
		int			mPrice;			// to add one to your next wave (0: waves only)
		bool		mEatsFish;
	};
	const MinionDef& MinionDefOf(int theKind);
	static const float	kWaveFirstS = 45, kWaveEveryS = 30;
	static const int	kWaveSize = 4;
	static const float	kMinionAggroR = 170;		// they turn on heroes this close
	static const float	kMinionBiteS = 3;			// between fish bites

	static const float	kTowerHealth = 3000, kTowerHit = 60, kTowerPeriodS = 1.2f, kTowerRange = 330, kTowerBoltSpeed = 700;
	static const float	kTowerAggroS = 2.5f;		// a hero that hurt my hero is targeted first for this long
	static const float	kCoreHealth = 4500, kCoreArmor = 0.5f, kCoreR = 80;
	static const float	kBackdoor = 0.35f, kBackdoorR = 350;	// heroes hurt structures less without their minions near
	static const float	kTowerRampPerShot = 0.25f;				// each shot at the same hero hits harder...
	static const int	kTowerRampMax = 4;						// ...up to double
	static const float	kTowerR = 46;
	int		TowerRepairPrice();					// +500 health
	static const float	kTowerRepair = 500, kTowerRepairQuietS = 5;
	int		TowerUpgradePrice(int theLevel);	// both towers: +25% damage, +40 range
	static const int	kTowerMaxLevel = 3;
	int		WaveSizePrice(int theRank);
	int		WaveToughPrice(int theRank);
	static const int	kWaveUpgradeRanks = 3;

	// Sudden death.
	static const float	kSuddenDeathS = 15 * 60;
	static const float	kSuddenGrowthPerMin = 0.15f;

	///////////////////////////////////////////////////////////////////////////
	// The shop (B)
	///////////////////////////////////////////////////////////////////////////
	enum ShopTab : uint8_t { TAB_FISH, TAB_UPGRADES, TAB_HERO, TAB_TOWERS, TAB_MINIONS, TAB_COUNT };
	enum ShopId : uint8_t
	{
		SHOP_GUPPY, SHOP_BREEDER, SHOP_CARNIVORE,
		SHOP_FOOD_QUALITY, SHOP_FOOD_COUNT, SHOP_LASER,
		SHOP_ITEM_FIRST, SHOP_ITEM_LAST = SHOP_ITEM_FIRST + ITEM_COUNT - 1,
		SHOP_REPAIR_LEFT, SHOP_REPAIR_RIGHT, SHOP_TOWER_UPGRADE,
		SHOP_WAVE_SIZE, SHOP_WAVE_TOUGH, SHOP_SEND_SYLV, SHOP_SEND_GUS, SHOP_SEND_BALROG, SHOP_SEND_DESTRUCTOR,
		SHOP_COUNT
	};
	struct ShopDef
	{
		ShopTab		mTab;
		const char*	mName;
		const char*	mDesc;
	};
	const ShopDef& ShopDefOf(int theShop);
	static const int	kFoodQualityPrice[2] = { 200, 400 };
	static const int	kFoodCountPrice = 150;

	///////////////////////////////////////////////////////////////////////////
	// The arena's layout (the same in both tanks)
	///////////////////////////////////////////////////////////////////////////
	struct WallDef
	{
		std::vector<Vec>	mPoly;
		int					mArt;		// which cut-out to draw (HeroesDraw)
	};
	struct KelpDef { float mX0, mY0, mX1, mY1; };

	struct MapDef
	{
		std::vector<Vec>	mFloor;			// left to right; y = floor surface
		std::vector<WallDef> mWalls;
		std::vector<KelpDef> mKelp;
		Vec					mPortal;		// the warp hole: minions and swimmers cross here
		float				mPortalR;
		Vec					mPortalExit;	// where a hero arrives through the portal
		Vec					mPad[2];		// floor warp pads (left, right): walkers can cross too
		float				mPadR;
		Vec					mCore;			// middle of the core's body
		Vec					mTower[2];		// middle of each tower's body (0 left, 1 right)
		std::vector<Vec>	mLane[2];		// minion waypoints, portal to tower
	};
	const MapDef& TheMap();
}

#endif
