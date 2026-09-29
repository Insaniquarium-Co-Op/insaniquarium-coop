// Pet Heroes - a whole match on one computer: both sides, joined by in-memory links.
// Used by practice mode (you against the bot) and the headless tests.

#ifndef __HEROES_MATCH_H__
#define __HEROES_MATCH_H__

#include "HeroesSide.h"

namespace Heroes
{
	class Match
	{
	public:
		Side		mSide[kTeams];
		std::unique_ptr<MemoryLink> mLink[kTeams];
		uint32_t	mNow = 0, mStart = 0;

		void		Start(int theHero0, int theHero1, uint64_t theSeed, uint32_t theNow, uint32_t theDelayMs = 0);
		void		Step(uint32_t theNow);		// both sides, one tick
		void		Tick() { Step(mNow + kTickMs); }
		int			Winner() const;				// -1 while it's running
		uint32_t	Ms() const { return mNow - mStart; }
	};
}

#endif
