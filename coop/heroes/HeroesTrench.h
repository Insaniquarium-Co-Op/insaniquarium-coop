// Pet Heroes - the Trench (D36): the shared tank between the two homes. Both teams'
// waves come out of their own gate, meet in the lane and fight; survivors leave through
// the far gate into the rival's tank. Minions that die here drop coins any hero can grab
// by touching them. Monsters (two buff camps, the Psychosquid and the Boss) wait to be
// fought. Simulated by team 0's side (the host), like a tank by its owner.

#ifndef __HEROES_TRENCH_H__
#define __HEROES_TRENCH_H__

#include "HeroesArena.h"

namespace Heroes
{
	struct LaneCoin
	{
		uint32_t	mId = 0;
		int			mValue = 0;
		Vec			mPos;
		float		mDrift = 0;				// sideways as it sinks
		uint32_t	mLandedAt = 0;
	};

	struct TrenchContext
	{
		std::vector<HeroPresence> mHeroes;	// heroes in the Trench
		bool		mSuddenDeath = false;
	};

	class Trench
	{
	public:
		Rng			mRng;
		uint32_t	mNextId = 1;
		uint32_t	mLastStep = 0;
		std::vector<Minion>		mMinions;	// both teams' lane minions and the monsters (team 2)
		std::vector<Bolt>		mBolts;		// Destructor shells
		std::vector<LaneCoin>	mCoins;
		uint32_t	mMonsterAt[MON_COUNT] = { 0, 0, 0, 0 };	// when it (re)appears; 0: it's here
		uint32_t	mMonsterId[MON_COUNT] = { 0, 0, 0, 0 };
		int			mLaneCoinsDropped = 0;

		// Outputs, collected by the owner's side after every step or hit.
		std::vector<std::pair<EntityRef, Hit>>	mOutHits;		// hits on heroes
		std::vector<Reward>						mOutRewards;
		std::vector<Event>						mOutEvents;
		std::vector<Arrival>					mOutArrivals[kTeams];	// leaving for that team's tank

		void		Init(uint64_t theSeed, uint32_t theNow);
		void		Step(uint32_t theNow, const TrenchContext& theCtx);
		void		SpawnWave(const std::vector<Arrival>& theMinions, int theFromTeam, uint32_t theNow);	// at that team's gate
		void		ApplyHit(const EntityRef& theTarget, const Hit& theHit, uint32_t theNow);
		ArenaSnap	Snapshot() const;
		Minion*		FindMinion(uint32_t theId);
		const Minion* Monster(int theSlot) const;
		uint32_t	MonsterIn(int theSlot, uint32_t theNow) const;	// ms until it's back (0: it's here)

	private:
		void		StepMonsters(uint32_t theNow, float theDt, const TrenchContext& theCtx);
		void		StepLane(uint32_t theNow, float theDt, const TrenchContext& theCtx);
		void		StepBolts(uint32_t theNow, float theDt, const TrenchContext& theCtx);
		void		StepCoins(uint32_t theNow, float theDt, const TrenchContext& theCtx);
		void		HurtMinion(Minion& m, const Hit& theHit, uint32_t theNow);
		void		MinionDied(Minion& m, const Hit& theHit, uint32_t theNow);
		void		DropCoins(int theValue, Vec thePos, int theCount);
		void		HitHero(int thePlayer, const Hit& theHit) { mOutHits.push_back({ EntityRef::Hero(thePlayer), theHit }); }
		void		Sound(uint8_t theSound, Vec theAt);
		void		Burst(Vec theAt, float theRadius, uint8_t theLook);
		Vec			LaneTarget(const Minion& m, int theTeam) const;
		std::vector<HeroPresence> mHeroes;	// from the last step (shared XP on deaths between steps)
	};
}

#endif
