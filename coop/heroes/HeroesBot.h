// Pet Heroes - the bot: an average player for practice matches and for bot-vs-bot
// balance tournaments. It only uses the orders a person has (Side's move, attack,
// cast, talents, buy and home clicks), at a human-ish click rate. 3.0 (D36): it lanes
// in the Trench with its wave, grabs coins, takes camps and the big monsters when it's
// safe, follows its wave into the rival's tank, and farms home through the home window.

#ifndef __HEROES_BOT_H__
#define __HEROES_BOT_H__

#include "HeroesSide.h"

namespace Heroes
{
	class Bot
	{
	public:
		int			mSkill = 1;				// 0 easy, 1 normal, 2 hard
		bool		mPassive = false;		// the tutorial: farms, never leaves home, never fights
		void		Think(Side& s);			// call every tick

		// What it's doing (for tests and the practice HUD).
		enum Plan : uint8_t { PLAN_FARM, PLAN_DEFEND, PLAN_PUSH, PLAN_RETREAT, PLAN_FIGHT, PLAN_LANE, PLAN_MONSTER };
		uint8_t		mPlan = PLAN_FARM;

	private:
		void		Farm(Side& s);
		void		Shop(Side& s);
		void		Fight(Side& s);
		bool		UseAbilities(Side& s, const Target* theEnemyHero, const std::vector<Target>& theNear);
		void		GoTo(Side& s, int theArena, Vec thePos, bool theViaPads = false);
		bool		HomeInDanger(const Side& s, int& theWhy) const;
		bool		Lane(Side& s, const std::vector<Target>& theAll, const Target* theEnemyHero, float theHpFrac);
		bool		Monsters(Side& s, const Target* theEnemyHero, float theHpFrac);
		bool		Raid(Side& s, const std::vector<Target>& theAll, const Target* theEnemyHero, float theHpFrac);
		void		Attack(Side& s, const EntityRef& theRef);
		uint32_t	mNextClick = 0, mNextShop = 0, mNextThink = 0, mNextCast = 0;
		int			mBuildStep = 0;
		Vec			mLastMove;
		int			mLastMoveArena = -1;
		uint32_t	mRetreatUntil = 0;
		uint32_t	mDefendSince = 0, mCounterUntil = 0;	// push instead of defending forever
		int			mKillsAtDefend = 0;
	};
}

#endif
