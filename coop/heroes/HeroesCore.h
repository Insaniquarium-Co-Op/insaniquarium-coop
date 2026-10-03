// Pet Heroes - types shared by the arenas, heroes, sides and the network: references
// to things in a tank or the Trench, hits, rewards, visual/sound events, and snapshots.
//
// Authority: each side decides its own tank's contents and its own hero; team 0's side
// (the host) also decides the Trench's. A hit is worked out by the attacker's side and
// applied by the target's owner; everything crosses the Link as the structs below.

#ifndef __HEROES_CORE_H__
#define __HEROES_CORE_H__

#include "HeroesData.h"
#include <string>

namespace Heroes
{
	enum EntKind : uint8_t { ENT_NONE, ENT_FISH, ENT_COIN, ENT_FOOD, ENT_MINION, ENT_TOWER, ENT_CORE, ENT_HERO };

	// Something in a tank or the Trench. Heroes: mArena unused, mId = player. Towers: mId 0/1. Core: 0.
	struct EntityRef
	{
		uint8_t		mArena = 0;
		uint8_t		mKind = ENT_NONE;
		uint32_t	mId = 0;
		bool Valid() const { return mKind != ENT_NONE; }
		bool operator==(const EntityRef& o) const { return mKind == o.mKind && mId == o.mId && (mKind == ENT_HERO || mArena == o.mArena); }
		bool operator!=(const EntityRef& o) const { return !(*this == o); }
		static EntityRef Hero(int thePlayer) { EntityRef r; r.mKind = ENT_HERO; r.mId = (uint32_t)thePlayer; return r; }
		static EntityRef Of(int theArena, uint8_t theKind, uint32_t theId) { EntityRef r; r.mArena = (uint8_t)theArena; r.mKind = theKind; r.mId = theId; return r; }
	};

	enum HitSource : uint8_t { SRC_ATTACK, SRC_ABILITY, SRC_TOWER, SRC_LASER, SRC_MINION, SRC_ZONE, SRC_MONSTER, SRC_BURN };
	static const uint8_t kNoAbility = 255;

	struct Hit
	{
		float		mDamage = 0;
		int8_t		mPlayer = -1;		// who dealt it (rewards); -1: a tower, laser, minion or monster
		uint8_t		mTeam = 0;			// the attacker's team (2: a monster)
		uint8_t		mSource = SRC_ATTACK;
		uint8_t		mAbility = kNoAbility;	// which slot (death recap); the hero is the attacker's
		uint8_t		mHero = 255;		// the attacker's hero (death recap)
		uint16_t	mStunMs = 0;
		uint16_t	mSleepMs = 0;		// like a stun, but damage wakes it (Meryl's Lullaby)
		uint16_t	mSlowMs = 0;
		float		mSlowPct = 0;
		Vec			mPush;				// knockback
		bool		mPull = false;		// Claw Grab, Switcheroo, Siren Song: move to mPullTo
		Vec			mPullTo;
		uint16_t	mCharmMs = 0;		// a minion fights for mTeam
		uint16_t	mBlindMs = 0;		// a tower can't shoot (Stink Cloud)
		float		mHeal = 0;			// a heal instead of damage (Heal Beam on a tower)
		float		mShield = 0;		// a shield (Halo on a tower/core)
		uint16_t	mShieldMs = 0;
		float		mMaxHpPct = 0;		// + this share of the target's max health (Kraken Tooth; worked out by the owner)
		uint16_t	mRallyMs = 0;		// a friendly minion hits harder (Meryl's Anthem)
		bool		mCleanse = false;	// ends slows and stuns (Angie's Angelic)
	};

	// Money, XP or a buff owed to a player (kills, towers, fish, coins, monsters).
	struct Reward
	{
		int8_t		mPlayer = -1;			// -1: whoever plays for mTeam (a tower or minion kill)
		int8_t		mTeam = -1;
		float		mXp = 0;
		int			mMoney = 0;
		uint8_t		mWhat = 0;			// EntKind that died (for the kill feed and stats); ENT_COIN: a lane coin
		uint8_t		mBuff = 255;		// BuffId to start (BUFF_SQUID: a Squid joins the next wave); 255 none
	};

	// Something to show or play; both sides draw the events of the tank on screen.
	enum EventType : uint8_t
	{
		EV_TEXT,			// floating number/word at mA (mValue, mParam = color)
		EV_SOUND,			// mParam = sound id (HeroesSound)
		EV_PROJECTILE,		// id mId, from mA with velocity mB for mMs; mParam = look
		EV_PROJ_END,		// projectile mId hit something or ran out
		EV_BOLT,			// a tower shot from mA toward mB taking mMs
		EV_ZONE,			// area at mA, radius mValue, for mMs; mParam = look
		EV_LIGHTNING,		// from mA to mB
		EV_SLASH,			// melee hit at mA facing mB
		EV_BURST,			// ring/explosion at mA, radius mValue; mParam = look
		EV_BEAM,			// heal beam from mA to mB for mMs
		EV_LEVEL_UP,		// player mPlayer reached level mValue
		EV_KILL,			// mPlayer killed hero mId (mValue = bounty, mParam = the source)
		EV_TOWER_DOWN,		// tower mId of arena mArena fell
		EV_WAVE,			// a wave of mValue minions arrived
		EV_FISH_DIED,		// at mA
		EV_REVIVE,			// fish came back at mA
		EV_CROSS,			// hero mPlayer crossed into arena mArena
		EV_BUMP,			// my hero ran into a wall at mA (local only, never sent; D35)
		EV_ANNOUNCE,		// a banner: mParam = AnnounceKind, mPlayer = who did it, mId = the other, mValue = a number
		EV_LOB,				// something lobbed from mA landing at mB after mMs; mParam = look
		EV_DECOY,			// a fake hero mId (HeroId) of team mPlayer at mA for mMs
		EV_TURRET,			// a turret of team mPlayer at mA for mMs (mId = its id)
		EV_TURRET_END,		// turret mId is gone
		EV_CONE,			// a cone from mA toward mB, mValue degrees to each side; mParam = look
		EV_LINE,			// a wave from mA to mB, mValue wide, over mMs; mParam = look
		EV_MONSTER,			// a monster appeared (mParam = MonsterSlot) at mA, or fell (mValue = 1)
	};

	enum AnnounceKind : uint8_t
	{
		AN_FIRST_BLOOD, AN_SPREE, AN_RAMPAGE, AN_UNSTOPPABLE, AN_GODLIKE, AN_SHUTDOWN,
		AN_TOWER, AN_CORE_OPEN, AN_SQUID, AN_BOSS, AN_EVOLVE, AN_CAMP, AN_SQUID_UP, AN_BOSS_UP,
	};

	enum SoundId : uint8_t
	{
		SND_HIT, SND_ZAP, SND_SLASH, SND_TOWER, SND_EXPLODE, SND_COIN, SND_DIAMOND, SND_BUY, SND_BUZZER,
		SND_DIE, SND_LEVEL, SND_WARP, SND_HEAL, SND_SHIELD, SND_ROAR, SND_SPLASH, SND_CHOMP, SND_LASER,
		SND_ALARM, SND_GROW, SND_STINK, SND_THUNDER,
		SND_CARD, SND_SING, SND_MISSILE, SND_CLAM_OPEN, SND_CLAM_CLOSE, SND_EVOLVE, SND_SCREAM, SND_BIG_SPLASH,
		SND_SONAR, SND_TONE, SND_BOOM,
		SND_COUNT
	};

	enum Look : uint8_t
	{
		LOOK_NONE, LOOK_ZAP, LOOK_CLYDE_ATTACK, LOOK_ANGIE_ATTACK, LOOK_STATIC, LOOK_STINK, LOOK_SLIME,
		LOOK_STORM, LOOK_SLAM, LOOK_LEAP, LOOK_HALO, LOOK_CHARM, LOOK_RESURRECT, LOOK_GOLD, LOOK_DESTRUCTOR,
		LOOK_TELEPORT, LOOK_LUNGE,
		LOOK_CARD, LOOK_PEARL, LOOK_NOTE, LOOK_SLEEP, LOOK_BOMB, LOOK_MINE, LOOK_INK, LOOK_FORTRESS,
		LOOK_SIREN, LOOK_BURN, LOOK_EVOLVE, LOOK_MISSILE, LOOK_SPARKLE, LOOK_BOSS, LOOK_ANTHEM,
	};

	enum TextColor : uint8_t { TC_DAMAGE, TC_HEAL, TC_MONEY, TC_XP, TC_INFO, TC_BAD };

	struct Event
	{
		uint8_t		mType = EV_TEXT;
		uint8_t		mArena = 0;			// which tank it happens in
		int8_t		mPlayer = -1;
		uint8_t		mParam = 0;
		uint32_t	mId = 0;
		Vec			mA, mB;
		float		mValue = 0;
		uint16_t	mMs = 0;
		std::string	mText;				// EV_TEXT only (short)
	};

	///////////////////////////////////////////////////////////////////////////
	// Snapshots: what a side shows the other of its tank (or the Trench) and its hero.
	///////////////////////////////////////////////////////////////////////////
	enum FishFlags : uint8_t { FF_RIGHT = 1, FF_HUNGRY = 2, FF_DYING = 4, FF_EATING = 8 };
	struct FishSnap { uint32_t mId; uint8_t mKind, mSize, mFlags; Vec mPos; };
	struct CoinSnap { uint32_t mId; uint8_t mKind; Vec mPos; };
	struct FoodSnap { uint32_t mId; Vec mPos; };
	enum MinionFlags : uint8_t { MF_RIGHT = 1, MF_CHARMED = 2, MF_STUNNED = 4, MF_ATTACKING = 8, MF_ASLEEP = 16, MF_RALLIED = 32, MF_ANGRY = 64, MF_HOMING = 128 };
	struct MinionSnap { uint32_t mId; uint8_t mKind, mFlags, mTeam; float mHpFrac; Vec mPos; };

	struct ArenaSnap
	{
		uint8_t		mArena = 0;				// 0/1: a team's tank, kTrench
		uint8_t		mTeam = 0;				// who runs it
		float		mTowerHp[2] = { 0, 0 };
		float		mTowerMax = kTowerHealth;
		float		mTowerShield[2] = { 0, 0 };
		uint8_t		mTowerLevel = 0;
		uint8_t		mTowerBlind = 0;		// bit per tower
		float		mCoreHp = 0;
		float		mCoreShield = 0;
		uint16_t	mFishCount = 0;
		uint8_t		mFoodQuality = 0;		// 0 pellets, 1 cans, 2 pills
		uint8_t		mCollectorLevel = 0;	// Stinky the pet (0: none)
		Vec			mCollectorPos;
		bool		mCollectorRight = true;
		uint16_t	mMonsterIn[MON_COUNT] = { 0, 0, 0, 0 };	// the Trench: seconds until it's back (0: there)
		std::vector<FishSnap>	mFish;
		std::vector<CoinSnap>	mCoins;
		std::vector<FoodSnap>	mFood;
		std::vector<MinionSnap>	mMinions;
	};

	enum HeroFlags : uint32_t
	{
		HF_ALIVE = 1, HF_HIDDEN = 2, HF_UNTARGETABLE = 4, HF_STUNNED = 8, HF_IMMUNE = 16,
		HF_RIGHT = 32, HF_ATTACKING = 64, HF_STORM = 128, HF_LEAPING = 256, HF_SLOWED = 512,
		HF_GOLDRUSH = 1024, HF_TAUNT = 2048, HF_SPEED = 4096,
		HF_ASLEEP = 1u << 13, HF_COPY = 1u << 14, HF_FORTRESS = 1u << 15, HF_ANTHEM = 1u << 16,
		HF_INK = 1u << 17, HF_CLAM = 1u << 18, HF_BURNING = 1u << 19, HF_CASTING = 1u << 20,
	};
	struct HeroSnap
	{
		uint8_t		mPlayer = 0, mTeam = 0, mHero = 0, mArena = 0;
		uint8_t		mBaseHero = 0;			// != mHero while Presto copies someone
		uint32_t	mFlags = 0;
		Vec			mPos;
		float		mHp = 0, mMaxHp = 1, mShield = 0;
		uint8_t		mLevel = 1;
		uint16_t	mRespawnMs = 0;			// until it's back
		uint16_t	mKills = 0, mDeaths = 0;
		uint8_t		mStreak = 0;			// kills since it last died
		uint8_t		mTalents = 0;			// 2 bits a tier: 0 none, 1 left, 2 right
		uint8_t		mBuffS[BUFF_COUNT] = { 0, 0, 0 };	// seconds left
		uint8_t		mItems[kItemSlots] = { 255, 255, 255, 255, 255, 255 };
		uint16_t	mFishLost = 0, mTowers = 0;		// the result screen
		uint32_t	mEarned = 0;
		uint32_t	mStructDamage = 0;		// awards
		uint16_t	mLaneCoins = 0, mMinionKills = 0;
		uint8_t		mObjectives = 0;
	};

	// A minion entering a tank or the Trench (a new wave, or a minion leaving the Trench).
	struct Arrival
	{
		uint8_t		mKind = MIN_MINI;
		float		mMult = 1;				// health and damage multiplier (Tougher Minions, sudden death)
		float		mHpFrac = 1;			// health left
	};

	// A hero inside a tank or the Trench, as its towers, minions and monsters see it.
	struct HeroPresence
	{
		int			mPlayer = -1;
		int			mTeam = 0;
		Vec			mPos;
		float		mRadius = 32;
		bool		mTargetable = false;	// alive, not hidden, not untargetable
		bool		mTaunting = false;		// Shell Up: pull aggro
		bool		mAlive = true;			// coins and shared XP (hidden heroes still collect)
		bool		mMagnet = false;		// Speedy's Treasure Trail: Trench coins fly to him
		uint32_t	mHurtMyHeroAt = 0;		// last time it damaged this tank's owner's hero
		uint32_t	mHurtHeroAt = 0;		// the Trench: last time it damaged the other team's hero
	};
}

#endif
