#include "HeroesMatch.h"

namespace Heroes
{
	void Match::Start(int theHero0, int theHero1, uint64_t theSeed, uint32_t theNow, uint32_t theDelayMs)
	{
		MemoryLink::MakePair(mLink[0], mLink[1], theDelayMs);
		mNow = mStart = theNow;
		for (int t = 0; t < kTeams; t++)
		{
			mLink[t]->SetNow(theNow);
			mSide[t].mLink = mLink[t].get();
			mSide[t].Init(t, t, t == 0 ? theHero0 : theHero1, 1 - t, theSeed * 2654435761u + (uint64_t)t * 97 + 1, theNow);
		}
	}

	void Match::Step(uint32_t theNow)
	{
		mNow = theNow;
		for (int t = 0; t < kTeams; t++)
		{
			mLink[t]->SetNow(theNow);
			mSide[t].Step(theNow);
		}
	}

	int Match::Winner() const
	{
		for (int t = 0; t < kTeams; t++)
			if (mSide[t].mWon || mSide[1 - t].mLost)
				return t;
		return -1;
	}
}
