#include "HeroesBot.h"
#include "HeroesMap.h"

namespace Heroes
{
	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }

	// Each hero's item order.
	static const uint8_t kBuild[HERO_COUNT][kItemSlots] =
	{
		{ ITEM_SHARP_FIN, ITEM_LEECH_TOOTH, ITEM_SPEED_KELP, ITEM_TOWER_BUSTER, ITEM_SHARP_FIN, ITEM_GOLDEN_SCALE },	// Itchy
		{ ITEM_PEARL_CHARM, ITEM_THICK_SHELL, ITEM_SHARP_FIN, ITEM_PEARL_CHARM, ITEM_TOWER_BUSTER, ITEM_GOLDEN_SCALE },	// Clyde
		{ ITEM_THICK_SHELL, ITEM_CORAL_ARMOR, ITEM_TOWER_BUSTER, ITEM_THICK_SHELL, ITEM_SHARP_FIN, ITEM_LEECH_TOOTH },	// Rhubarb
		{ ITEM_PEARL_CHARM, ITEM_THICK_SHELL, ITEM_GOLDEN_SCALE, ITEM_CORAL_ARMOR, ITEM_PEARL_CHARM, ITEM_SHARP_FIN },	// Angie
		{ ITEM_GOLDEN_SCALE, ITEM_THICK_SHELL, ITEM_TOWER_BUSTER, ITEM_CORAL_ARMOR, ITEM_SHARP_FIN, ITEM_SPEED_KELP },	// Stinky
	};

	void Bot::Think(Side& s)
	{
		if (s.Over())
			return;
		Farm(s);
		if (Elapsed(s.mNow, mNextShop))
		{
			mNextShop = s.mNow + 900;
			Shop(s);
		}
		if (Elapsed(s.mNow, mNextThink))
		{
			mNextThink = s.mNow + 250;
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
		// A person splits attention: fewer farm clicks while the hero fights or is away.
		static const uint32_t kPeriod[3] = { 650, 450, 320 };
		uint32_t aPeriod = kPeriod[std::clamp(mSkill, 0, 2)];
		if (s.mHero.mArena != s.mTeam || s.mHero.mOrder == HeroState::ORD_ATTACK)
			aPeriod = aPeriod * 2;
		mNextClick = s.mNow + aPeriod;
		Arena& a = s.mArena;
		const MapDef& m = TheMap();

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
		if (aThreat != nullptr && (s.mNow / 1000) % 3 != 0)		// most of the time
		{
			s.HomeClick(aThreat->mPos);
			return;
		}
		// A raiding hero in my tank: laser it every other click.
		HeroSnap o;
		if (s.OtherHeroIn(s.mTeam, &o) && (o.mFlags & HF_ALIVE) && !(o.mFlags & (HF_HIDDEN | HF_UNTARGETABLE)) && ((s.mNow / 500) & 1))
		{
			s.HomeClick(o.mPos);
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
			s.HomeClick(ClampToWater(aHungry->mPos - Vec(0, 36), 10));
			return;
		}
		if (aCoin != nullptr)
		{
			s.HomeClick(aCoin->mPos);
			return;
		}
		if (aCanFeed)
			s.HomeClick(ClampToWater(aHungry->mPos - Vec(0, 36), 10));
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
		const uint8_t* aBuild = kBuild[s.mHero.mHero];
		auto Try = [&](int theShop) {
			if (s.CanBuy(theShop) && a.mMoney - s.Price(theShop) >= 40)
			{
				s.Buy(theShop);
				return true;
			}
			return false;
		};
		auto Wants = [&](int theShop) { return s.CanBuy(theShop) || a.mMoney < s.Price(theShop); };
		auto Item = [&](int n) { return SHOP_ITEM_FIRST + aBuild[std::min(n, kItemSlots - 1)]; };

		// Towers first when they're hurt.
		for (int i = 0; i < 2; i++)
			if (a.mTower[i].mAlive && a.mTower[i].mHp < kTowerHealth * 0.55f && Try(i == 0 ? SHOP_REPAIR_LEFT : SHOP_REPAIR_RIGHT))
				return;

		struct Step { bool mWant; int mShop; };
		const Step kPlan[] = {
			{ aFish < 3, SHOP_GUPPY },
			{ aFish < 4 && aMin < 3, SHOP_GUPPY },
			{ aFish >= 4 && a.mFoodQuality == 0, SHOP_FOOD_QUALITY },
			{ aItems < 1 && aMin > 2, Item(0) },
			{ aFish < 6, SHOP_GUPPY },
			{ aItems < 1, Item(0) },
			{ aFish >= 6 && aBreeders == 0, SHOP_BREEDER },
			{ aFish >= 6 && a.mPellets < 3, SHOP_FOOD_COUNT },
			{ aItems < 2, Item(1) },
			{ aFish < 9, SHOP_GUPPY },
			{ aFish >= 8 && aCarnivores == 0, SHOP_CARNIVORE },
			{ aItems < 3, Item(2) },
			{ aMin > 6 && s.mWaveSizeRank < 1, SHOP_WAVE_SIZE },
			{ aItems < 4, Item(3) },
			{ a.mFoodQuality < 2, SHOP_FOOD_QUALITY },
			{ aMin > 7 && s.mWaveToughRank < 1, SHOP_WAVE_TOUGH },
			{ aFish < 12, SHOP_GUPPY },
			{ aItems < 5, Item(4) },
			{ a.mLaserLevel < 1, SHOP_LASER },
			{ aItems < 6, Item(5) },
			{ aMin > 9 && s.mWaveSizeRank < 2, SHOP_WAVE_SIZE },
			{ aMin > 9 && s.mWaveToughRank < 2, SHOP_WAVE_TOUGH },
			{ a.mTowerLevel < 1, SHOP_TOWER_UPGRADE },
			{ aMin > 8, SHOP_SEND_SYLV + (mBuildStep++ % 4) },
		};
		for (const Step& st : kPlan)
		{
			if (!st.mWant)
				continue;
			if (!Wants(st.mShop))
				continue;						// maxed or impossible: skip it
			Try(st.mShop);
			return;								// saving up for this one
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// The hero
	///////////////////////////////////////////////////////////////////////////
	void Bot::GoTo(Side& s, int theArena, Vec thePos)
	{
		const HeroState& h = s.mHero;
		const MapDef& m = TheMap();
		Vec aGoal = thePos;
		if (h.mArena != theArena)
		{
			// Through the portal (walkers ride its beam from the floor below).
			aGoal = m.mPortal;
		}
		if (h.mOrder == HeroState::ORD_MOVE && Dist(aGoal, mLastMove) < 30)
			return;
		if (h.mOrder == HeroState::ORD_IDLE && Dist(aGoal, h.mPos) < 20)
			return;
		mLastMove = aGoal;
		s.OrderMove(aGoal);
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
		const MapDef& m = TheMap();
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
		if (aNear >= 3)
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
		const MapDef& m = TheMap();
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
			if (t.mTeam != s.mTeam && Dist(t.mPos, h.mPos) < 360)
				aNear.push_back(t);

		// Retreat when hurt; go back out once healed.
		if (aHpFrac < (h.mArena == aAway ? 0.38f : 0.25f))
			mRetreatUntil = s.mNow + 60000;
		if (aHpFrac > 0.88f)
			mRetreatUntil = 0;
		if (!Elapsed(s.mNow, mRetreatUntil))
		{
			mPlan = PLAN_RETREAT;
			UseAbilities(s, aEnemyHero, aNear);
			GoTo(s, aHome, m.mCore - Vec(0, 130));
			return;
		}

		int aWhy;
		bool aDanger = HomeInDanger(s, aWhy);
		bool aEarly = s.MatchMs() < 150000;
		// Chasing a hero around my tank without killing it? Push theirs instead while
		// my towers can take it (their hero isn't home to defend).
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
				GoTo(s, aHome, m.mCore);
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
				if (h.mOrder != HeroState::ORD_ATTACK || h.mTarget != aBest->mRef)
					s.OrderAttack(aBest->mRef);
				return;
			}
		}

		if (h.mArena == aHome)
		{
			// Early on, farm invaders at home; afterwards head out once a wave has gone over.
			const Target* aMinion = nullptr;
			float aBestD = 1e9f;
			for (const Target& t : aAll)
				if (t.mRef.mKind == ENT_MINION && t.mTeam != s.mTeam && Dist(t.mPos, h.mPos) < aBestD)
				{
					aBestD = Dist(t.mPos, h.mPos);
					aMinion = &t;
				}
			if (aMinion != nullptr && (aEarly || aBestD < 400))
			{
				mPlan = PLAN_FARM;
				if (h.mOrder != HeroState::ORD_ATTACK || h.mTarget != aMinion->mRef)
					s.OrderAttack(aMinion->mRef);
				UseAbilities(s, aEnemyHero, aNear);
				return;
			}
			if (aEarly && s.MatchMs() < 60000)
			{
				mPlan = PLAN_FARM;
				GoTo(s, aHome, m.mPortalExit + Vec(0, 120));
				return;
			}
			mPlan = PLAN_PUSH;
			GoTo(s, aAway, m.mPortalExit);
			return;
		}

		// In the rival's tank.
		mPlan = PLAN_PUSH;
		const ArenaSnap* aTheirs = s.mOther.LatestArena();
		// Their hero: fight it when we're ahead or it's close and hurt.
		if (aEnemyHero != nullptr && Dist(aEnemyHero->mPos, h.mPos) < 420)
		{
			const HeroSnap* o = s.OtherHero();
			int aTheirLevel = o != nullptr ? o->mLevel : 1;
			bool aGood = aHpFrac > aEnemyHero->mHpFrac + 0.05f || (h.mLevel > aTheirLevel && aHpFrac > 0.5f) || aEnemyHero->mHpFrac < 0.3f;
			if (aGood)
			{
				mPlan = PLAN_FIGHT;
				UseAbilities(s, aEnemyHero, aNear);
				if (h.mOrder != HeroState::ORD_ATTACK || h.mTarget != aEnemyHero->mRef)
					s.OrderAttack(aEnemyHero->mRef);
				return;
			}
		}
		// Structures: with minions to tank, when the tower is low, or when tanky.
		const Target* aStruct = nullptr;
		float aBestD = 1e9f;
		bool aCoreOpen = aTheirs != nullptr && aTheirs->mTowerHp[0] <= 0 && aTheirs->mTowerHp[1] <= 0;
		for (const Target& t : aAll)
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
		if (aStruct != nullptr)
		{
			int aCover = 0;
			for (const Target& t : aAll)
				if (t.mRef.mKind == ENT_MINION && t.mTeam == s.mTeam && Dist(t.mPos, aStruct->mPos) < 300)
					aCover++;
			bool aTanky = h.mHero == HERO_RHUBARB || s.ArmorVsStructures() < 0.75f;
			bool aGo = aCover >= 2 || aStruct->mHpFrac < 0.3f || (aTanky && aHpFrac > 0.6f) || aStruct->mRef.mKind == ENT_CORE
				|| (aHpFrac > 0.8f && h.mLevel >= 6);
			if (aGo)
			{
				UseAbilities(s, aEnemyHero, aNear);
				if (h.mOrder != HeroState::ORD_ATTACK || h.mTarget != aStruct->mRef)
					s.OrderAttack(aStruct->mRef);
				return;
			}
		}
		// Their fish, away from the towers.
		const Target* aFish = nullptr;
		aBestD = 1e9f;
		for (const Target& t : aAll)
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
		if (aFish != nullptr)
		{
			UseAbilities(s, aEnemyHero, aNear);
			if (h.mOrder != HeroState::ORD_ATTACK || h.mTarget != aFish->mRef)
				s.OrderAttack(aFish->mRef);
			return;
		}
		// Nothing safe to do: wait by the portal for the next wave.
		GoTo(s, aAway, m.mPortalExit + Vec(0, 60));
	}

	bool Bot::UseAbilities(Side& s, const Target* theEnemy, const std::vector<Target>& theNear)
	{
		if (!Elapsed(s.mNow, mNextCast))
			return false;
		mNextCast = s.mNow + (mSkill >= 2 ? 250 : 450);
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
		Vec aHomeGate = h.mArena == s.mTeam ? TheMap().mCore : (h.Def().mWalker ? TheMap().mPad[0] : TheMap().mPortal);

		switch (h.mHero)
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
			if (aRetreating && aEnemyD < 200 && Try(AB_E, aHomeGate))
				return true;
			if (theEnemy != nullptr && (theEnemy->mHpFrac < 0.5f || aCount >= 6) && Try(AB_R, h.mPos))
				return true;
			if (theEnemy == nullptr && h.mArena != s.mTeam && s.mOther.LatestArena() != nullptr && s.mOther.LatestArena()->mFishCount >= 8 && Try(AB_R, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 480 && Try(AB_Q, theEnemy->mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 380 && Try(AB_W, theEnemy->mPos))
				return true;
			if (aCount >= 3 && Try(AB_W, aCenter))
				return true;
			if (aHasTarget && aCur.mStructure && Try(AB_Q, aCur.mPos))
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
			if (aHasTarget && aCur.mStructure && Dist(aCur.mPos, h.mPos) > 150 && Try(AB_Q, aCur.mPos))
				return true;
			break;
		case HERO_ANGIE:
		{
			const Arena& a = s.mArena;
			if (aHpFrac < 0.6f && Try(AB_Q, h.mPos))
				return true;
			if (aHpFrac < 0.7f && aEnemyD < 400 && Try(AB_W, h.mPos))
				return true;
			if (h.mArena == s.mTeam)
				for (int i = 0; i < 2; i++)
					if (a.mTower[i].mAlive && a.mTower[i].mHp < kTowerHealth * 0.7f && Dist(TheMap().mTower[i], h.mPos) < 380)
					{
						if (Try(AB_Q, TheMap().mTower[i]) || Try(AB_W, TheMap().mTower[i]))
							return true;
					}
			int aMinions = 0;
			for (const Target& t : theNear)
				if (t.mRef.mKind == ENT_MINION && Dist(t.mPos, h.mPos) < 200)
					aMinions++;
			if (aMinions >= 3 && Try(AB_E, h.mPos))
				return true;
			if ((a.FishCount() < 5 && a.mDead.size() >= 3) || a.AliveTowers() < 2 || aHpFrac < 0.3f)
				if (Try(AB_R, h.mPos))
					return true;
			break;
		}
		case HERO_STINKY:
			if (aHpFrac < 0.4f && (aEnemyD < 250 || aHasTarget) && Try(AB_E, h.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 250 && Try(AB_Q, h.mPos))
				return true;
			if (aHasTarget && aCur.mRef.mKind == ENT_TOWER && Try(AB_W, aCur.mPos))
				return true;
			if (theEnemy != nullptr && aEnemyD < 300 && Try(AB_W, theEnemy->mPos))
				return true;
			if (h.mArena == s.mTeam && s.mArena.FishCount() >= 7 && Try(AB_R, h.mPos))
				return true;
			break;
		default:
			break;
		}
		return false;
	}
}
