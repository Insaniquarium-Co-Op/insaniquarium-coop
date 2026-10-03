#include "HeroesBot.h"
#include "HeroesMap.h"

namespace Heroes
{
	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }

	// Which talent each hero's bot takes at levels 3, 6 and 9 (0 left, 1 right).
	static const uint8_t kTalentPick[HERO_COUNT][kTalentTiers] =
	{
		{ 1, 0, 0 },	// Itchy: Long Lunge, Whirlpool, Frenzy
		{ 0, 0, 1 },	// Clyde: Overcharge, Supercell, Capacitor
		{ 0, 1, 1 },	// Rhubarb: Heavy Landing, Riptide, Iron Claw
		{ 0, 0, 0 },	// Angie: Radiance, Miracle, Pied Piper
		{ 0, 1, 0 },	// Speedy: Sticky Slime, Treasure Trail, Hoarder
		{ 0, 1, 1 },	// Presto: Full Deck, Quick Change, Bait and Switch
		{ 0, 0, 0 },	// Niko: Pearl Battery, Citadel, Cluster Pearl
		{ 0, 1, 0 },	// Meryl: Deep Sleep, Showstopper, Power Ballad
		{ 0, 1, 0 },	// Shrapnel: Big Bang, Bunker Buster, Minefield
	};

	void Bot::Think(Side& s)
	{
		if (s.Over())
			return;
		int aTier = s.PendingTalent();
		if (aTier >= 0)
			s.PickTalent(kTalentPick[s.mHero.mHero][aTier]);
		Farm(s);
		if (Elapsed(s.mNow, mNextShop))
		{
			mNextShop = s.mNow + 900;
			Shop(s);
		}
		if (Elapsed(s.mNow, mNextThink))
		{
			mNextThink = s.mNow + (mSkill == 0 ? 600 : 250);	// Easy reacts to fights later
			Fight(s);
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// The farm: coins, food, the laser
	///////////////////////////////////////////////////////////////////////////
	void Bot::Farm(Side& s)
	{
		if (!Elapsed(s.mNow, mNextClick))
			return;
		// It farms what a person could see: its tank when it's home, and the home window
		// (D36) while its hero is away, at a slower pace (attention is split). Easy hardly
		// looks at the home window.
		int aSkill = std::clamp(mSkill, 0, 2);
		bool aAway = s.mHero.mAlive && s.mHero.mArena != s.mTeam;
		static const uint32_t kPeriod[3] = { 800, 450, 320 };
		static const uint32_t kAwayPeriod[3] = { 2600, 1000, 650 };
		uint32_t aPeriod = aAway ? kAwayPeriod[aSkill] : kPeriod[aSkill];
		if (s.mHero.mOrder == HeroState::ORD_ATTACK || mPlan == PLAN_FIGHT)
			aPeriod = aPeriod * 2;
		mNextClick = s.mNow + aPeriod;
		Arena& a = s.mArena;
		const MapDef& m = TankMap();

		// Invaders hitting a structure: laser them.
		const Minion* aThreat = nullptr;
		float aBest = 1e9f;
		for (const Minion& mi : a.mMinions)
		{
			if (mi.mHp <= 0 || mi.Team(s.mNow) == s.mTeam)
				continue;
			float d = std::min(Dist(mi.mPos, m.mTower[0]), Dist(mi.mPos, m.mTower[1]));
			d = std::min(d, Dist(mi.mPos, m.mCore));
			if (d < 260 && d < aBest)
			{
				aBest = d;
				aThreat = &mi;
			}
		}
		float aReach = aAway ? 70.0f : kCoinClickR;	// the home window's clicks reach farther
		if (aThreat != nullptr && (s.mNow / 1000) % 3 != 0)		// most of the time
		{
			s.HomeClick(aThreat->mPos, aReach);
			return;
		}
		// A raiding hero in my tank: laser it every other click.
		HeroSnap o;
		if (s.OtherHeroIn(s.mTeam, &o) && (o.mFlags & HF_ALIVE) && !(o.mFlags & (HF_HIDDEN | HF_UNTARGETABLE)) && ((s.mNow / 500) & 1))
		{
			s.HomeClick(o.mPos, aReach);
			return;
		}
		// Coins: the ones about to vanish first, then the most valuable.
		const Coin* aCoin = nullptr;
		float aScore = -1e9f;
		for (const Coin& c : a.mCoins)
		{
			float sc = (float)CoinValue(c.mKind);
			if (c.mLandedAt != 0)
				sc += 200 + (s.mNow - c.mLandedAt) * 0.05f;
			if (sc > aScore)
			{
				aScore = sc;
				aCoin = &c;
			}
		}
		// Hungry fish.
		const Fish* aHungry = nullptr;
		uint32_t aStarving = 0;
		int aHungryCount = 0;
		for (const Fish& f : a.mFish)
			if (f.mDyingAt == 0 && f.mKind != FISH_CARNIVORE && f.Hungry(s.mNow))
			{
				aHungryCount++;
				uint32_t aFor = s.mNow - f.mHungryAt;
				if (aHungry == nullptr || aFor > aStarving)
				{
					aHungry = &f;
					aStarving = aFor;
				}
			}
		bool aCanFeed = aHungry != nullptr && (int)a.mFood.size() < a.mPellets && a.mMoney >= 5;
		bool aFeedFirst = aCanFeed && (aStarving > 6000 || aCoin == nullptr || aHungryCount >= 3);
		if (aFeedFirst)
		{
			s.HomeClick(ClampToWater(s.mTeam, aHungry->mPos - Vec(0, 36), 10));
			return;
		}
		if (aCoin != nullptr)
		{
			s.HomeClick(aCoin->mPos, aReach);
			return;
		}
		if (aCanFeed)
			s.HomeClick(ClampToWater(s.mTeam, aHungry->mPos - Vec(0, 36), 10));
	}

	///////////////////////////////////////////////////////////////////////////
	// The shop
	///////////////////////////////////////////////////////////////////////////
	void Bot::Shop(Side& s)
	{
		const Arena& a = s.mArena;
		int aFish = a.FishCount(), aBreeders = 0, aCarnivores = 0;
		for (const Fish& f : a.mFish)
			if (f.mDyingAt == 0)
			{
				aBreeders += f.mKind == FISH_BREEDER;
				aCarnivores += f.mKind == FISH_CARNIVORE;
			}
		int aItems = 0;
		for (uint8_t i : s.mHero.mItems)
			aItems += i != ITEM_NONE;
		float aMin = s.MatchMs() / 60000.0f;
		int aNext = s.SuggestedItem();
		int aItem = aNext == ITEM_NONE ? -1 : SHOP_ITEM_FIRST + aNext;
		auto Try = [&](int theShop) {
			if (s.CanBuy(theShop) && a.mMoney - s.Price(theShop) >= 40)
			{
				s.Buy(theShop);
				return true;
			}
			return false;
		};
		auto Wants = [&](int theShop) { return theShop >= 0 && (s.CanBuy(theShop) || a.mMoney < s.Price(theShop)); };

		// Towers first when they're hurt.
		for (int i = 0; i < 2; i++)
			if (a.mTower[i].mAlive && a.mTower[i].mHp < kTowerHealth * 0.55f && Try(i == 0 ? SHOP_REPAIR_LEFT : SHOP_REPAIR_RIGHT))
				return;

		struct Step { bool mWant; int mShop; };
		const Step kPlan[] = {
			{ aFish < 3, SHOP_GUPPY },
			{ aFish < 4 && aMin < 2, SHOP_GUPPY },
			{ aItems < 1 && aMin > 1.5f, aItem },
			{ aFish >= 4 && a.mFoodQuality == 0, SHOP_FOOD_QUALITY },
			{ aFish >= 4 && a.mCollectorLevel == 0, SHOP_COLLECTOR },
			{ aFish < 6, SHOP_GUPPY },
			{ aItems < 2, aItem },
			{ aFish >= 6 && aBreeders == 0, SHOP_BREEDER },
			{ aFish >= 6 && a.mPellets < 3, SHOP_FOOD_COUNT },
			{ aItems < 3, aItem },
			{ aFish < 9, SHOP_GUPPY },
			{ aFish >= 8 && aCarnivores == 0, SHOP_CARNIVORE },
			{ a.mCollectorLevel < 2, SHOP_COLLECTOR },
			{ aMin > 4 && s.mWaveSizeRank < 1, SHOP_WAVE_SIZE },
			{ aItems < 4, aItem },
			{ a.mFoodQuality < 2, SHOP_FOOD_QUALITY },
			{ aMin > 5 && s.mWaveToughRank < 1, SHOP_WAVE_TOUGH },
			{ aItems < 5, aItem },
			{ aFish < 12, SHOP_GUPPY },
			{ aItems < 6, aItem },
			{ aMin > 7 && s.mWaveSizeRank < 2, SHOP_WAVE_SIZE },
			{ aMin > 7 && s.mWaveToughRank < 2, SHOP_WAVE_TOUGH },
			{ a.mLaserLevel < 1, SHOP_LASER },
			{ a.mTowerLevel < 1, SHOP_TOWER_UPGRADE },
			{ aMin > 6, SHOP_SEND_SYLV + (mBuildStep++ % 4) },
		};
		for (const Step& st : kPlan)
		{
			if (!st.mWant)
				continue;
			if (!Wants(st.mShop))
				continue;						// maxed or impossible: skip it
			if (mSkill == 0 && st.mShop >= SHOP_ITEM_FIRST && st.mShop <= SHOP_ITEM_LAST && aMin < 3 + 1.5f * aItems)
				continue;						// Easy buys hero items later
			Try(st.mShop);
			return;								// saving up for this one
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// The hero
	///////////////////////////////////////////////////////////////////////////
	// Go somewhere in any of the three tanks: through the portal into the Trench, through
	// a Trench gate into a tank, or (theViaPads) straight across by a floor pad.
	void Bot::GoTo(Side& s, int theArena, Vec thePos, bool theViaPads)
	{
		const HeroState& h = s.mHero;
		Vec aGoal = thePos;
		if (h.mArena != theArena)
		{
			const MapDef& m = MapOf(h.mArena);
			if (IsTank(h.mArena))
			{
				if (IsTank(theArena) && theViaPads)
					aGoal = Dist(h.mPos, m.mPad[0]) < Dist(h.mPos, m.mPad[1]) ? m.mPad[0] : m.mPad[1];
				else
					aGoal = m.mPortal;
			}
			else
				aGoal = m.mGate[IsTank(theArena) ? theArena : 0];
		}
		if (h.mOrder == HeroState::ORD_MOVE && mLastMoveArena == h.mArena && Dist(aGoal, mLastMove) < 30)
			return;
		if (h.mOrder == HeroState::ORD_IDLE && Dist(aGoal, h.mPos) < 20)
			return;
		mLastMove = aGoal;
		mLastMoveArena = h.mArena;
		s.OrderMove(aGoal);
	}

	void Bot::Attack(Side& s, const EntityRef& theRef)
	{
		if (s.mHero.mOrder != HeroState::ORD_ATTACK || s.mHero.mTarget != theRef)
			s.OrderAttack(theRef);
	}

	bool Bot::HomeInDanger(const Side& s, int& theWhy) const
	{
		theWhy = 0;
		HeroSnap o;
		if (s.OtherHeroIn(s.mTeam, &o) && (o.mFlags & HF_ALIVE) && !(o.mFlags & HF_HIDDEN))
		{
			theWhy = 1;
			return true;
		}
		const MapDef& m = TankMap();
		int aNear = 0;
		for (const Minion& mi : s.mArena.mMinions)
		{
			if (mi.mHp <= 0 || mi.Team(s.mNow) == s.mTeam)
				continue;
			for (int i = 0; i < 2; i++)
				if (s.mArena.mTower[i].mAlive && Dist(mi.mPos, m.mTower[i]) < 300)
					aNear += mi.mKind == MIN_MINI ? 1 : 3;
			if (s.mArena.CoreOpen() && Dist(mi.mPos, m.mCore) < 300)
				aNear += 3;
		}
		if (aNear >= 4)
		{
			theWhy = 2;
			return true;
		}
		return false;
	}

	void Bot::Fight(Side& s)
	{
		HeroState& h = s.mHero;
		if (!h.mAlive)
		{
			mPlan = PLAN_RETREAT;
			return;
		}
		const MapDef& aTank = TankMap();
		int aHome = s.mTeam, aAway = 1 - s.mTeam;
		float aHpFrac = h.mHp / std::max(1.0f, s.MaxHp());
		std::vector<Target> aAll;
		s.Targets(h.mArena, aAll);
		const Target* aEnemyHero = nullptr;
		for (const Target& t : aAll)
			if (t.mHero && t.mTeam != s.mTeam)
				aEnemyHero = &t;
		std::vector<Target> aNear;
		for (const Target& t : aAll)
			if (t.mTeam != s.mTeam && !t.mMonster && Dist(t.mPos, h.mPos) < 360)
				aNear.push_back(t);

		if (mPassive)
		{
			// The tutorial's opponent: stays home and only defends itself.
			mPlan = PLAN_FARM;
			if (h.mArena != aHome)
				GoTo(s, aHome, aTank.mCore - Vec(0, 130), true);
			else if (h.mOrder == HeroState::ORD_IDLE && Dist(h.mPos, aTank.mCore - Vec(0, 130)) > 60)
				s.OrderMove(aTank.mCore - Vec(0, 130));
			return;
		}

		// Retreat when hurt; go back out once healed.
		float aLow = mSkill == 0 ? (h.mArena == aAway ? 0.2f : 0.12f) : (h.mArena == aAway ? 0.35f : (h.mArena == kTrench ? 0.25f : 0.2f));
		if (aHpFrac < aLow)
			mRetreatUntil = s.mNow + 60000;
		if (aHpFrac > 0.82f)
			mRetreatUntil = 0;
		if (!Elapsed(s.mNow, mRetreatUntil))
		{
			mPlan = PLAN_RETREAT;
			UseAbilities(s, aEnemyHero, aNear);
			GoTo(s, aHome, aTank.mCore - Vec(0, 130), true);
			return;
		}

		int aWhy;
		bool aDanger = HomeInDanger(s, aWhy);
		// Chasing a hero around my tank without killing it? Push theirs instead while my
		// towers can take it.
		bool aTowersOk = s.mArena.mTower[0].mHp > kTowerHealth * 0.5f && s.mArena.mTower[1].mHp > kTowerHealth * 0.5f;
		if (aDanger && aWhy == 1)
		{
			if (mDefendSince == 0)
			{
				mDefendSince = s.mNow;
				mKillsAtDefend = h.mKills;
			}
			else if (h.mKills > mKillsAtDefend)
				mDefendSince = 0;
			else if (Elapsed(s.mNow, mDefendSince + 40000) && aTowersOk)
			{
				mCounterUntil = s.mNow + 40000;
				mDefendSince = 0;
			}
		}
		else
			mDefendSince = 0;
		if (!Elapsed(s.mNow, mCounterUntil) && aTowersOk && aWhy == 1)
			aDanger = false;
		if (aDanger && (h.mArena == aHome || aWhy == 1 || s.mArena.AliveTowers() < 2))
		{
			mPlan = PLAN_DEFEND;
			if (h.mArena != aHome)
			{
				GoTo(s, aHome, aTank.mCore, true);
				return;
			}
			// Fight what's in my tank: the enemy hero, else the nearest invader.
			const Target* aBest = nullptr;
			float aBestD = 1e9f;
			for (const Target& t : aAll)
			{
				if (t.mTeam == s.mTeam || t.mStructure || t.mRef.mKind == ENT_FISH)
					continue;
				float d = Dist(t.mPos, h.mPos) - (t.mHero ? 250.0f : 0.0f);
				if (d < aBestD)
				{
					aBestD = d;
					aBest = &t;
				}
			}
			if (aBest != nullptr)
			{
				UseAbilities(s, aEnemyHero, aNear);
				Attack(s, aBest->mRef);
				return;
			}
		}

		if (h.mArena == aHome)
		{
			// Mop up invaders near home, then out to the Trench.
			const Target* aMinion = nullptr;
			float aBestD = 1e9f;
			for (const Target& t : aAll)
				if (t.mRef.mKind == ENT_MINION && t.mTeam != s.mTeam && Dist(t.mPos, h.mPos) < aBestD)
				{
					aBestD = Dist(t.mPos, h.mPos);
					aMinion = &t;
				}
			if (aMinion != nullptr && aBestD < 450)
			{
				mPlan = PLAN_DEFEND;
				Attack(s, aMinion->mRef);
				UseAbilities(s, aEnemyHero, aNear);
				return;
			}
			mPlan = PLAN_LANE;
			GoTo(s, kTrench, TrenchMap().mGateExit[s.mTeam]);
			return;
		}

		if (h.mArena == kTrench)
		{
			if (Monsters(s, aEnemyHero, aHpFrac))
				return;
			Lane(s, aAll, aEnemyHero, aHpFrac);
			return;
		}

		// In the rival's tank.
		if (!Raid(s, aAll, aEnemyHero, aHpFrac))
			GoTo(s, kTrench, TrenchMap().mPit + Vec(0, 220));
	}

	// The Trench's lane: stay a little behind your own wave, fight theirs, grab coins,
	// take on their hero when it looks good, follow your wave through their gate.
	bool Bot::Lane(Side& s, const std::vector<Target>& theAll, const Target* theEnemyHero, float theHpFrac)
	{
		HeroState& h = s.mHero;
		const MapDef& m = TrenchMap();
		float aDir = s.mTeam == 0 ? 1.0f : -1.0f;
		mPlan = PLAN_LANE;
		std::vector<Target> aNear;
		for (const Target& t : theAll)
			if (t.mTeam != s.mTeam && !t.mMonster && Dist(t.mPos, h.mPos) < 360)
				aNear.push_back(t);

		// Their hero: fight it when we're ahead or it's close and hurt.
		if (theEnemyHero != nullptr && Dist(theEnemyHero->mPos, h.mPos) < 420)
		{
			const HeroSnap* o = s.OtherHero();
			int aTheirLevel = o != nullptr ? o->mLevel : 1;
			bool aGood = theHpFrac > theEnemyHero->mHpFrac + 0.05f || (h.mLevel > aTheirLevel && theHpFrac > 0.5f) || theEnemyHero->mHpFrac < 0.3f;
			if (aGood)
			{
				mPlan = PLAN_FIGHT;
				UseAbilities(s, theEnemyHero, aNear);
				Attack(s, theEnemyHero->mRef);
				return true;
			}
		}
		// Coins close by, when their hero isn't right there.
		const ArenaSnap* aSnap = nullptr;
		ArenaSnap aOwned;
		if (s.Owns(kTrench))
		{
			aOwned = s.ViewArena(kTrench);
			aSnap = &aOwned;
		}
		else
			aSnap = s.LatestArena(kTrench);
		bool aEnemyClose = theEnemyHero != nullptr && Dist(theEnemyHero->mPos, h.mPos) < 260;
		if (aSnap != nullptr && !aEnemyClose)
		{
			const CoinSnap* aCoin = nullptr;
			float aBest = 230;
			for (const CoinSnap& c : aSnap->mCoins)
				if (Dist(c.mPos, h.mPos) < aBest && (!h.Def().mWalker || c.mPos.y > FloorY(kTrench, c.mPos.x) - 160))
				{
					aBest = Dist(c.mPos, h.mPos);
					aCoin = &c;
				}
			if (aCoin != nullptr && (mSkill > 0 || (s.mNow / 4000) % 2 == 0))
			{
				GoTo(s, kTrench, aCoin->mPos);
				return true;
			}
		}
		// Their minions: the weakest near.
		const Target* aMinion = nullptr;
		float aScore = 1e9f;
		for (const Target& t : theAll)
		{
			if (t.mRef.mKind != ENT_MINION || t.mTeam == s.mTeam || t.mMonster)
				continue;
			float d = Dist(t.mPos, h.mPos);
			if (d > 420)
				continue;
			float sc = d + t.mHpFrac * 150;
			if (sc < aScore)
			{
				aScore = sc;
				aMinion = &t;
			}
		}
		if (aMinion != nullptr)
		{
			UseAbilities(s, theEnemyHero, aNear);
			Attack(s, aMinion->mRef);
			return true;
		}
		// No fight: follow my wave. If it has gone through their gate, go with it.
		float aFront = -1e9f;
		int aMine = 0;
		for (const Target& t : theAll)
			if (t.mRef.mKind == ENT_MINION && t.mTeam == s.mTeam)
			{
				aMine++;
				aFront = std::max(aFront, t.mPos.x * aDir);
			}
		const ArenaSnap* aTheirs = s.LatestArena(1 - s.mTeam);
		int aInTheirs = 0;
		if (aTheirs != nullptr)
			for (const MinionSnap& mi : aTheirs->mMinions)
				aInTheirs += mi.mTeam == s.mTeam;
		// Push with the wave: it's through their gate (or about to be) and I'm healthy, or
		// their hero is down. That's how towers fall.
		const HeroSnap* o = s.OtherHero();
		bool aTheyDown = o != nullptr && !(o->mFlags & HF_ALIVE);
		float aGateX = m.mGate[1 - s.mTeam].x;
		bool aAtGate = aMine >= 2 && std::fabs(aFront * aDir - aGateX) < 260;
		if ((aInTheirs >= 2 || aAtGate || (aTheyDown && aMine >= 1)) && theHpFrac > 0.5f && h.mLevel >= 3 && (mSkill > 0 || h.mLevel >= 5))
		{
			mPlan = PLAN_PUSH;
			GoTo(s, 1 - s.mTeam, TankMap().mPortalExit);
			return true;
		}
		Vec aSpot;
		if (aMine > 0)
		{
			float x = aFront * aDir - aDir * 90;
			aSpot = Vec(Clamp(x, 160, kWorldW - 160), 640);
		}
		else
			aSpot = Vec(640 - aDir * 160, 640);
		if (h.Def().mWalker)
			aSpot = WalkerPos(kTrench, aSpot.x, h.Def().mRadius);
		GoTo(s, kTrench, aSpot);
		(void)m;
		return true;
	}

	// Camps, the Squid and the Boss, when it's safe enough.
	bool Bot::Monsters(Side& s, const Target* theEnemyHero, float theHpFrac)
	{
		HeroState& h = s.mHero;
		float aMin = s.MatchMs() / 60000.0f;
		const HeroSnap* o = s.OtherHero();
		bool aTheyAway = o == nullptr || !(o->mFlags & HF_ALIVE) || o->mArena != kTrench;
		int aTheirLevel = o != nullptr ? o->mLevel : 1;
		static const uint8_t kKind[MON_COUNT] = { MIN_CAMP_GUS, MIN_CAMP_BALROG, MIN_PSYCHO, MIN_BOSS };
		for (int sl = MON_BOSS; sl >= MON_GUS; sl--)
		{
			Vec aPos;
			float aFrac;
			if (!s.MonsterAlive(sl, &aPos, &aFrac))
				continue;
			bool aWant = false;
			bool aCamp = sl <= MON_BALROG;
			if (aCamp)
				aWant = (h.mLevel >= 4 || aMin >= 3) && theHpFrac > 0.7f && (mSkill > 0 || aMin > 5) && !s.HasBuff(sl == MON_GUS ? BUFF_GUS : BUFF_BALROG);
			else if (sl == MON_SQUID)
				aWant = h.mLevel >= 5 && theHpFrac > 0.75f && (aTheyAway || h.mLevel > aTheirLevel || aFrac < 0.35f) && mSkill > 0;
			else
				aWant = h.mLevel >= 7 && theHpFrac > 0.8f && (aTheyAway || aFrac < 0.3f) && mSkill > 0;
			// Mid-fight: finish it unless it's going badly.
			bool aEngaged = h.mAutoTarget.mKind == ENT_MINION && h.mAutoTarget.mArena == kTrench;
			if (!aWant && !(aEngaged && aFrac < 0.6f && theHpFrac > 0.35f))
				continue;
			// Their hero is right here and we're not winning: leave it.
			if (theEnemyHero != nullptr && Dist(theEnemyHero->mPos, h.mPos) < 300 && theHpFrac < theEnemyHero->mHpFrac)
				continue;
			std::vector<Target> all;
			s.Targets(kTrench, all);
			for (const Target& t : all)
				if (t.mMonster && t.mKind == kKind[sl])
				{
					mPlan = PLAN_MONSTER;
					std::vector<Target> aNear = { t };
					UseAbilities(s, nullptr, aNear);
					Attack(s, t.mRef);
					return true;
				}
		}
		return false;
	}

	// In the rival's tank: their hero when it looks good, structures with cover, their fish.
	bool Bot::Raid(Side& s, const std::vector<Target>& theAll, const Target* theEnemyHero, float theHpFrac)
	{
		HeroState& h = s.mHero;
		const MapDef& m = TankMap();
		mPlan = PLAN_PUSH;
		std::vector<Target> aNear;
		for (const Target& t : theAll)
			if (t.mTeam != s.mTeam && Dist(t.mPos, h.mPos) < 360)
				aNear.push_back(t);
		const ArenaSnap* aTheirs = s.LatestArena(h.mArena);
		if (theEnemyHero != nullptr && Dist(theEnemyHero->mPos, h.mPos) < 420)
		{
			const HeroSnap* o = s.OtherHero();
			int aTheirLevel = o != nullptr ? o->mLevel : 1;
			bool aGood = theHpFrac > theEnemyHero->mHpFrac + 0.1f || (h.mLevel > aTheirLevel + 1 && theHpFrac > 0.5f) || theEnemyHero->mHpFrac < 0.25f;
			if (aGood)
			{
				mPlan = PLAN_FIGHT;
				UseAbilities(s, theEnemyHero, aNear);
				Attack(s, theEnemyHero->mRef);
				return true;
			}
		}
		// Structures: with minions to tank, when the tower is low, or when tanky.
		const Target* aStruct = nullptr;
		float aBestD = 1e9f;
		bool aCoreOpen = aTheirs != nullptr && aTheirs->mTowerHp[0] <= 0 && aTheirs->mTowerHp[1] <= 0;
		for (const Target& t : theAll)
		{
			if (!t.mStructure || t.mTeam == s.mTeam)
				continue;
			if (t.mRef.mKind == ENT_CORE && !aCoreOpen)
				continue;
			float d = Dist(t.mPos, h.mPos);
			if (d < aBestD)
			{
				aBestD = d;
				aStruct = &t;
			}
		}
		int aCover = 0;
		if (aStruct != nullptr)
			for (const Target& t : theAll)
				if (t.mRef.mKind == ENT_MINION && t.mTeam == s.mTeam && Dist(t.mPos, aStruct->mPos) < 320)
					aCover++;
		if (aStruct != nullptr)
		{
			bool aTanky = h.mHero == HERO_RHUBARB || h.mHero == HERO_NIKO || s.ArmorVsStructures() < 0.75f;
			bool aGo = aCover >= 1 || aStruct->mHpFrac < 0.3f || (aTanky && theHpFrac > 0.6f) || aStruct->mRef.mKind == ENT_CORE
				|| (theHpFrac > 0.75f && h.mLevel >= 6);
			if (aGo)
			{
				UseAbilities(s, theEnemyHero, aNear);
				Attack(s, aStruct->mRef);
				return true;
			}
		}
		// Invaders of mine still alive here? Wait with them near the portal.
		int aMine = 0;
		for (const Target& t : theAll)
			if (t.mRef.mKind == ENT_MINION && t.mTeam == s.mTeam)
				aMine++;
		// Their fish, away from the towers.
		const Target* aFish = nullptr;
		aBestD = 1e9f;
		for (const Target& t : theAll)
		{
			if (t.mRef.mKind != ENT_FISH || t.mTeam == s.mTeam)
				continue;
			bool aGuarded = false;
			for (int i = 0; i < 2; i++)
				if (aTheirs != nullptr && aTheirs->mTowerHp[i] > 0 && Dist(t.mPos, m.mTower[i] - Vec(0, 40)) < kTowerRange + 40)
					aGuarded = true;
			if (aGuarded)
				continue;
			float d = Dist(t.mPos, h.mPos);
			if (d < aBestD)
			{
				aBestD = d;
				aFish = &t;
			}
		}
		if (aFish != nullptr && aMine > 0)
		{
			UseAbilities(s, theEnemyHero, aNear);
			Attack(s, aFish->mRef);
			return true;
		}
		if (aMine > 0)
		{
			GoTo(s, h.mArena, m.mPortalExit + Vec(0, 60));
			return true;
		}
		return false;							// nothing to do here: back to the Trench
	}

	bool Bot::UseAbilities(Side& s, const Target* theEnemy, const std::vector<Target>& theNear)
	{
		if (!Elapsed(s.mNow, mNextCast))
			return false;
		mNextCast = s.mNow + (mSkill >= 2 ? 250 : (mSkill == 1 ? 450 : 900));
		const HeroState& h = s.mHero;
		float aHpFrac = h.mHp / std::max(1.0f, s.MaxHp());
		float aEnemyD = theEnemy != nullptr ? Dist(theEnemy->mPos, h.mPos) : 1e9f;
		Target aCur;
		bool aHasTarget = h.mOrder == HeroState::ORD_ATTACK && s.FindTarget(h.mTarget, aCur);
		int aCount = 0;
		Vec aCenter;
		for (const Target& t : theNear)
			if (!t.mStructure)
			{
				aCount++;
				aCenter += t.mPos;
			}
		if (aCount > 0)
			aCenter = aCenter * (1.0f / aCount);
		auto Try = [&](int theSlot, Vec theAim) { return s.CanCast(theSlot) && s.Cast(theSlot, theAim); };
		bool aRetreating = !Elapsed(s.mNow, mRetreatUntil);
		const MapDef& aMap = MapOf(h.mArena);
		Vec aHomeWay = h.mArena == s.mTeam ? TankMap().mCore : (IsTank(h.mArena) ? aMap.mPad[0] : aMap.mGate[s.mTeam]);
		bool aMonster = aHasTarget && aCur.mMonster;
		Vec aFocus = theEnemy != nullptr ? theEnemy->mPos : (aHasTarget ? aCur.mPos : aCenter);
		bool aHaveFocus = theEnemy != nullptr || aHasTarget || aCount > 0;

		switch (h.Look())
		{
		case HERO_ITCHY:
			if (aRetreating && aEnemyD < 260 && Try(AB_E, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 110 && Try(AB_R, h.mPos))
				return true;
			if (aCount >= 4 && Dist(aCenter, h.mPos) < 110 && Try(AB_R, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 280 && aEnemyD > 90 && !aRetreating && Try(AB_Q, theEnemy->mPos))
				return true;
			if (aHasTarget && Try(AB_W, h.mPos))
				return true;
			break;
		case HERO_CLYDE:
			if (aRetreating && aEnemyD < 200 && Try(AB_E, aHomeWay))
				return true;
			if (theEnemy != nullptr && (theEnemy->mHpFrac < 0.5f || aCount >= 6) && Try(AB_R, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 480 && Try(AB_Q, theEnemy->mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 380 && Try(AB_W, theEnemy->mPos))
				return true;
			if (aCount >= 3 && Try(AB_W, aCenter))
				return true;
			if (aHasTarget && (aCur.mStructure || aMonster) && Try(AB_Q, aCur.mPos))
				return true;
			break;
		case HERO_RHUBARB:
			if ((aHpFrac < 0.45f && (aEnemyD < 250 || aHasTarget)) && Try(AB_E, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 200 && Try(AB_R, h.mPos))
				return true;
			if (aCount >= 4 && Dist(aCenter, h.mPos) < 200 && Try(AB_R, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 300 && Try(AB_W, theEnemy->mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 320 && aEnemyD > 100 && !aRetreating && Try(AB_Q, theEnemy->mPos))
				return true;
			if (aHasTarget && (aCur.mStructure || aMonster) && Dist(aCur.mPos, h.mPos) > 150 && Try(AB_Q, aCur.mPos))
				return true;
			break;
		case HERO_ANGIE:
		{
			const Arena& a = s.mArena;
			if (aHpFrac < 0.6f && Try(AB_Q, h.mPos))
				return true;
			if (aHpFrac < 0.7f && (aEnemyD < 400 || aMonster) && Try(AB_W, h.mPos))
				return true;
			if (h.mArena == s.mTeam)
				for (int i = 0; i < 2; i++)
					if (a.mTower[i].mAlive && a.mTower[i].mHp < kTowerHealth * 0.7f && Dist(TankMap().mTower[i], h.mPos) < 380)
					{
						if (Try(AB_Q, TankMap().mTower[i]) || Try(AB_W, TankMap().mTower[i]))
							return true;
					}
			int aMinions = 0;
			for (const Target& t : theNear)
				if (t.mRef.mKind == ENT_MINION && !t.mMonster && Dist(t.mPos, h.mPos) < 200)
					aMinions++;
			if (aMinions >= 3 && Try(AB_E, h.mPos))
				return true;
			if ((a.FishCount() < 5 && a.mDead.size() >= 3) || a.AliveTowers() < 2 || aHpFrac < 0.3f)
				if (Try(AB_R, h.mPos))
					return true;
			break;
		}
		case HERO_SPEEDY:
			if (aHpFrac < 0.4f && (aEnemyD < 250 || aHasTarget) && Try(AB_E, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 250 && Try(AB_Q, h.mPos))
				return true;
			if (aHasTarget && aCur.mRef.mKind == ENT_TOWER && Try(AB_W, aCur.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 300 && Try(AB_W, theEnemy->mPos))
				return true;
			if ((s.mArena.FishCount() >= 7 || (h.mArena == kTrench && s.HasTalent(1, 1))) && Try(AB_R, h.mPos))
				return true;
			break;
		case HERO_PRESTO:
			if (aRetreating && aEnemyD < 300 && Try(AB_E, h.mPos))
				return true;
			if (h.mCopyHero < 0 && theEnemy != nullptr && aEnemyD < 380 && aHpFrac > 0.4f && Try(AB_R, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 420 && Try(AB_Q, theEnemy->mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 360 && aEnemyD > 160 && aHpFrac > theEnemy->mHpFrac && Try(AB_W, theEnemy->mPos))
				return true;
			if (aCount >= 3 && Dist(aCenter, h.mPos) < 400 && Try(AB_Q, aCenter))
				return true;
			if (aMonster && Try(AB_Q, aCur.mPos))
				return true;
			break;
		case HERO_NIKO:
			if (aHpFrac < 0.6f && (aEnemyD < 300 || aMonster) && Try(AB_W, h.mPos))
				return true;
			if (((theEnemy != nullptr && aEnemyD < 300) || aCount >= 5) && aHpFrac > 0.3f && Try(AB_R, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 450 && Try(AB_E, theEnemy->mPos))
				return true;
			if (aHaveFocus && Dist(aFocus, h.mPos) < 360 && Try(AB_Q, Lerp(h.mPos, aFocus, 0.5f)))
				return true;
			if (aHasTarget && (aCur.mStructure || aMonster) && Try(AB_E, aCur.mPos))
				return true;
			break;
		case HERO_MERYL:
			if (theEnemy != nullptr && aEnemyD < 260 && (aRetreating || theEnemy->mHpFrac > 0.3f) && Try(AB_Q, theEnemy->mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 260 && aHpFrac > 0.45f && !aRetreating && Try(AB_R, h.mPos))
				return true;
			if ((aRetreating || aCount >= 3 || aMonster) && Try(AB_W, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 480 && Try(AB_E, theEnemy->mPos))
				return true;
			if (aCount >= 3 && Try(AB_E, aCenter))
				return true;
			if (aMonster && Try(AB_E, aCur.mPos))
				return true;
			break;
		case HERO_SHRAPNEL:
			if (aRetreating && aEnemyD < 260 && Try(AB_E, aHomeWay))
				return true;
			if (aHasTarget && aCur.mStructure && Try(AB_R, aCur.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 500 && theEnemy->mHpFrac < 0.5f && Try(AB_R, theEnemy->mPos))
				return true;
			if (aHaveFocus && Dist(aFocus, h.mPos) < 420 && Try(AB_Q, aFocus))
				return true;
			if ((theEnemy != nullptr && aEnemyD < 300) || (aRetreating && aEnemyD < 400))
				if (Try(AB_W, h.mPos))
					return true;
			if (aCount >= 2 && Try(AB_W, aCenter))
				return true;
			break;
		default:
			break;
		}
		return false;
	}
}
