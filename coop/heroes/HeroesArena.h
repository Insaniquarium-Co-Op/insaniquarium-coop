// Pet Heroes - one tank, simulated by its owner's side: the farm (fish, coins, food),
// the invading minions, the towers, the core, the owner's wallet and shop levels.

#ifndef __HEROES_ARENA_H__
#define __HEROES_ARENA_H__

#include "HeroesCore.h"

namespace Heroes
{
	struct Fish
	{
		uint32_t	mId = 0;
		uint8_t		mKind = FISH_GUPPY;
		uint8_t		mSize = SIZE_SMALL;
		Vec			mPos, mTarget;
		float		mMeals = 0;
		float		mHp = 40;
		uint32_t	mHungryAt = 0, mStarveAt = 0;
		uint32_t	mNextCoin = 0, mNextBreed = 0, mNextWander = 0, mEatingUntil = 0;
		uint32_t	mDyingAt = 0;			// != 0: dying (removed after a moment)
		bool		mRight = true;
		bool		Hungry(uint32_t theNow) const { return theNow >= mHungryAt; }
		float		Radius() const { return FishDefOf(mKind).mRadius[mSize]; }
	};

	struct Coin
	{
		uint32_t	mId = 0;
		uint8_t		mKind = COIN_SILVER;
		Vec			mPos;
		uint32_t	mLandedAt = 0;
	};

	struct Food
	{
		uint32_t	mId = 0;
		Vec			mPos;
		uint32_t	mLandedAt = 0;
	};

	struct Minion
	{
		uint32_t	mId = 0;
		uint8_t		mKind = MIN_MINI;
		uint8_t		mTeam = 0;				// who sent it (it attacks the other team)
		uint8_t		mLane = 0;
		int			mWaypoint = 0;
		Vec			mPos;
		float		mHp = 1, mMaxHp = 1, mMult = 1;
		uint32_t	mNextHit = 0, mNextBite = 0, mNextThink = 0, mAttackingUntil = 0;
		uint32_t	mCharmUntil = 0;
		uint8_t		mCharmTeam = 0;
		int8_t		mCharmPlayer = -1;
		uint32_t	mStunUntil = 0, mSlowUntil = 0;
		float		mSlowPct = 0;
		EntityRef	mTarget;
		bool		mRight = false;
		int8_t		mLastHitBy = -1;		// player who last hurt it (rewards)
		int			Team(uint32_t theNow) const { return theNow < mCharmUntil ? mCharmTeam : mTeam; }
	};

	struct Tower
	{
		float		mHp = kTowerHealth;
		bool		mAlive = true;
		uint32_t	mNextShot = 0, mLastHurtAt = 0, mBlindUntil = 0, mShieldUntil = 0;
		float		mShield = 0;
		EntityRef	mLastTarget;
		int			mRamp = 0;
	};

	struct Bolt								// a tower or Destructor shot in flight
	{
		Vec			mPos;
		EntityRef	mTarget;
		Vec			mTargetPos;
		float		mDamage = 0;
		uint8_t		mTeam = 0;				// the shooter's team
		uint8_t		mSource = SRC_TOWER;
	};

	struct DeadFish { uint8_t mKind, mSize; Vec mPos; };

	// What the owner's side tells the arena each step.
	struct ArenaContext
	{
		std::vector<HeroPresence> mHeroes;	// heroes inside this tank
		int			mOwnerPlayer = 0;		// the owner's hero's player (rewards for defense)
		bool		mOwnerHeroHere = false;	// the owner's hero is in this tank
		Vec			mOwnerHeroPos;
		bool		mScavenger = false;		// Stinky: collect coins he touches
		bool		mGoldRush = false;		// Stinky's R: coins fly to him, fish drop double
		bool		mGrace = false;			// Angie: dead fish may come back
		bool		mSuddenDeath = false;	// the core loses its armor
	};

	class Arena
	{
	public:
		int			mTeam = 0;
		Rng			mRng;
		uint32_t	mNextId = 1;

		// The owner's economy and upgrades.
		int			mMoney = kStartMoney;
		int			mFoodQuality = 0;		// 0..2
		int			mPellets = kStartPellets;
		int			mLaserLevel = 0;
		uint32_t	mNextLaser = 0;
		int			mTowerLevel = 0;
		int			mMoneyEarned = 0;

		std::vector<Fish>		mFish;
		std::vector<Coin>		mCoins;
		std::vector<Food>		mFood;
		std::vector<Minion>		mMinions;
		std::vector<Bolt>		mBolts;
		std::vector<DeadFish>	mDead;		// recent deaths (Resurrection)
		Tower		mTower[2];
		float		mCoreHp = kCoreHealth;
		float		mCoreShield = 0;
		uint32_t	mCoreShieldUntil = 0;
		bool		mCoreDead = false;
		int			mFishLost = 0, mMinionsKilled = 0;
		int			mStarved = 0, mFishKilled = 0, mFishBitten = 0;	// how fish died (stats)

		// Outputs, collected by the side after every step or action.
		std::vector<std::pair<EntityRef, Hit>>	mOutHits;		// hits on heroes
		std::vector<Reward>						mOutRewards;
		std::vector<Event>						mOutEvents;

		void		Init(int theTeam, uint64_t theSeed, uint32_t theNow);
		void		Step(uint32_t theNow, const ArenaContext& theCtx);

		// The owner's left-click in this tank.
		enum ClickKind { CLICK_NONE, CLICK_COIN, CLICK_LASER, CLICK_FOOD, CLICK_REFUSED };
		ClickKind	Click(Vec p, uint32_t theNow, const ArenaContext& theCtx);

		bool		CanAfford(int thePrice) const { return mMoney >= thePrice; }
		bool		Spend(int thePrice);
		void		Earn(int theMoney, Vec theWhere, bool theShow);
		bool		AddFish(int theKind, uint32_t theNow, Vec thePos, int theSize = SIZE_SMALL);
		int			FishCount() const;

		void		ApplyHit(const EntityRef& theTarget, const Hit& theHit, uint32_t theNow);
		void		SpawnWave(const std::vector<uint8_t>& theKinds, float theMult, int theFromTeam, uint32_t theNow);
		void		Revive(int theCount, uint32_t theNow);
		bool		CoreOpen() const { return !mTower[0].mAlive && !mTower[1].mAlive; }
		float		TowerRange() const { return kTowerRange + 40.0f * mTowerLevel; }
		float		TowerDamage() const { return kTowerHit * (1.0f + 0.25f * mTowerLevel); }
		int			AliveTowers() const { return (mTower[0].mAlive ? 1 : 0) + (mTower[1].mAlive ? 1 : 0); }
		Vec			TowerGun(int i) const { return TheMap().mTower[i] - Vec(0, 40); }

		ArenaSnap	Snapshot() const;

		Minion*		FindMinion(uint32_t theId);
		Fish*		FindFish(uint32_t theId);

	private:
		void		StepFish(uint32_t theNow, float theDt, const ArenaContext& theCtx);
		void		StepCoins(uint32_t theNow, float theDt, const ArenaContext& theCtx);
		void		StepMinions(uint32_t theNow, float theDt, const ArenaContext& theCtx);
		void		StepTowers(uint32_t theNow, float theDt, const ArenaContext& theCtx);
		void		KillFish(Fish& f, uint32_t theNow, bool theCanRevive, bool theCountLoss);
		void		HurtMinion(Minion& m, const Hit& theHit, uint32_t theNow);
		void		HurtStructure(int theWhich, const Hit& theHit, uint32_t theNow);	// 0/1 towers, 2 core
		float		Backdoor(int theWhich, const Hit& theHit, uint32_t theNow) const;
		void		DropCoin(uint8_t theKind, Vec thePos);
		void		Text(Vec theAt, const std::string& theText, uint8_t theColor);
		void		Sound(uint8_t theSound, Vec theAt);
		Vec			MinionGoal(Minion& m, uint32_t theNow, const ArenaContext& theCtx, float& theReach);
		uint32_t	mLastStep = 0;
		uint32_t	mLastDamageSound = 0;
		bool		mGrace = false, mSuddenDeath = false;	// from the last step's context
	};
}

#endif
