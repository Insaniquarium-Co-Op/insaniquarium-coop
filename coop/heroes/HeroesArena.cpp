#include "HeroesArena.h"
#include "HeroesMap.h"

namespace Heroes
{
	static const int	kFoodPrice = 5;
	static const float	kFishHp[FISH_KIND_COUNT][3] = { { 120, 200, 300 }, { 360, 360, 360 }, { 600, 600, 600 } };

	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }

	void Arena::Init(int theTeam, uint64_t theSeed, uint32_t theNow)
	{
		*this = Arena();
		mTeam = theTeam;
		mRng.Seed(theSeed);
		mLastStep = theNow;
		mCollectorPos = WalkerPos(TheMap().mCore.x + 160, kCollectorRadius);
		for (int i = 0; i < kStartGuppies; i++)
			AddFish(FISH_GUPPY, theNow, Vec(520 + 240.0f * i, 560));
	}

	int Arena::FishCount() const
	{
		int n = 0;
		for (const Fish& f : mFish)
			if (f.mDyingAt == 0)
				n++;
		return n;
	}

	bool Arena::AddFish(int theKind, uint32_t theNow, Vec thePos, int theSize)
	{
		if (FishCount() >= kMaxFish)
			return false;
		const FishDef& d = FishDefOf(theKind);
		Fish f;
		f.mId = mNextId++;
		f.mKind = (uint8_t)theKind;
		f.mSize = (uint8_t)(theKind == FISH_GUPPY ? theSize : SIZE_SMALL);
		if (theKind == FISH_GUPPY)
			f.mMeals = theSize == SIZE_LARGE ? kGuppyMealsLarge : (theSize == SIZE_MEDIUM ? kGuppyMealsMedium : 0);
		f.mPos = ClampToWater(thePos, f.Radius());
		f.mTarget = f.mPos;
		f.mHp = kFishHp[theKind][f.mSize];
		f.mHungryAt = theNow + (uint32_t)(d.mHungryAfterS * 1000);
		f.mNextCoin = theNow + (uint32_t)((kCoinEveryS + mRng.Range(-1.5f, 1.5f)) * 1000);
		f.mNextBreed = theNow + (uint32_t)(kBreedEveryS * 1000);
		f.mRight = mRng.Chance(0.5f);
		mFish.push_back(f);
		return true;
	}

	bool Arena::Spend(int thePrice)
	{
		if (mMoney < thePrice)
			return false;
		mMoney -= thePrice;
		return true;
	}

	void Arena::Earn(int theMoney, Vec theWhere, bool theShow)
	{
		if (theMoney <= 0)
			return;
		mMoney += theMoney;
		mMoneyEarned += theMoney;
		if (theShow)
			Text(theWhere, "+$" + std::to_string(theMoney), TC_MONEY);
	}

	void Arena::Text(Vec theAt, const std::string& theText, uint8_t theColor)
	{
		Event e;
		e.mType = EV_TEXT;
		e.mArena = (uint8_t)mTeam;
		e.mA = theAt;
		e.mText = theText;
		e.mParam = theColor;
		mOutEvents.push_back(e);
	}

	void Arena::Sound(uint8_t theSound, Vec theAt)
	{
		Event e;
		e.mType = EV_SOUND;
		e.mArena = (uint8_t)mTeam;
		e.mParam = theSound;
		e.mA = theAt;
		mOutEvents.push_back(e);
	}

	Minion* Arena::FindMinion(uint32_t theId)
	{
		for (Minion& m : mMinions)
			if (m.mId == theId && m.mHp > 0)
				return &m;
		return nullptr;
	}

	Fish* Arena::FindFish(uint32_t theId)
	{
		for (Fish& f : mFish)
			if (f.mId == theId && f.mDyingAt == 0)
				return &f;
		return nullptr;
	}

	void Arena::DropCoin(uint8_t theKind, Vec thePos)
	{
		Coin c;
		c.mId = mNextId++;
		c.mKind = theKind;
		c.mPos = thePos;
		mCoins.push_back(c);
	}

	///////////////////////////////////////////////////////////////////////////
	// Step
	///////////////////////////////////////////////////////////////////////////
	void Arena::Step(uint32_t theNow, const ArenaContext& theCtx)
	{
		float dt = std::min(0.1f, (theNow - mLastStep) / 1000.0f);
		mLastStep = theNow;
		mGrace = theCtx.mGrace;
		mSuddenDeath = theCtx.mSuddenDeath;
		StepFish(theNow, dt, theCtx);
		StepCoins(theNow, dt, theCtx);
		StepMinions(theNow, dt, theCtx);
		StepTowers(theNow, dt, theCtx);
		if (mCoreShield > 0 && Elapsed(theNow, mCoreShieldUntil))
			mCoreShield = 0;
	}

	void Arena::KillFish(Fish& f, uint32_t theNow, bool theCanRevive, bool theCountLoss)
	{
		if (f.mDyingAt != 0)
			return;
		if (theCanRevive && mGrace && mRng.Chance(0.25f))
		{
			// Angie's Grace: it comes back fed.
			f.mHp = kFishHp[f.mKind][f.mSize];
			f.mHungryAt = theNow + (uint32_t)(FishDefOf(f.mKind).mHungryAfterS * 1000);
			Event e;
			e.mType = EV_REVIVE;
			e.mArena = (uint8_t)mTeam;
			e.mA = f.mPos;
			mOutEvents.push_back(e);
			return;
		}
		f.mDyingAt = theNow;
		if (theCountLoss)
		{
			mFishLost++;
			mDead.push_back({ f.mKind, f.mSize, f.mPos });
			if (mDead.size() > 12)
				mDead.erase(mDead.begin());
		}
		Event e;
		e.mType = EV_FISH_DIED;
		e.mArena = (uint8_t)mTeam;
		e.mA = f.mPos;
		e.mParam = f.mKind;
		mOutEvents.push_back(e);
	}

	void Arena::StepFish(uint32_t theNow, float theDt, const ArenaContext& theCtx)
	{
		size_t aCount = mFish.size();			// babies added this step wait for the next
		for (size_t i = 0; i < aCount; i++)
		{
			Fish& f = mFish[i];
			if (f.mDyingAt != 0)
				continue;
			const FishDef& d = FishDefOf(f.mKind);
			bool aHungry = f.Hungry(theNow);
			if (aHungry && Elapsed(theNow, f.mHungryAt + (uint32_t)(d.mStarveS * 1000)))
			{
				mStarved++;
				KillFish(f, theNow, true, true);
				continue;
			}

			// Where to swim: food (or a small guppy) when hungry, else wander.
			float r = f.Radius();
			float aSpeed = d.mSpeed;
			bool aChasing = false;
			if (aHungry && f.mKind != FISH_CARNIVORE)
			{
				Food* aBest = nullptr;
				float aBestD = 1e9f;
				for (Food& fd : mFood)
				{
					float dd = Dist2(fd.mPos, f.mPos);
					if (dd < aBestD)
					{
						aBestD = dd;
						aBest = &fd;
					}
				}
				if (aBest != nullptr)
				{
					f.mTarget = aBest->mPos;
					aChasing = true;
					aSpeed *= 1.7f;
					if (Dist(aBest->mPos, f.mPos) < r + 12)
					{
						float aMeal = 1.0f + 0.5f * mFoodQuality;
						f.mMeals += aMeal;
						f.mHungryAt = theNow + (uint32_t)(d.mHungryAfterS * (1.0f + 0.25f * mFoodQuality) * 1000);
						f.mEatingUntil = theNow + 400;
						mFood.erase(mFood.begin() + (aBest - mFood.data()));
						if (f.mKind == FISH_GUPPY)
						{
							int aSize = f.mMeals >= kGuppyMealsLarge ? SIZE_LARGE : (f.mMeals >= kGuppyMealsMedium ? SIZE_MEDIUM : SIZE_SMALL);
							if (aSize != f.mSize)
							{
								f.mSize = (uint8_t)aSize;
								f.mHp = kFishHp[FISH_GUPPY][aSize];
								Sound(SND_GROW, f.mPos);
							}
						}
					}
				}
			}
			else if (aHungry && f.mKind == FISH_CARNIVORE)
			{
				Fish* aPrey = nullptr;
				float aBestD = 1e9f;
				for (size_t j = 0; j < aCount; j++)
				{
					Fish& g = mFish[j];
					if (g.mKind != FISH_GUPPY || g.mSize != SIZE_SMALL || g.mDyingAt != 0)
						continue;
					float dd = Dist2(g.mPos, f.mPos);
					if (dd < aBestD)
					{
						aBestD = dd;
						aPrey = &g;
					}
				}
				if (aPrey != nullptr)
				{
					f.mTarget = aPrey->mPos;
					aChasing = true;
					aSpeed *= 1.5f;
					if (Dist(aPrey->mPos, f.mPos) < r + aPrey->Radius())
					{
						KillFish(*aPrey, theNow, false, false);	// eaten by its own carnivore: not a loss
						f.mHungryAt = theNow + (uint32_t)(d.mHungryAfterS * 1000);
						f.mEatingUntil = theNow + 500;
						Sound(SND_CHOMP, f.mPos);
					}
				}
			}
			// Enemy heroes scare fish away.
			for (const HeroPresence& hp : theCtx.mHeroes)
				if (hp.mTeam != mTeam && hp.mTargetable && Dist(hp.mPos, f.mPos) < 170)
				{
					Vec aAway = Norm(f.mPos - hp.mPos);
					if (aAway.x == 0 && aAway.y == 0)
						aAway = Vec(1, 0);
					f.mTarget = ClampToWater(f.mPos + aAway * 160, r);
					f.mNextWander = theNow + 800;
					aChasing = true;
					aSpeed = d.mSpeed * 1.8f;
					break;
				}
			if (!aChasing)
			{
				if (Elapsed(theNow, f.mNextWander) || Dist(f.mTarget, f.mPos) < 8)
				{
					f.mTarget = ClampToWater(Vec(mRng.Range(60, kWorldW - 60), mRng.Range(kSurfaceY + 60, 740)), r);
					f.mNextWander = theNow + 3000 + mRng.Int(3000);
				}
				aSpeed *= 0.6f;
			}
			Vec aOld = f.mPos;
			StepToward(f.mPos, f.mTarget, aSpeed * theDt);
			f.mPos = ClampToWater(f.mPos, r);
			if (std::fabs(f.mPos.x - aOld.x) > 0.05f)
				f.mRight = f.mPos.x > aOld.x;

			// Money: fed fish drop coins.
			if (!aHungry)
			{
				int aDrops = theCtx.mGoldRush ? 2 : 1;
				if (f.mKind == FISH_GUPPY && f.mSize >= SIZE_MEDIUM && Elapsed(theNow, f.mNextCoin))
				{
					for (int k = 0; k < aDrops; k++)
						DropCoin(f.mSize == SIZE_LARGE ? COIN_GOLD : COIN_SILVER, f.mPos + Vec(k * 14.0f, 0));
					f.mNextCoin = theNow + (uint32_t)((kCoinEveryS + mRng.Range(-1.5f, 1.5f)) * 1000);
				}
				else if (f.mKind == FISH_CARNIVORE && Elapsed(theNow, f.mNextCoin))
				{
					for (int k = 0; k < aDrops; k++)
						DropCoin(COIN_DIAMOND, f.mPos + Vec(k * 14.0f, 0));
					f.mNextCoin = theNow + (uint32_t)((kDiamondEveryS + mRng.Range(-2, 2)) * 1000);
				}
				else if (f.mKind == FISH_BREEDER && Elapsed(theNow, f.mNextBreed))
				{
					if (AddFish(FISH_GUPPY, theNow, f.mPos + Vec(f.mRight ? -20.0f : 20.0f, 6)))
						Sound(SND_SPLASH, f.mPos);
					f.mNextBreed = theNow + (uint32_t)(kBreedEveryS * 1000);
				}
			}
		}
		for (size_t i = 0; i < mFish.size();)
			if (mFish[i].mDyingAt != 0 && Elapsed(theNow, mFish[i].mDyingAt + 1000))
				mFish.erase(mFish.begin() + i);
			else
				i++;
	}

	void Arena::StepCoins(uint32_t theNow, float theDt, const ArenaContext& theCtx)
	{
		// Gold Rush: coins fly to Speedy, or, while he's away, up into the portal and into
		// the wallet (D31).
		bool aPull = theCtx.mGoldRush;
		Vec aPullTo = theCtx.mOwnerHeroHere ? theCtx.mOwnerHeroPos : TheMap().mPortal;
		// Stinky the pet crawls toward the nearest coin that's low enough to reach soon.
		if (mCollectorLevel > 0)
		{
			const Coin* aBest = nullptr;
			float aBestD = 1e9f;
			for (const Coin& c : mCoins)
			{
				if (c.mPos.y < FloorY(c.mPos.x) - 160)
					continue;
				float d = std::fabs(c.mPos.x - mCollectorPos.x);
				if (d < aBestD)
				{
					aBestD = d;
					aBest = &c;
				}
			}
			if (aBest != nullptr && aBestD > 4)
			{
				float aStep = kCollectorSpeed[std::min(mCollectorLevel, 2) - 1] * theDt;
				float x = mCollectorPos.x + Clamp(aBest->mPos.x - mCollectorPos.x, -aStep, aStep);
				mCollectorRight = x > mCollectorPos.x;
				mCollectorPos = WalkerPos(x, kCollectorRadius);
			}
		}
		for (size_t i = 0; i < mCoins.size();)
		{
			Coin& c = mCoins[i];
			bool aTaken = false;
			if (aPull)
			{
				StepToward(c.mPos, aPullTo, 700 * theDt);
				aTaken = Dist(c.mPos, aPullTo) < 30;
			}
			else
			{
				float aFloor = FloorY(c.mPos.x) - 12;
				if (c.mPos.y < aFloor)
					c.mPos.y = std::min(aFloor, c.mPos.y + kCoinFallSpeed * theDt);
				else if (c.mLandedAt == 0)
					c.mLandedAt = theNow;
				if (theCtx.mScavenger && theCtx.mOwnerHeroHere && Dist(c.mPos, theCtx.mOwnerHeroPos) < 56)
					aTaken = true;
				if (mCollectorLevel > 0 && std::fabs(c.mPos.x - mCollectorPos.x) < kCollectorReach && c.mPos.y > FloorY(c.mPos.x) - 50)
					aTaken = true;				// Stinky the pet
			}
			if (aTaken)
			{
				Earn(CoinValue(c.mKind), c.mPos, true);
				Sound(c.mKind == COIN_DIAMOND ? SND_DIAMOND : SND_COIN, c.mPos);
				mCoins.erase(mCoins.begin() + i);
				continue;
			}
			if (c.mLandedAt != 0 && Elapsed(theNow, c.mLandedAt + (uint32_t)(kCoinFloorS * 1000)))
			{
				// Missed: it still pays half as it sinks away.
				int aBank = (int)(CoinValue(c.mKind) * kMissedCoinShare);
				Earn(aBank, c.mPos, false);
				mMissedMoney += aBank;
				Text(c.mPos - Vec(0, 16), "+$" + std::to_string(aBank), TC_INFO);
				mCoins.erase(mCoins.begin() + i);
				continue;
			}
			i++;
		}
		for (size_t i = 0; i < mFood.size();)
		{
			Food& fd = mFood[i];
			float aFloor = FloorY(fd.mPos.x) - 8;
			if (fd.mPos.y < aFloor)
				fd.mPos.y = std::min(aFloor, fd.mPos.y + kFoodFallSpeed * theDt);
			else if (fd.mLandedAt == 0)
				fd.mLandedAt = theNow;
			if (fd.mLandedAt != 0 && Elapsed(theNow, fd.mLandedAt + (uint32_t)(kFoodFloorS * 1000)))
				mFood.erase(mFood.begin() + i);
			else
				i++;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Minions
	///////////////////////////////////////////////////////////////////////////
	static float StructRadius(int theWhich) { return theWhich == 2 ? kCoreR : kTowerR; }

	Vec Arena::MinionGoal(Minion& m, uint32_t theNow, const ArenaContext& theCtx, float& theReach)
	{
		const MinionDef& d = MinionDefOf(m.mKind);
		const MapDef& aMap = TheMap();
		theReach = d.mRange + d.mRadius;
		if (m.mTarget.mKind == ENT_HERO)
		{
			for (const HeroPresence& h : theCtx.mHeroes)
				if (h.mPlayer == (int)m.mTarget.mId)
				{
					theReach += h.mRadius;
					return h.mPos;
				}
		}
		else if (m.mTarget.mKind == ENT_MINION)
		{
			if (Minion* o = FindMinion(m.mTarget.mId))
			{
				theReach += MinionDefOf(o->mKind).mRadius;
				return o->mPos;
			}
		}
		else if (m.mTarget.mKind == ENT_TOWER || m.mTarget.mKind == ENT_CORE)
		{
			int w = m.mTarget.mKind == ENT_CORE ? 2 : (int)m.mTarget.mId;
			// Follow the lane until its last waypoint, then go for the structure.
			const std::vector<Vec>& aLane = aMap.mLane[m.mLane];
			if (m.mWaypoint < (int)aLane.size() && w == m.mLane)
			{
				theReach = 24;
				return aLane[m.mWaypoint];
			}
			theReach += StructRadius(w);
			return w == 2 ? aMap.mCore : aMap.mTower[w];
		}
		theReach = 10;
		return m.mPos;
	}

	void Arena::StepMinions(uint32_t theNow, float theDt, const ArenaContext& theCtx)
	{
		for (size_t i = 0; i < mMinions.size();)
			if (mMinions[i].mHp <= 0)
				mMinions.erase(mMinions.begin() + i);
			else
				i++;
		for (size_t i = 0; i < mMinions.size(); i++)
		{
			Minion& m = mMinions[i];
			const MinionDef& d = MinionDefOf(m.mKind);
			int aTeam = m.Team(theNow);
			bool aHostile = aTeam != mTeam;			// attacking this tank

			// Pick a target now and then.
			if (Elapsed(theNow, m.mNextThink))
			{
				m.mNextThink = theNow + 400;
				EntityRef aTarget;
				float aBest = 1e9f;
				for (const HeroPresence& h : theCtx.mHeroes)
				{
					if (!h.mTargetable || h.mTeam == aTeam)
						continue;
					float dd = Dist(h.mPos, m.mPos);
					float aLimit = h.mTaunting ? 320.0f : kMinionAggroR;
					if (h.mTaunting)
						dd -= 1000;					// taunt wins
					if (Dist(h.mPos, m.mPos) < aLimit && dd < aBest)
					{
						aBest = dd;
						aTarget = EntityRef::Hero(h.mPlayer);
					}
				}
				if (!aTarget.Valid())
				{
					// Other minions fighting for the other side (charmed ones, or charmed me).
					for (Minion& o : mMinions)
					{
						if (&o == &m || o.mHp <= 0 || o.Team(theNow) == aTeam)
							continue;
						float dd = Dist(o.mPos, m.mPos);
						if (dd < (aHostile ? 120.0f : 400.0f) && dd < aBest)
						{
							aBest = dd;
							aTarget = EntityRef::Of(mTeam, ENT_MINION, o.mId);
						}
					}
				}
				if (!aTarget.Valid() && aHostile)
				{
					// The objective: my lane's tower, then the core once it can be hurt,
					// else the other tower.
					int aLaneTower = m.mLane;
					if (mTower[aLaneTower].mAlive)
						aTarget = EntityRef::Of(mTeam, ENT_TOWER, (uint32_t)aLaneTower);
					else if (CoreOpen())
						aTarget = EntityRef::Of(mTeam, ENT_CORE, 0);
					else if (mTower[1 - aLaneTower].mAlive)
					{
						m.mLane = (uint8_t)(1 - aLaneTower);
						m.mWaypoint = (int)TheMap().mLane[m.mLane].size();	// straight over
						aTarget = EntityRef::Of(mTeam, ENT_TOWER, (uint32_t)m.mLane);
					}
				}
				m.mTarget = aTarget;
			}

			if (Elapsed(theNow, m.mStunUntil) == false)
				continue;
			float aReach;
			Vec aGoal = MinionGoal(m, theNow, theCtx, aReach);
			float aSpeed = d.mSpeed * (Elapsed(theNow, m.mSlowUntil) ? 1.0f : (1.0f - m.mSlowPct));
			float aDist = Dist(aGoal, m.mPos);
			bool aAtWaypoint = (m.mTarget.mKind == ENT_TOWER || m.mTarget.mKind == ENT_CORE) && aReach == 24;
			if (aDist > aReach)
			{
				Vec aOld = m.mPos;
				StepToward(m.mPos, aGoal, aSpeed * theDt);
				m.mPos = ClampToWater(m.mPos, d.mRadius * 0.6f);
				if (std::fabs(m.mPos.x - aOld.x) > 0.05f)
					m.mRight = m.mPos.x > aOld.x;
			}
			else if (aAtWaypoint)
				m.mWaypoint++;
			else if (m.mTarget.Valid() && Elapsed(theNow, m.mNextHit))
			{
				// Attack.
				m.mNextHit = theNow + (uint32_t)(d.mHitS * 1000);
				m.mAttackingUntil = theNow + 300;
				m.mRight = aGoal.x > m.mPos.x;
				Hit h;
				h.mTeam = (uint8_t)aTeam;
				h.mSource = SRC_MINION;
				h.mPlayer = aHostile ? -1 : m.mCharmPlayer;
				if (m.mKind == MIN_DESTRUCTOR)
				{
					Bolt b;
					b.mPos = m.mPos;
					b.mTarget = m.mTarget;
					b.mTargetPos = aGoal;
					b.mDamage = (m.mTarget.mKind == ENT_HERO ? d.mHeroHit : d.mStructHit) * m.mMult;
					b.mTeam = (uint8_t)aTeam;
					b.mSource = SRC_MINION;
					mBolts.push_back(b);
					Event e;
					e.mType = EV_PROJECTILE;
					e.mArena = (uint8_t)mTeam;
					e.mA = m.mPos;
					e.mB = Norm(aGoal - m.mPos) * 420;
					e.mMs = (uint16_t)std::min(3000.0f, Dist(aGoal, m.mPos) / 420 * 1000);
					e.mParam = LOOK_DESTRUCTOR;
					mOutEvents.push_back(e);
				}
				else if (m.mTarget.mKind == ENT_HERO)
				{
					h.mDamage = d.mHeroHit * m.mMult;
					mOutHits.push_back({ m.mTarget, h });
				}
				else if (m.mTarget.mKind == ENT_MINION)
				{
					h.mDamage = d.mHeroHit * m.mMult;
					if (Minion* o = FindMinion(m.mTarget.mId))
						HurtMinion(*o, h, theNow);
				}
				else
				{
					h.mDamage = d.mStructHit * m.mMult;
					HurtStructure(m.mTarget.mKind == ENT_CORE ? 2 : (int)m.mTarget.mId, h, theNow);
				}
			}

			// Hungry aliens bite fish on the way.
			if (aHostile && d.mEatsFish && Elapsed(theNow, m.mNextBite))
				for (Fish& f : mFish)
					if (f.mDyingAt == 0 && f.mKind == FISH_GUPPY && (f.mSize != SIZE_LARGE || m.mKind == MIN_GUS)
						&& Dist(f.mPos, m.mPos) < d.mRadius * 0.8f + f.Radius())
					{
						mFishBitten++;
						KillFish(f, theNow, true, true);
						Sound(SND_CHOMP, f.mPos);
						m.mNextBite = theNow + (uint32_t)(kMinionBiteS * 1000);
						break;
					}
		}
	}

	void Arena::HurtMinion(Minion& m, const Hit& theHit, uint32_t theNow)
	{
		if (m.mHp <= 0)
			return;
		if (theHit.mCharmMs > 0)
		{
			if (m.mKind == MIN_MINI || m.mKind == MIN_SYLV)	// big aliens are too proud
			{
				m.mCharmUntil = theNow + theHit.mCharmMs;
				m.mCharmTeam = theHit.mTeam;
				m.mCharmPlayer = theHit.mPlayer;
				m.mTarget = EntityRef();
				m.mNextThink = theNow;
			}
			return;
		}
		if (theHit.mStunMs > 0)
			m.mStunUntil = std::max(m.mStunUntil, theNow + theHit.mStunMs);
		if (theHit.mSlowMs > 0)
		{
			m.mSlowUntil = theNow + theHit.mSlowMs;
			m.mSlowPct = theHit.mSlowPct;
		}
		if (theHit.mPull)
			m.mPos = ClampToWater(theHit.mPullTo, MinionDefOf(m.mKind).mRadius * 0.6f);
		if (theHit.mPush.x != 0 || theHit.mPush.y != 0)
			m.mPos = ClampToWater(m.mPos + theHit.mPush, MinionDefOf(m.mKind).mRadius * 0.6f);
		if (theHit.mDamage <= 0)
			return;
		m.mHp -= theHit.mDamage;
		if (theHit.mPlayer >= 0)
			m.mLastHitBy = theHit.mPlayer;
		if (m.mHp <= 0)
		{
			const MinionDef& d = MinionDefOf(m.mKind);
			mMinionsKilled++;
			Reward r;
			r.mPlayer = theHit.mPlayer;
			r.mTeam = (int8_t)theHit.mTeam;
			r.mXp = m.mKind == MIN_MINI ? kXpMinion : kXpBigAlien;
			r.mMoney = d.mBounty;
			r.mWhat = ENT_MINION;
			mOutRewards.push_back(r);
			Event e;
			e.mType = EV_BURST;
			e.mArena = (uint8_t)mTeam;
			e.mA = m.mPos;
			e.mValue = d.mRadius;
			e.mParam = LOOK_NONE;
			mOutEvents.push_back(e);
			Sound(SND_EXPLODE, m.mPos);
			Text(m.mPos - Vec(0, 20), "+$" + std::to_string(d.mBounty), TC_MONEY);
		}
	}

	// Backdoor protection: a hero's hits on a structure count for little unless that
	// hero's minions are there too (towers are for pushing with a wave, not soloing).
	float Arena::Backdoor(int theWhich, const Hit& theHit, uint32_t theNow) const
	{
		if (theHit.mSource != SRC_ATTACK && theHit.mSource != SRC_ABILITY && theHit.mSource != SRC_ZONE)
			return 1.0f;
		Vec aAt = theWhich == 2 ? TheMap().mCore : TheMap().mTower[theWhich];
		for (const Minion& m : mMinions)
			if (m.mHp > 0 && m.Team(theNow) == theHit.mTeam && Dist(m.mPos, aAt) < kBackdoorR)
				return 1.0f;
		return kBackdoor;
	}

	void Arena::HurtStructure(int theWhich, const Hit& theHit, uint32_t theNow)
	{
		const MapDef& aMap = TheMap();
		if (theWhich < 2)
		{
			Tower& t = mTower[theWhich];
			if (!t.mAlive)
				return;
			if (theHit.mHeal > 0)
				t.mHp = std::min(kTowerHealth, t.mHp + theHit.mHeal);
			if (theHit.mShield > 0)
			{
				t.mShield = std::max(t.mShield, theHit.mShield);
				t.mShieldUntil = theNow + theHit.mShieldMs;
			}
			if (theHit.mBlindMs > 0)
				t.mBlindUntil = std::max(t.mBlindUntil, theNow + theHit.mBlindMs);
			if (theHit.mDamage <= 0)
				return;
			float aDamage = theHit.mDamage * Backdoor(theWhich, theHit, theNow);
			if (t.mShield > 0)
			{
				float a = std::min(t.mShield, aDamage);
				t.mShield -= a;
				aDamage -= a;
			}
			t.mHp -= aDamage;
			t.mLastHurtAt = theNow;
			if (t.mHp <= 0)
			{
				t.mHp = 0;
				t.mAlive = false;
				Reward r;
				r.mPlayer = theHit.mPlayer;
				r.mTeam = (int8_t)theHit.mTeam;
				r.mXp = kXpTower;
				r.mMoney = kBountyTower;
				r.mWhat = ENT_TOWER;
				mOutRewards.push_back(r);
				Event e;
				e.mType = EV_TOWER_DOWN;
				e.mArena = (uint8_t)mTeam;
				e.mId = (uint32_t)theWhich;
				e.mA = aMap.mTower[theWhich];
				e.mPlayer = theHit.mPlayer;
				mOutEvents.push_back(e);
				Event b;
				b.mType = EV_BURST;
				b.mArena = (uint8_t)mTeam;
				b.mA = aMap.mTower[theWhich];
				b.mValue = 90;
				mOutEvents.push_back(b);
				Sound(SND_EXPLODE, aMap.mTower[theWhich]);
			}
			return;
		}
		if (theHit.mHeal > 0)
			mCoreHp = std::min(kCoreHealth, mCoreHp + theHit.mHeal);
		if (theHit.mShield > 0)
		{
			mCoreShield = std::max(mCoreShield, theHit.mShield);
			mCoreShieldUntil = theNow + theHit.mShieldMs;
		}
		if (theHit.mDamage <= 0 || mCoreDead)
			return;
		if (!CoreOpen())
		{
			if (Elapsed(theNow, mLastDamageSound + 1500))
			{
				mLastDamageSound = theNow;
				Text(aMap.mCore - Vec(0, 70), "Protected by the towers!", TC_INFO);
			}
			return;
		}
		float aDamage = theHit.mDamage * (mSuddenDeath ? 1.0f : kCoreArmor) * Backdoor(2, theHit, theNow);
		if (mCoreShield > 0)
		{
			float a = std::min(mCoreShield, aDamage);
			mCoreShield -= a;
			aDamage -= a;
		}
		mCoreHp -= aDamage;
		if (mCoreHp <= 0)
		{
			mCoreHp = 0;
			mCoreDead = true;
			Event b;
			b.mType = EV_BURST;
			b.mArena = (uint8_t)mTeam;
			b.mA = aMap.mCore;
			b.mValue = 160;
			mOutEvents.push_back(b);
			Sound(SND_EXPLODE, aMap.mCore);
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Towers and bolts
	///////////////////////////////////////////////////////////////////////////
	void Arena::StepTowers(uint32_t theNow, float theDt, const ArenaContext& theCtx)
	{
		const MapDef& aMap = TheMap();
		for (int i = 0; i < 2; i++)
		{
			Tower& t = mTower[i];
			if (t.mShield > 0 && Elapsed(theNow, t.mShieldUntil))
				t.mShield = 0;
			if (!t.mAlive || !Elapsed(theNow, t.mNextShot) || !Elapsed(theNow, t.mBlindUntil))
				continue;
			Vec aGun = TowerGun(i);
			float aRange = TowerRange();
			EntityRef aTarget;
			Vec aTargetPos;
			int aRank = 99;
			float aBest = 1e9f;
			// 0: a hero who just hurt my hero; 1: a taunting hero; 2: minions; 3: heroes.
			for (const HeroPresence& h : theCtx.mHeroes)
			{
				if (h.mTeam == mTeam || !h.mTargetable)
					continue;
				float dd = Dist(h.mPos, aGun);
				if (dd > aRange + h.mRadius || !LineOfSight(aGun, h.mPos))
					continue;
				int aR = (h.mHurtMyHeroAt != 0 && !Elapsed(theNow, h.mHurtMyHeroAt + (uint32_t)(kTowerAggroS * 1000))) ? 0 : (h.mTaunting ? 1 : 3);
				if (aR < aRank || (aR == aRank && dd < aBest))
				{
					aRank = aR;
					aBest = dd;
					aTarget = EntityRef::Hero(h.mPlayer);
					aTargetPos = h.mPos;
				}
			}
			for (Minion& m : mMinions)
			{
				if (m.mHp <= 0 || m.Team(theNow) == mTeam)
					continue;
				float dd = Dist(m.mPos, aGun);
				if (dd > aRange + MinionDefOf(m.mKind).mRadius || !LineOfSight(aGun, m.mPos))
					continue;
				if (2 < aRank || (aRank == 2 && dd < aBest))
				{
					aRank = 2;
					aBest = dd;
					aTarget = EntityRef::Of(mTeam, ENT_MINION, m.mId);
					aTargetPos = m.mPos;
				}
			}
			if (!aTarget.Valid())
				continue;
			t.mNextShot = theNow + (uint32_t)(kTowerPeriodS * 1000);
			// Focus: shots in a row at the same hero hit harder.
			if (aTarget.mKind == ENT_HERO && aTarget == t.mLastTarget)
				t.mRamp = std::min(kTowerRampMax, t.mRamp + 1);
			else
				t.mRamp = 0;
			t.mLastTarget = aTarget;
			Bolt b;
			b.mPos = aGun;
			b.mTarget = aTarget;
			b.mTargetPos = aTargetPos;
			b.mDamage = TowerDamage() * (1.0f + kTowerRampPerShot * t.mRamp);
			b.mTeam = (uint8_t)mTeam;
			b.mSource = SRC_TOWER;
			mBolts.push_back(b);
			Event e;
			e.mType = EV_BOLT;
			e.mArena = (uint8_t)mTeam;
			e.mA = aGun;
			e.mB = aTargetPos;
			e.mMs = (uint16_t)(Dist(aGun, aTargetPos) / kTowerBoltSpeed * 1000);
			mOutEvents.push_back(e);
			Sound(SND_TOWER, aGun);
		}

		// Bolts home in on their target and hit on arrival.
		for (size_t i = 0; i < mBolts.size();)
		{
			Bolt& b = mBolts[i];
			bool aGone = false;
			if (b.mTarget.mKind == ENT_HERO)
			{
				aGone = true;
				for (const HeroPresence& h : theCtx.mHeroes)
					if (h.mPlayer == (int)b.mTarget.mId && h.mTargetable)
					{
						b.mTargetPos = h.mPos;
						aGone = false;
					}
			}
			else if (b.mTarget.mKind == ENT_MINION)
			{
				Minion* m = FindMinion(b.mTarget.mId);
				if (m != nullptr)
					b.mTargetPos = m->mPos;
				else
					aGone = true;
			}
			if (aGone)
			{
				mBolts.erase(mBolts.begin() + i);
				continue;
			}
			float aSpeed = b.mSource == SRC_TOWER ? kTowerBoltSpeed : 420;
			if (StepToward(b.mPos, b.mTargetPos, aSpeed * theDt) || Dist(b.mPos, b.mTargetPos) < 16)
			{
				Hit h;
				h.mDamage = b.mDamage;
				h.mTeam = b.mTeam;
				h.mSource = b.mSource;
				if (b.mTarget.mKind == ENT_HERO)
					mOutHits.push_back({ b.mTarget, h });
				else if (b.mTarget.mKind == ENT_MINION)
				{
					if (Minion* m = FindMinion(b.mTarget.mId))
						HurtMinion(*m, h, theNow);
				}
				else if (b.mTarget.mKind == ENT_TOWER || b.mTarget.mKind == ENT_CORE)
					HurtStructure(b.mTarget.mKind == ENT_CORE ? 2 : (int)b.mTarget.mId, h, theNow);
				mBolts.erase(mBolts.begin() + i);
				continue;
			}
			i++;
		}
		(void)aMap;
	}

	///////////////////////////////////////////////////////////////////////////
	// Actions
	///////////////////////////////////////////////////////////////////////////
	Arena::ClickKind Arena::Click(Vec p, uint32_t theNow, const ArenaContext& theCtx)
	{
		// A coin under the cursor.
		for (size_t i = 0; i < mCoins.size(); i++)
			if (Dist(mCoins[i].mPos, p) < kCoinClickR)
			{
				Coin c = mCoins[i];
				mCoins.erase(mCoins.begin() + i);
				Earn(CoinValue(c.mKind), c.mPos, true);
				Sound(c.mKind == COIN_DIAMOND ? SND_DIAMOND : SND_COIN, c.mPos);
				return CLICK_COIN;
			}
		// An enemy under the cursor: the laser.
		EntityRef aTarget;
		float aBest = 1e9f;
		for (Minion& m : mMinions)
		{
			if (m.mHp <= 0 || m.Team(theNow) == mTeam)
				continue;
			float dd = Dist(m.mPos, p);
			if (dd < std::max(kLaserClickR, MinionDefOf(m.mKind).mRadius) && dd < aBest)
			{
				aBest = dd;
				aTarget = EntityRef::Of(mTeam, ENT_MINION, m.mId);
			}
		}
		for (const HeroPresence& h : theCtx.mHeroes)
		{
			if (h.mTeam == mTeam || !h.mTargetable)
				continue;
			float dd = Dist(h.mPos, p);
			if (dd < kLaserClickR + h.mRadius * 0.5f && dd < aBest)
			{
				aBest = dd;
				aTarget = EntityRef::Hero(h.mPlayer);
			}
		}
		if (aTarget.Valid())
		{
			if (!Elapsed(theNow, mNextLaser))
				return CLICK_NONE;
			mNextLaser = theNow + (uint32_t)(kLaserPeriodS * 1000);
			Hit h;
			h.mTeam = (uint8_t)mTeam;
			h.mPlayer = (int8_t)theCtx.mOwnerPlayer;
			h.mSource = SRC_LASER;
			h.mDamage = LaserDamage(mLaserLevel);
			if (aTarget.mKind == ENT_HERO)
			{
				h.mDamage *= kLaserVsHero;
				mOutHits.push_back({ aTarget, h });
			}
			else if (Minion* m = FindMinion(aTarget.mId))
				HurtMinion(*m, h, theNow);
			Event e;
			e.mType = EV_BURST;
			e.mArena = (uint8_t)mTeam;
			e.mA = p;
			e.mValue = 18;
			e.mParam = LOOK_ZAP;
			mOutEvents.push_back(e);
			Sound(SND_LASER, p);
			return CLICK_LASER;
		}
		// Otherwise food.
		if ((int)mFood.size() >= mPellets || p.y > FloorY(p.x) - 10 || p.y < kSurfaceY)
			return CLICK_REFUSED;
		if (!Spend(kFoodPrice))
			return CLICK_REFUSED;
		Food fd;
		fd.mId = mNextId++;
		fd.mPos = p;
		mFood.push_back(fd);
		Sound(SND_SPLASH, p);
		return CLICK_FOOD;
	}

	void Arena::ApplyHit(const EntityRef& theTarget, const Hit& theHit, uint32_t theNow)
	{
		switch (theTarget.mKind)
		{
		case ENT_FISH:
			if (Fish* f = FindFish(theTarget.mId))
			{
				if (theHit.mDamage <= 0)
					break;
				f->mHp -= theHit.mDamage;
				if (f->mHp <= 0)
				{
					mFishKilled++;
					KillFish(*f, theNow, true, true);
					if (f->mDyingAt != 0)
					{
						Reward r;
						r.mPlayer = theHit.mPlayer;
						r.mTeam = (int8_t)theHit.mTeam;
						r.mXp = kXpFish;
						r.mMoney = kBountyFish;
						r.mWhat = ENT_FISH;
						mOutRewards.push_back(r);
					}
				}
			}
			break;
		case ENT_MINION:
			if (Minion* m = FindMinion(theTarget.mId))
				HurtMinion(*m, theHit, theNow);
			break;
		case ENT_TOWER:
			if (theTarget.mId < 2)
				HurtStructure((int)theTarget.mId, theHit, theNow);
			break;
		case ENT_CORE:
			HurtStructure(2, theHit, theNow);
			break;
		default:
			break;
		}
	}

	void Arena::SpawnWave(const std::vector<uint8_t>& theKinds, float theMult, int theFromTeam, uint32_t theNow)
	{
		const MapDef& aMap = TheMap();
		for (size_t i = 0; i < theKinds.size(); i++)
		{
			const MinionDef& d = MinionDefOf(theKinds[i]);
			Minion m;
			m.mId = mNextId++;
			m.mKind = theKinds[i];
			m.mTeam = (uint8_t)theFromTeam;
			m.mLane = (uint8_t)(i % 2);
			m.mPos = aMap.mPortal + Vec(mRng.Range(-30, 30), mRng.Range(-10, 30));
			m.mMult = theMult;
			m.mHp = m.mMaxHp = d.mHealth * theMult;
			m.mNextThink = theNow;
			m.mNextHit = theNow + 500;
			m.mNextBite = theNow + 1000;
			m.mRight = m.mLane == 1;
			mMinions.push_back(m);
		}
		Event e;
		e.mType = EV_WAVE;
		e.mArena = (uint8_t)mTeam;
		e.mA = aMap.mPortal;
		e.mValue = (float)theKinds.size();
		mOutEvents.push_back(e);
		Sound(SND_WARP, aMap.mPortal);
	}

	void Arena::Revive(int theCount, uint32_t theNow)
	{
		for (int i = 0; i < theCount && !mDead.empty(); i++)
		{
			DeadFish d = mDead.back();
			mDead.pop_back();
			if (!AddFish(d.mKind, theNow, d.mPos, d.mSize))
				break;
			Event e;
			e.mType = EV_REVIVE;
			e.mArena = (uint8_t)mTeam;
			e.mA = d.mPos;
			mOutEvents.push_back(e);
		}
	}

	ArenaSnap Arena::Snapshot() const
	{
		ArenaSnap s;
		s.mTeam = (uint8_t)mTeam;
		for (int i = 0; i < 2; i++)
		{
			s.mTowerHp[i] = mTower[i].mAlive ? mTower[i].mHp : 0;
			s.mTowerShield[i] = mTower[i].mShield;
			if (!Elapsed(mLastStep, mTower[i].mBlindUntil))
				s.mTowerBlind |= (uint8_t)(1 << i);
		}
		s.mTowerLevel = (uint8_t)mTowerLevel;
		s.mCoreHp = mCoreHp;
		s.mCoreShield = mCoreShield;
		s.mFishCount = (uint16_t)FishCount();
		for (const Fish& f : mFish)
		{
			uint8_t aFlags = (f.mRight ? FF_RIGHT : 0) | (f.Hungry(mLastStep) ? FF_HUNGRY : 0) | (f.mDyingAt != 0 ? FF_DYING : 0)
				| (!Elapsed(mLastStep, f.mEatingUntil) ? FF_EATING : 0);
			s.mFish.push_back({ f.mId, f.mKind, f.mSize, aFlags, f.mPos });
		}
		for (const Coin& c : mCoins)
			s.mCoins.push_back({ c.mId, c.mKind, c.mPos });
		s.mFoodQuality = (uint8_t)mFoodQuality;
		s.mCollectorLevel = (uint8_t)mCollectorLevel;
		s.mCollectorPos = mCollectorPos;
		s.mCollectorRight = mCollectorRight;
		for (const Food& fd : mFood)
			s.mFood.push_back({ fd.mId, fd.mPos });
		for (const Minion& m : mMinions)
		{
			if (m.mHp <= 0)
				continue;
			uint8_t aFlags = (m.mRight ? MF_RIGHT : 0) | (!Elapsed(mLastStep, m.mCharmUntil) ? MF_CHARMED : 0)
				| (!Elapsed(mLastStep, m.mStunUntil) ? MF_STUNNED : 0) | (!Elapsed(mLastStep, m.mAttackingUntil) ? MF_ATTACKING : 0);
			s.mMinions.push_back({ m.mId, m.mKind, aFlags, (uint8_t)m.Team(mLastStep), m.mHp / std::max(1.0f, m.mMaxHp), m.mPos });
		}
		return s;
	}
}
