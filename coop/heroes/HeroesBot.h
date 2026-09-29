// Pet Heroes - the bot: an average player for practice matches and for bot-vs-bot
// balance tournaments. It only uses the orders a person has (Side's move, attack,
// cast, buy and home clicks), at a human-ish click rate.

#ifndef __HEROES_BOT_H__
#define __HEROES_BOT_H__

#include "HeroesSide.h"

namespace Heroes
{
	class Bot
	{
	public:
		int			mSkill = 1;				// 0 easy, 1 normal, 2 hard
		void		Think(Side& s);			// call every tick

		// What it's doing (for tests and the practice HUD).
		enum Plan : uint8_t { PLAN_FARM, PLAN_DEFEND, PLAN_PUSH, PLAN_RETREAT, PLAN_FIGHT };
		uint8_t		mPlan = PLAN_FARM;

	private:
		void		Farm(Side& s);
		void		Shop(Side& s);
		void		Fight(Side& s);
		bool		UseAbilities(Side& s, const Target* theEnemyHero, const std::vector<Target>& theNear);
		void		GoTo(Side& s, int theArena, Vec thePos);
		bool		HomeInDanger(const Side& s, int& theWhy) const;
		uint32_t	mNextClick = 0, mNextShop = 0, mNextThink = 0, mNextCast = 0, mNextOrder = 0;
		int			mBuildStep = 0;
		EntityRef	mLastTarget;
		Vec			mLastMove;
		uint32_t	mRetreatUntil = 0;
		uint32_t	mDefendSince = 0, mCounterUntil = 0;	// split-push instead of defending forever
		int			mKillsAtDefend = 0;
	};
}

#endif
