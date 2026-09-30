// Pet Heroes - types shared by the arena, heroes, sides and the network: references
// to things in a tank, hits, rewards, visual/sound events, and snapshots.
//
// Authority: each side decides its own tank's contents and its own hero. A hit is
// worked out by the attacker's side and applied by the target's owner; everything
// crosses the Link as the structs below.

#ifndef __HEROES_CORE_H__
#define __HEROES_CORE_H__

#include "HeroesData.h"
#include <string>

namespace Heroes
{
	enum EntKind : uint8_t { ENT_NONE, ENT_FISH, ENT_COIN, ENT_FOOD, ENT_MINION, ENT_TOWER, ENT_CORE, ENT_HERO };

	// Something in a tank. Heroes: mArena unused, mId = player. Towers: mId 0/1. Core: 0.
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

	enum HitSource : uint8_t { SRC_ATTACK, SRC_ABILITY, SRC_TOWER, SRC_LASER, SRC_MINION, SRC_ZONE };

	struct Hit
	{
		float		mDamage = 0;
		int8_t		mPlayer = -1;		// who dealt it (rewards); -1: a tower, laser or minion
		uint8_t		mTeam = 0;			// the attacker's team
		uint8_t		mSource = SRC_ATTACK;
		uint16_t	mStunMs = 0;
		uint16_t	mSlowMs = 0;
		float		mSlowPct = 0;
		Vec			mPush;				// knockback
		bool		mPull = false;		// Claw Grab: move to mPullTo
		Vec			mPullTo;
		uint16_t	mCharmMs = 0;		// a minion fights for mTeam
		uint16_t	mBlindMs = 0;		// a tower can't shoot (Stink Cloud)
		float		mHeal = 0;			// a heal instead of damage (Heal Beam on a tower)
		float		mShield = 0;		// a shield (Halo on a tower/core)
		uint16_t	mShieldMs = 0;
	};

	// Money and XP owed to a player (kills, towers, fish).
	struct Reward
	{
		int8_t		mPlayer = -1;			// -1: whoever plays for mTeam (a tower or minion kill)
		int8_t		mTeam = -1;
		float		mXp = 0;
		int			mMoney = 0;
		uint8_t		mWhat = 0;			// EntKind that died (for the kill feed)
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
		EV_KILL,			// mPlayer killed hero mId (mValue = bounty)
		EV_TOWER_DOWN,		// tower mId of arena mArena fell
		EV_WAVE,			// a wave of mValue minions arrived
		EV_FISH_DIED,		// at mA
		EV_REVIVE,			// fish came back at mA
		EV_CROSS,			// hero mPlayer crossed into arena mArena
	};

	enum SoundId : uint8_t
	{
		SND_HIT, SND_ZAP, SND_SLASH, SND_TOWER, SND_EXPLODE, SND_COIN, SND_DIAMOND, SND_BUY, SND_BUZZER,
		SND_DIE, SND_LEVEL, SND_WARP, SND_HEAL, SND_SHIELD, SND_ROAR, SND_SPLASH, SND_CHOMP, SND_LASER,
		SND_ALARM, SND_GROW, SND_STINK, SND_THUNDER, SND_COUNT
	};

	enum Look : uint8_t
	{
		LOOK_NONE, LOOK_ZAP, LOOK_CLYDE_ATTACK, LOOK_ANGIE_ATTACK, LOOK_STATIC, LOOK_STINK, LOOK_SLIME,
		LOOK_STORM, LOOK_SLAM, LOOK_LEAP, LOOK_HALO, LOOK_CHARM, LOOK_RESURRECT, LOOK_GOLD, LOOK_DESTRUCTOR,
		LOOK_TELEPORT, LOOK_LUNGE,
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
	// Snapshots: what a side shows the other of its tank and its hero.
	///////////////////////////////////////////////////////////////////////////
	enum FishFlags : uint8_t { FF_RIGHT = 1, FF_HUNGRY = 2, FF_DYING = 4, FF_EATING = 8 };
	struct FishSnap { uint32_t mId; uint8_t mKind, mSize, mFlags; Vec mPos; };
	struct CoinSnap { uint32_t mId; uint8_t mKind; Vec mPos; };
	struct FoodSnap { uint32_t mId; Vec mPos; };
	enum MinionFlags : uint8_t { MF_RIGHT = 1, MF_CHARMED = 2, MF_STUNNED = 4, MF_ATTACKING = 8 };
	struct MinionSnap { uint32_t mId; uint8_t mKind, mFlags, mTeam; float mHpFrac; Vec mPos; };

	struct ArenaSnap
	{
		uint8_t		mTeam = 0;
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
		std::vector<FishSnap>	mFish;
		std::vector<CoinSnap>	mCoins;
		std::vector<FoodSnap>	mFood;
		std::vector<MinionSnap>	mMinions;
	};

	enum HeroFlags : uint16_t
	{
		HF_ALIVE = 1, HF_HIDDEN = 2, HF_UNTARGETABLE = 4, HF_STUNNED = 8, HF_IMMUNE = 16,
		HF_RIGHT = 32, HF_ATTACKING = 64, HF_STORM = 128, HF_LEAPING = 256, HF_SLOWED = 512,
		HF_GOLDRUSH = 1024, HF_TAUNT = 2048, HF_SPEED = 4096,
	};
	struct HeroSnap
	{
		uint8_t		mPlayer = 0, mTeam = 0, mHero = 0, mArena = 0;
		uint16_t	mFlags = 0;
		Vec			mPos;
		float		mHp = 0, mMaxHp = 1, mShield = 0;
		uint8_t		mLevel = 1;
		uint16_t	mRespawnMs = 0;			// until it's back
		uint16_t	mKills = 0, mDeaths = 0;
		uint8_t		mItems[kItemSlots] = { 255, 255, 255, 255, 255, 255 };
		uint16_t	mFishLost = 0, mTowers = 0;		// the result screen
		uint32_t	mEarned = 0;
	};

	// A hero inside a tank, as that tank's towers and minions see it.
	struct HeroPresence
	{
		int			mPlayer = -1;
		int			mTeam = 0;
		Vec			mPos;
		float		mRadius = 32;
		bool		mTargetable = false;	// alive, not hidden, not untargetable
		bool		mTaunting = false;		// Shell Up: pull aggro
		uint32_t	mHurtMyHeroAt = 0;		// last time it damaged this tank's owner's hero
	};
}

#endif
