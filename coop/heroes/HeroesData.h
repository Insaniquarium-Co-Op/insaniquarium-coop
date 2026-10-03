// Pet Heroes - every number in the mode: heroes, their abilities and talents, items,
// fish, coins, minions, monsters, towers, the core, the shop and the three tanks'
// layouts. First guesses from docs/PET_HEROES_3_PLAN.md, tuned with bot-vs-bot matches
// (tests/heroes).

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

	// Arenas: 0 and 1 are the teams' tanks, 2 is the Trench between them (D36), run by
	// team 0's computer (the host).
	static const int	kTrench = 2, kArenaCount = 3;
	static const int	kNeutralTeam = 2;			// the Trench's monsters
	inline bool			IsTank(int theArena) { return theArena == 0 || theArena == 1; }

	///////////////////////////////////////////////////////////////////////////
	// Heroes
	///////////////////////////////////////////////////////////////////////////
	enum HeroId : uint8_t
	{
		HERO_ITCHY, HERO_CLYDE, HERO_RHUBARB, HERO_ANGIE, HERO_SPEEDY,
		HERO_PRESTO, HERO_NIKO, HERO_MERYL, HERO_SHRAPNEL, HERO_COUNT
	};
	static const int AB_Q = 0, AB_W = 1, AB_E = 2, AB_R = 3, AB_COUNT = 4;	// ability slots (keys Q, E, R, F)

	// WASD movement: walkers hop with W (minion attacks and slowing ground miss them in
	// the air) and cross with S near a portal's beam or a floor pad.
	static const float	kHopHeight = 120;
	static const int	kHopMs = 500, kHopCooldownMs = 3000;
	static const float	kCrossAssistR = 150;

	// How an ability is aimed (and what hold-to-aim shows).
	enum AimKind : uint8_t
	{
		AIM_SELF,		// no aim (cast on key press)
		AIM_POINT,		// a spot (clamped to range): a circle of mRadius there
		AIM_DIR,		// a direction from the hero: a line mRange long, mRadius wide
		AIM_FRIEND,		// the hero, or an own tower/core near the mouse
		AIM_CONE,		// a cone mRange long, mRadius degrees to each side
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

	struct TalentDef
	{
		const char*	mName;
		const char*	mDesc;
	};
	static const int	kTalentTiers = 3;
	static const int	kTalentLevel[kTalentTiers] = { 3, 6, 9 };

	struct HeroDef
	{
		const char*	mName;
		const char*	mRole;
		const char*	mBlurb;
		bool		mWalker;		// walks the floor; else swims
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
		uint8_t		mRankOrder[9];	// which of Q/W/E ranks up at levels 2..10
		TalentDef	mTalent[kTalentTiers][2];
		uint8_t		mBuild[6];		// the suggested items, in order
	};

	const HeroDef& HeroDefOf(int theHero);

	// Leveling (D36: 10-12 minute matches).
	static const int	kMaxLevel = 10;
	static const int	kMaxRank = 4;				// Q/W/E: rank 1 at level 1, +3 from level-ups
	static const int	kUltLevel = 6, kUlt2Level = 9;
	static const int	kEvolveLevel = 6;			// the pet evolves: bigger, glowing, F unlocks
	static const float	kEvolveBonus = 0.10f;		// +health and damage once evolved
	static const float	kEvolveScale = 1.2f;		// drawn this much bigger
	int		XpForLevel(int theLevel);				// XP to go from theLevel to theLevel+1
	float	LevelScale(int theLevel);				// health and damage multiplier
	float	RankScale(int theRank);					// ability damage multiplier per rank
	float	RankCooldown(int theRank);				// ability cooldown multiplier per rank
	static const float	kXpPerSecond = 3.0f;
	static const float	kXpMinion = 25, kXpBigAlien = 60, kXpHeroKill = 150, kXpHeroKillPerLevel = 30, kXpTower = 250, kXpFish = 6;
	static const float	kSharedXpR = 500;			// a minion dying in the Trench: XP to enemy heroes this close
	static const int	kBountyHeroKill = 150, kBountyHeroKillPerLevel = 30, kBountyTower = 300, kBountyFish = 10;
	static const int	kShutdownPerKill = 100, kShutdownMax = 400, kShutdownStreak = 3;
	float	RespawnS(int theLevel);
	static const float	kFountainRadius = 230, kFountainHealPct = 0.06f;	// of max health per second
	static const float	kRaidLockS = 6;				// after entering the rival's tank, no leaving for this long
	static const float	kCrossGuardS = 1.5f;		// after any other crossing (no ping-pong)
	static const float	kHomeDamage = 1.2f, kHomeArmor = 0.8f;	// home waters: a hero fights better in its own tank
	static const float	kHeroVsFish = 0.6f;						// heroes are slow fishermen
	static const float	kRevealR = 150;				// enemy heroes this close see you in kelp
	static const float	kWalkerReachUp = 320;		// walkers pounce: they hit things this far above them

	///////////////////////////////////////////////////////////////////////////
	// Items
	///////////////////////////////////////////////////////////////////////////
	enum ItemId : uint8_t
	{
		ITEM_SHARP_FIN, ITEM_THICK_SHELL, ITEM_SPEED_KELP, ITEM_PEARL_CHARM,
		ITEM_ELECTRIC_SCALE, ITEM_INK_SAC, ITEM_KRAKEN_TOOTH, ITEM_LEECH_TOOTH,
		ITEM_CORAL_ARMOR, ITEM_TOWER_BUSTER, ITEM_SEA_CROWN, ITEM_GOLDEN_SCALE,
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
		float		mLifestealPct;	// heal this share of attack damage dealt
		float		mArmorPct;		// - damage from towers, lasers, minions and monsters
		float		mGoldPerS;		// + money per second
		float		mAbilityPct;	// + ability damage
	};
	const ItemDef& ItemDefOf(int theItem);
	// Unique effects (the rest are in the numbers above).
	static const int	kChainEvery = 3;			// Electric Scale: every 3rd attack chains...
	static const float	kChainShare = 0.6f;			// ...for this share of the hit
	static const float	kInkAtHp = 0.3f, kInkUntargetableS = 1.0f, kInkSpeedS = 2.0f, kInkSpeedPct = 0.4f, kInkCooldownS = 60;
	static const float	kKrakenPct = 0.03f, kKrakenSlowPct = 0.2f;
	static const float	kAbilityLifesteal = 0.08f;	// Leech Tooth on abilities
	static const float	kKelpCalmS = 3, kKelpCalmPct = 0.2f;	// Speed Kelp out of combat
	static const float	kGoldenCoinBonus = 0.5f;	// Golden Scale: Trench coins worth more

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
	static const float	kFanClubFaster = 0.15f;		// Meryl's passive: her fish drop coins this much faster
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
	// Minions, monsters, towers, core
	///////////////////////////////////////////////////////////////////////////
	enum MinionKind : uint8_t
	{
		MIN_MINI, MIN_SYLV, MIN_GUS, MIN_BALROG, MIN_DESTRUCTOR,
		MIN_SQUID,							// the Psychosquid's gift: joins the killer's next wave
		MIN_CAMP_GUS, MIN_CAMP_BALROG,		// the Trench's buff camps (team 2)
		MIN_PSYCHO, MIN_BOSS,				// the Trench's objectives (team 2)
		MIN_KIND_COUNT
	};
	inline bool IsMonster(int theKind) { return theKind >= MIN_CAMP_GUS && theKind <= MIN_BOSS; }

	struct MinionDef
	{
		const char*	mName;
		float		mHealth;
		float		mStructHit;		// damage per hit to towers/core
		float		mHeroHit;		// damage per hit to heroes (and other minions)
		float		mHitS;			// seconds between hits
		float		mSpeed;
		float		mRadius;
		float		mRange;			// attack reach
		int			mBounty;		// money to whoever kills it (in a tank)
		float		mXp;
		int			mPrice;			// to add one to your next wave (0: waves only)
		bool		mEatsFish;
		int			mCoins;			// coins it drops when it dies in the Trench
	};
	const MinionDef& MinionDefOf(int theKind);
	static const float	kWaveFirstS = 20, kWaveEveryS = 30;
	static const float	kWaveGrowEveryS = 120;			// waves get one more mini Sylvester every 2 minutes
	static const int	kWaveSize = 4;
	static const float	kMinionAggroR = 170;		// they turn on heroes this close
	static const float	kLaneFightR = 220;			// lane minions fight enemy minions this close
	static const float	kMinionBiteS = 3;			// between fish bites

	// The Trench's monsters (D36).
	enum MonsterSlot : uint8_t { MON_GUS, MON_BALROG, MON_SQUID, MON_BOSS, MON_COUNT };
	static const float	kMonsterFirstS[MON_COUNT] = { 60, 60, 240, 480 };
	static const float	kMonsterRespawnS[MON_COUNT] = { 90, 90, 180, 180 };
	static const float	kLeashR = 300;				// they chase this far from home, then go back and heal
	static const float	kMonsterAggroS = 6;			// they forget a hero this long after its last hit
	static const float	kSquidInkEveryS = 6, kSquidInkR = 170, kSquidInkDamage = 40;
	static const float	kBossSlamEveryS = 7, kBossSlamR = 210, kBossSlamDamage = 100, kBossSlamStunS = 0.6f;

	// Buffs a hero can carry.
	enum BuffId : uint8_t { BUFF_GUS, BUFF_BALROG, BUFF_BOSS, BUFF_COUNT, BUFF_SQUID = 100 };	// the Squid is a wave, not a buff
	static const float	kBuffS[BUFF_COUNT] = { 60, 60, 120 };
	static const float	kGusRegenPct = 0.03f, kGusHealth = 0.15f, kGusScale = 1.15f;
	static const float	kBurnShare = 0.3f, kBurnS = 2.0f;	// Balrog: attacks burn for this much more over this long
	static const float	kBossSurge = 0.4f;					// the Boss: +40% damage, health, attack and move speed

	// Lane coins (D36).
	static const float	kLaneCoinFloorS = 10, kLaneCoinReach = 60;

	static const float	kTowerHealth = 1700, kTowerHit = 60, kTowerPeriodS = 1.2f, kTowerRange = 330, kTowerBoltSpeed = 700;
	static const float	kTowerAggroS = 2.5f;		// a hero that hurt my hero is targeted first for this long
	static const float	kTowerVsMinion = 0.5f;		// towers wear minions down slowly (a surviving wave hurts)
	static const float	kCoreHealth = 2500, kCoreArmor = 0.5f, kCoreR = 80;
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
	static const float	kSuddenDeathS = 12 * 60;
	static const float	kSuddenGrowthPerMin = 0.15f;

	///////////////////////////////////////////////////////////////////////////
	// The shop (B)
	///////////////////////////////////////////////////////////////////////////
	enum ShopTab : uint8_t { TAB_FISH, TAB_UPGRADES, TAB_HERO, TAB_TOWERS, TAB_MINIONS, TAB_COUNT };
	enum ShopId : uint8_t
	{
		SHOP_GUPPY, SHOP_BREEDER, SHOP_CARNIVORE,
		SHOP_FOOD_QUALITY, SHOP_FOOD_COUNT, SHOP_LASER, SHOP_COLLECTOR,
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
	// Stinky, the coin collector pet (D30): crawls the floor picking up landed coins.
	static const int	kCollectorPrice[2] = { 250, 500 };
	static const float	kCollectorSpeed[2] = { 60, 120 };
	static const float	kCollectorReach = 34, kCollectorRadius = 18;
	static const float	kMissedCoinShare = 0.5f;		// a coin nobody took pays this much as it vanishes

	///////////////////////////////////////////////////////////////////////////
	// The layouts: both tanks share one, the Trench has its own
	///////////////////////////////////////////////////////////////////////////
	struct WallDef
	{
		std::vector<Vec>	mPoly;
		int					mArt;		// which piece it is (all drawn as the same slate block since D35)
	};
	struct KelpDef { float mX0, mY0, mX1, mY1; };

	struct MapDef
	{
		bool				mTrench = false;
		std::vector<Vec>	mFloor;			// left to right; y = floor surface
		std::vector<WallDef> mWalls;
		std::vector<KelpDef> mKelp;
		// Tanks: the warp hole (top middle) leads into the Trench; floor pads (corners)
		// to the same corner of the other tank.
		Vec					mPortal;
		float				mPortalR = 0;
		Vec					mPortalExit;	// where a hero arrives through the portal
		Vec					mPad[2];		// floor warp pads (left, right)
		float				mPadR = 0;
		Vec					mCore;			// middle of the core's body
		Vec					mTower[2];		// middle of each tower's body (0 left, 1 right)
		std::vector<Vec>	mLane[2];		// minion waypoints, portal to tower
		// The Trench: a gate at each end (team 0's on the left, team 1's on the right),
		// the lane between them, two camps and the pit.
		Vec					mGate[2];
		float				mGateR = 0;
		Vec					mGateExit[2];	// where a hero coming from that team's tank arrives
		std::vector<Vec>	mTrenchLane;	// left to right
		Vec					mCamp[2];		// Gus (left ledge), Balrog (right ledge)
		Vec					mPit;			// the Squid and the Boss
	};
	const MapDef& TankMap();
	const MapDef& TrenchMap();
	inline const MapDef& MapOf(int theArena) { return theArena == kTrench ? TrenchMap() : TankMap(); }
	Vec		MonsterHome(int theSlot);
}

#endif
