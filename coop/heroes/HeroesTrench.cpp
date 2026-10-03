#include "HeroesTrench.h"
#include "HeroesMap.h"

namespace Heroes
{
	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }
	static const uint8_t kMonsterKind[MON_COUNT] = { MIN_CAMP_GUS, MIN_CAMP_BALROG, MIN_PSYCHO, MIN_BOSS };
	static const uint8_t kMonsterBuff[MON_COUNT] = { BUFF_GUS, BUFF_BALROG, BUFF_SQUID, BUFF_BOSS };

	void Trench::Init(uint64_t theSeed, uint32_t theNow)
	{
		*this = Trench();
		mRng.Seed(theSeed);
		mLastStep = theNow;
		for (int i = 0; i < MON_COUNT; i++)
			mMonsterAt[i] = theNow + (uint32_t)(kMonsterFirstS[i] * 1000);
	}

	Minion* Trench::FindMinion(uint32_t theId)
	{
		for (Minion& m : mMinions)
			if (m.mId == theId && m.mHp > 0)
				return &m;
		return nullptr;
	}

	const Minion* Trench::Monster(int theSlot) const
	{
		if (mMonsterId[theSlot] == 0)
			return nullptr;
		for (const Minion& m : mMinions)
			if (m.mId == mMonsterId[theSlot] && m.mHp > 0)
				return &m;
		return nullptr;
	}

	uint32_t Trench::MonsterIn(int theSlot, uint32_t theNow) const
	{
		if (mMonsterId[theSlot] != 0 || Elapsed(theNow, mMonsterAt[theSlot]))
			return 0;
		return mMonsterAt[theSlot] - theNow;
	}

	void Trench::Sound(uint8_t theSound, Vec theAt)
	{
		Event e;
		e.mType = EV_SOUND;
		e.mArena = kTrench;
		e.mParam = theSound;
		e.mA = theAt;
		mOutEvents.push_back(e);
	}

	void Trench::Burst(Vec theAt, float theRadius, uint8_t theLook)
	{
		Event e;
		e.mType = EV_BURST;
		e.mArena = kTrench;
		e.mA = theAt;
		e.mValue = theRadius;
		e.mParam = theLook;
		mOutEvents.push_back(e);
	}

	void Trench::SpawnWave(const std::vector<Arrival>& theMinions, int theFromTeam, uint32_t theNow)
	{
		const MapDef& m = TrenchMap();
		int t = std::clamp(theFromTeam, 0, 1);
		Vec aGate = m.mGate[t];
		float aOut = t == 0 ? 1.0f : -1.0f;
		for (size_t i = 0; i < theMinions.size(); i++)
		{
			const Arrival& a = theMinions[i];
			const MinionDef& d = MinionDefOf(a.mKind);
			Minion n;
			n.mId = mNextId++;
			n.mKind = a.mKind;
			n.mTeam = (uint8_t)t;
			n.mPos = ClampToWater(kTrench, aGate + Vec(aOut * (40 + 18.0f * (i % 4)), mRng.Range(-40, 20)), d.mRadius * 0.6f);
			n.mMult = a.mMult;
			n.mMaxHp = d.mHealth * a.mMult;
			n.mHp = std::max(1.0f, n.mMaxHp * Clamp(a.mHpFrac, 0.01f, 1));
			n.mNextThink = theNow;
			n.mNextHit = theNow + 400;
			n.mRight = t == 0;
			mMinions.push_back(n);
		}
		if (theMinions.empty())
			return;
		Event e;
		e.mType = EV_WAVE;
		e.mArena = kTrench;
		e.mPlayer = (int8_t)t;
		e.mA = aGate;
		e.mValue = (float)theMinions.size();
		mOutEvents.push_back(e);
	}

	///////////////////////////////////////////////////////////////////////////
	// Step
	///////////////////////////////////////////////////////////////////////////
	void Trench::Step(uint32_t theNow, const TrenchContext& theCtx)
	{
		float dt = std::min(0.1f, (theNow - mLastStep) / 1000.0f);
		mLastStep = theNow;
		mHeroes = theCtx.mHeroes;
		for (size_t i = 0; i < mMinions.size();)
			if (mMinions[i].mHp <= 0)
				mMinions.erase(mMinions.begin() + i);
			else
				i++;

		// Monsters appear on their timers.
		for (int s = 0; s < MON_COUNT; s++)
		{
			if (mMonsterId[s] != 0 && Monster(s) == nullptr)
				mMonsterId[s] = 0;				// (died in a hit between steps: MinionDied set the timer)
			if (mMonsterId[s] != 0 || !Elapsed(theNow, mMonsterAt[s]) || mMonsterAt[s] == 0)
				continue;
			const MinionDef& d = MinionDefOf(kMonsterKind[s]);
			Minion n;
			n.mId = mNextId++;
			n.mKind = kMonsterKind[s];
			n.mTeam = kNeutralTeam;
			n.mSlot = (int8_t)s;
			n.mHome = MonsterHome(s);
			n.mPos = n.mHome;
			n.mHp = n.mMaxHp = d.mHealth;
			n.mNextThink = theNow;
			n.mRight = s == MON_BALROG;
			mMinions.push_back(n);
			mMonsterId[s] = n.mId;
			mMonsterAt[s] = 0;
			Event e;
			e.mType = EV_MONSTER;
			e.mArena = kTrench;
			e.mParam = (uint8_t)s;
			e.mA = n.mPos;
			mOutEvents.push_back(e);
			if (s == MON_SQUID || s == MON_BOSS)
			{
				Event a;
				a.mType = EV_ANNOUNCE;
				a.mArena = kTrench;
				a.mParam = s == MON_SQUID ? AN_SQUID_UP : AN_BOSS_UP;
				mOutEvents.push_back(a);
				Sound(s == MON_BOSS ? SND_SCREAM : SND_ROAR, n.mPos);
			}
			Burst(n.mPos, d.mRadius * 1.6f, s == MON_BOSS ? LOOK_BOSS : LOOK_TELEPORT);
		}

		StepMonsters(theNow, dt, theCtx);
		StepLane(theNow, dt, theCtx);
		StepBolts(theNow, dt, theCtx);
		StepCoins(theNow, dt, theCtx);
	}

	// Where a lane minion heads when it has nothing to fight: along the lane toward the
	// far gate, at its own height in the pack.
	Vec Trench::LaneTarget(const Minion& m, int theTeam) const
	{
		const MapDef& aMap = TrenchMap();
		float aDir = theTeam == 0 ? 1.0f : -1.0f;
		float x = Clamp(m.mPos.x + aDir * 80, aMap.mTrenchLane.front().x, aMap.mTrenchLane.back().x);
		const std::vector<Vec>& l = aMap.mTrenchLane;
		float y = l.back().y;
		for (size_t i = 1; i < l.size(); i++)
			if (x <= l[i].x)
			{
				float t = (x - l[i - 1].x) / std::max(1.0f, l[i].x - l[i - 1].x);
				y = l[i - 1].y + (l[i].y - l[i - 1].y) * t;
				break;
			}
		float aSpread = (float)((int)(m.mId * 37 % 7) - 3) * 14;	// keep a pack from stacking
		Vec aGoal(x, y + aSpread);
		// The last stretch: straight into the gate.
		Vec aGate = aMap.mGate[theTeam == 0 ? 1 : 0];
		if (std::fabs(m.mPos.x - aGate.x) < 160)
			aGoal = aGate;
		return aGoal;
	}

	void Trench::StepLane(uint32_t theNow, float theDt, const TrenchContext& theCtx)
	{
		const MapDef& aMap = TrenchMap();
		for (size_t i = 0; i < mMinions.size(); i++)
		{
			Minion& m = mMinions[i];
			if (m.mHp <= 0 || m.mTeam == kNeutralTeam)
				continue;
			const MinionDef& d = MinionDefOf(m.mKind);
			int aTeam = m.Team(theNow);
			bool aCharmed = aTeam != m.mTeam;

			if (Elapsed(theNow, m.mNextThink))
			{
				m.mNextThink = theNow + 350;
				EntityRef aTarget;
				float aBest = 1e9f;
				for (const Minion& o : mMinions)
				{
					if (&o == &m || o.mHp <= 0 || o.mTeam == kNeutralTeam || o.Team(theNow) == aTeam)
						continue;
					float dd = Dist(o.mPos, m.mPos);
					if (dd < kLaneFightR && dd < aBest)
					{
						aBest = dd;
						aTarget = EntityRef::Of(kTrench, ENT_MINION, o.mId);
					}
				}
				// Heroes: minions fight minions first (a MOBA's rule), but turn on a hero that
				// just hurt their own hero nearby, and on a taunting one.
				for (const HeroPresence& h : theCtx.mHeroes)
				{
					if (!h.mTargetable || h.mTeam == aTeam)
						continue;
					float dd = Dist(h.mPos, m.mPos);
					bool aAggressor = h.mHurtHeroAt != 0 && (int32_t)(theNow - h.mHurtHeroAt) < 2000 && dd < 260;
					float aLimit = h.mTaunting ? 320.0f : kMinionAggroR;
					if (h.mTaunting || aAggressor)
						dd -= 1000;
					else if (aTarget.Valid())
						continue;
					if (Dist(h.mPos, m.mPos) < std::max(aLimit, aAggressor ? 260.0f : 0.0f) && dd < aBest)
					{
						aBest = dd;
						aTarget = EntityRef::Hero(h.mPlayer);
					}
				}
				m.mTarget = aTarget;
			}
			if (m.Disabled(theNow))
				continue;

			// Where to go.
			Vec aGoal;
			float aReach = d.mRange + d.mRadius;
			bool aFight = false;
			if (m.mTarget.mKind == ENT_HERO)
			{
				bool aFound = false;
				for (const HeroPresence& h : theCtx.mHeroes)
					if (h.mPlayer == (int)m.mTarget.mId && h.mTargetable)
					{
						aGoal = h.mPos;
						aReach += h.mRadius;
						aFound = true;
					}
				if (!aFound)
					m.mTarget = EntityRef();
				aFight = aFound;
			}
			else if (m.mTarget.mKind == ENT_MINION)
			{
				if (Minion* o = FindMinion(m.mTarget.mId))
				{
					aGoal = o->mPos;
					aReach += MinionDefOf(o->mKind).mRadius;
					aFight = true;
				}
				else
					m.mTarget = EntityRef();
			}
			if (!aFight)
			{
				aGoal = LaneTarget(m, aTeam);
				aReach = 4;
			}
			float aSpeed = d.mSpeed * (Elapsed(theNow, m.mSlowUntil) ? 1.0f : (1.0f - m.mSlowPct));
			float aDist = Dist(aGoal, m.mPos);
			if (aDist > aReach)
			{
				Vec aOld = m.mPos;
				StepToward(m.mPos, aGoal, aSpeed * theDt);
				m.mPos = ClampToWater(kTrench, m.mPos, d.mRadius * 0.6f);
				if (std::fabs(m.mPos.x - aOld.x) > 0.05f)
					m.mRight = m.mPos.x > aOld.x;
			}
			else if (aFight && Elapsed(theNow, m.mNextHit))
			{
				m.mNextHit = theNow + (uint32_t)(d.mHitS * 1000);
				m.mAttackingUntil = theNow + 300;
				m.mRight = aGoal.x > m.mPos.x;
				Hit h;
				h.mTeam = (uint8_t)aTeam;
				h.mSource = SRC_MINION;
				h.mPlayer = aCharmed ? m.mCharmPlayer : (int8_t)-1;
				h.mDamage = d.mHeroHit * m.HitMult(theNow);
				if (m.mKind == MIN_DESTRUCTOR)
				{
					Bolt b;
					b.mPos = m.mPos;
					b.mTarget = m.mTarget;
					b.mTargetPos = aGoal;
					b.mDamage = h.mDamage;
					b.mTeam = (uint8_t)aTeam;
					b.mSource = SRC_MINION;
					mBolts.push_back(b);
					Event e;
					e.mType = EV_PROJECTILE;
					e.mArena = kTrench;
					e.mA = m.mPos;
					e.mB = Norm(aGoal - m.mPos) * 420;
					e.mMs = (uint16_t)std::min(3000.0f, aDist / 420 * 1000);
					e.mParam = LOOK_DESTRUCTOR;
					mOutEvents.push_back(e);
				}
				else if (m.mTarget.mKind == ENT_HERO)
					HitHero((int)m.mTarget.mId, h);
				else if (Minion* o = FindMinion(m.mTarget.mId))
					HurtMinion(*o, h, theNow);
			}

			// Through the far gate: on into the rival's tank, health and all.
			if (!aCharmed && m.mHp > 0)
			{
				int aTo = m.mTeam == 0 ? 1 : 0;
				if (Dist(m.mPos, aMap.mGate[aTo]) < aMap.mGateR)
				{
					Arrival a;
					a.mKind = m.mKind;
					a.mMult = m.mMult;
					a.mHpFrac = m.mHp / std::max(1.0f, m.mMaxHp);
					mOutArrivals[aTo].push_back(a);
					m.mHp = 0;						// gone from the Trench (no reward)
				}
			}
		}
	}

	void Trench::StepMonsters(uint32_t theNow, float theDt, const TrenchContext& theCtx)
	{
		for (Minion& m : mMinions)
		{
			if (m.mHp <= 0 || m.mTeam != kNeutralTeam)
				continue;
			const MinionDef& d = MinionDefOf(m.mKind);
			// Whom it's after: the hero that hit it last, while that hero stays near home.
			const HeroPresence* aFoe = nullptr;
			if (m.mAggroPlayer >= 0 && !Elapsed(theNow, m.mAggroUntil))
				for (const HeroPresence& h : theCtx.mHeroes)
					if (h.mPlayer == m.mAggroPlayer && h.mTargetable && Dist(h.mPos, m.mHome) < kLeashR + 200)
						aFoe = &h;
			if (aFoe == nullptr && m.mAggroPlayer >= 0)
			{
				m.mAggroPlayer = -1;
				m.mHoming = true;
			}
			if (Dist(m.mPos, m.mHome) > kLeashR + 60)
			{
				m.mAggroPlayer = -1;
				m.mHoming = true;
				aFoe = nullptr;
			}
			if (m.Disabled(theNow))
				continue;
			float aSpeed = d.mSpeed * (Elapsed(theNow, m.mSlowUntil) ? 1.0f : (1.0f - m.mSlowPct));
			if (aFoe == nullptr)
			{
				// Home, healing on the way; a lazy bob once there.
				Vec aBob = m.mHome + Vec(std::sin(theNow / 1300.0f + m.mId) * 14, std::sin(theNow / 900.0f + m.mId * 2) * 8);
				Vec aOld = m.mPos;
				StepToward(m.mPos, m.mHoming ? m.mHome : aBob, aSpeed * (m.mHoming ? 1.6f : 0.25f) * theDt);
				if (std::fabs(m.mPos.x - aOld.x) > 0.05f)
					m.mRight = m.mPos.x > aOld.x;
				if (m.mHoming)
				{
					m.mHp = std::min(m.mMaxHp, m.mHp + m.mMaxHp * 0.15f * theDt);
					if (Dist(m.mPos, m.mHome) < 6)
					{
						m.mHoming = false;
						m.mHp = m.mMaxHp;
					}
				}
				continue;
			}
			// Fight.
			float aReach = d.mRange + d.mRadius + aFoe->mRadius;
			if (Dist(aFoe->mPos, m.mPos) > aReach)
			{
				Vec aOld = m.mPos;
				StepToward(m.mPos, aFoe->mPos, aSpeed * theDt);
				m.mPos = ClampToWater(kTrench, m.mPos, d.mRadius * 0.6f);
				if (std::fabs(m.mPos.x - aOld.x) > 0.05f)
					m.mRight = m.mPos.x > aOld.x;
			}
			else if (Elapsed(theNow, m.mNextHit))
			{
				m.mNextHit = theNow + (uint32_t)(d.mHitS * 1000);
				m.mAttackingUntil = theNow + 350;
				m.mRight = aFoe->mPos.x > m.mPos.x;
				Hit h;
				h.mTeam = kNeutralTeam;
				h.mSource = SRC_MONSTER;
				h.mDamage = d.mHeroHit;
				HitHero(aFoe->mPlayer, h);
				Sound(m.mKind == MIN_BOSS ? SND_CHOMP : SND_HIT, aFoe->mPos);
			}
			// Their specials, while they're fighting.
			if (m.mNextSpecial == 0)
				m.mNextSpecial = theNow + 2500;
			if ((m.mKind == MIN_PSYCHO || m.mKind == MIN_BOSS) && Elapsed(theNow, m.mNextSpecial))
			{
				bool aSquid = m.mKind == MIN_PSYCHO;
				m.mNextSpecial = theNow + (uint32_t)((aSquid ? kSquidInkEveryS : kBossSlamEveryS) * 1000);
				float r = aSquid ? kSquidInkR : kBossSlamR;
				for (const HeroPresence& h : theCtx.mHeroes)
				{
					if (!h.mTargetable || Dist(h.mPos, m.mPos) > r + h.mRadius)
						continue;
					Hit hit;
					hit.mTeam = kNeutralTeam;
					hit.mSource = SRC_MONSTER;
					if (aSquid)
					{
						hit.mDamage = kSquidInkDamage;
						hit.mSlowMs = 2000;
						hit.mSlowPct = 0.4f;
					}
					else
					{
						hit.mDamage = kBossSlamDamage;
						hit.mStunMs = (uint16_t)(kBossSlamStunS * 1000);
						hit.mPush = Norm(h.mPos - m.mPos) * 80;
					}
					HitHero(h.mPlayer, hit);
				}
				if (aSquid)
				{
					Event e;
					e.mType = EV_ZONE;
					e.mArena = kTrench;
					e.mA = m.mPos;
					e.mValue = r;
					e.mMs = 900;
					e.mParam = LOOK_INK;
					mOutEvents.push_back(e);
					Sound(SND_BIG_SPLASH, m.mPos);
				}
				else
				{
					Burst(m.mPos, r, LOOK_SLAM);
					Sound(SND_BOOM, m.mPos);
				}
			}
		}
	}

	void Trench::StepBolts(uint32_t theNow, float theDt, const TrenchContext& theCtx)
	{
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
			else if (Minion* m = FindMinion(b.mTarget.mId))
				b.mTargetPos = m->mPos;
			else
				aGone = true;
			if (aGone)
			{
				mBolts.erase(mBolts.begin() + i);
				continue;
			}
			if (StepToward(b.mPos, b.mTargetPos, 420 * theDt) || Dist(b.mPos, b.mTargetPos) < 16)
			{
				Hit h;
				h.mDamage = b.mDamage;
				h.mTeam = b.mTeam;
				h.mSource = b.mSource;
				if (b.mTarget.mKind == ENT_HERO)
					HitHero((int)b.mTarget.mId, h);
				else if (Minion* m = FindMinion(b.mTarget.mId))
					HurtMinion(*m, h, theNow);
				mBolts.erase(mBolts.begin() + i);
				continue;
			}
			i++;
		}
	}

	void Trench::StepCoins(uint32_t theNow, float theDt, const TrenchContext& theCtx)
	{
		for (size_t i = 0; i < mCoins.size();)
		{
			LaneCoin& c = mCoins[i];
			// Speedy's Treasure Trail pulls coins in from afar.
			const HeroPresence* aMagnet = nullptr;
			for (const HeroPresence& h : theCtx.mHeroes)
				if (h.mAlive && h.mMagnet && Dist(h.mPos, c.mPos) < 700)
					aMagnet = &h;
			if (aMagnet != nullptr)
				StepToward(c.mPos, aMagnet->mPos, 650 * theDt);
			else
			{
				float aFloor = FloorY(kTrench, c.mPos.x) - 12;
				if (c.mPos.y < aFloor)
				{
					c.mPos.y = std::min(aFloor, c.mPos.y + kCoinFallSpeed * 1.4f * theDt);
					c.mPos.x = Clamp(c.mPos.x + c.mDrift * theDt, 20, kWorldW - 20);
					c.mDrift *= 1.0f - std::min(1.0f, 1.5f * theDt);
				}
				else if (c.mLandedAt == 0)
					c.mLandedAt = theNow;
			}
			// Whoever touches it first. Ties go to the nearer hero.
			const HeroPresence* aWho = nullptr;
			float aBest = 1e9f;
			for (const HeroPresence& h : theCtx.mHeroes)
			{
				if (!h.mAlive)
					continue;
				float dd = Dist(h.mPos, c.mPos);
				if (dd < kLaneCoinReach + h.mRadius * 0.5f && dd < aBest)
				{
					aBest = dd;
					aWho = &h;
				}
			}
			if (aWho != nullptr)
			{
				Reward r;
				r.mPlayer = (int8_t)aWho->mPlayer;
				r.mTeam = (int8_t)aWho->mTeam;
				r.mMoney = c.mValue;
				r.mWhat = ENT_COIN;
				mOutRewards.push_back(r);
				Sound(c.mValue >= 70 ? SND_DIAMOND : SND_COIN, c.mPos);
				mCoins.erase(mCoins.begin() + i);
				continue;
			}
			if (c.mLandedAt != 0 && Elapsed(theNow, c.mLandedAt + (uint32_t)(kLaneCoinFloorS * 1000)))
			{
				mCoins.erase(mCoins.begin() + i);
				continue;
			}
			i++;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Hits and deaths
	///////////////////////////////////////////////////////////////////////////
	void Trench::ApplyHit(const EntityRef& theTarget, const Hit& theHit, uint32_t theNow)
	{
		if (theTarget.mKind != ENT_MINION)
			return;
		if (Minion* m = FindMinion(theTarget.mId))
			HurtMinion(*m, theHit, theNow);
	}

	void Trench::HurtMinion(Minion& m, const Hit& theHit, uint32_t theNow)
	{
		if (m.mTeam == kNeutralTeam && theHit.mTeam == kNeutralTeam)
			return;
		if (HurtMinionCommon(m, theHit, theNow, kTrench).mDied)
			MinionDied(m, theHit, theNow);
	}

	void Trench::DropCoins(int theValue, Vec thePos, int theCount)
	{
		theCount = std::max(1, theCount);
		int aLeft = theValue;
		for (int i = 0; i < theCount; i++)
		{
			int v = i == theCount - 1 ? aLeft : theValue / theCount;
			aLeft -= v;
			LaneCoin c;
			c.mId = mNextId++;
			c.mValue = v;
			c.mPos = ClampToWater(kTrench, thePos + Vec(mRng.Range(-20, 20), mRng.Range(-14, 10)), 12);
			c.mDrift = theCount > 1 ? mRng.Range(-110, 110) : mRng.Range(-20, 20);
			mCoins.push_back(c);
			mLaneCoinsDropped += v;
		}
	}

	void Trench::MinionDied(Minion& m, const Hit& theHit, uint32_t theNow)
	{
		const MinionDef& d = MinionDefOf(m.mKind);
		m.mHp = 0;
		if (m.mTeam == kNeutralTeam)
		{
			// A monster: its prize to the last hero to hit it, coins for anyone near.
			int s = std::clamp<int>(m.mSlot, 0, MON_COUNT - 1);
			int8_t aKiller = theHit.mPlayer >= 0 ? theHit.mPlayer : m.mLastHitBy;
			int aKillerTeam = -1;
			for (const HeroPresence& h : mHeroes)
				if (h.mPlayer == aKiller)
					aKillerTeam = h.mTeam;
			if (aKillerTeam < 0 && theHit.mTeam < kTeams)
				aKillerTeam = theHit.mTeam;
			if (aKiller >= 0)
			{
				Reward r;
				r.mPlayer = aKiller;
				r.mTeam = (int8_t)aKillerTeam;
				r.mXp = d.mXp;
				r.mWhat = ENT_MINION;
				r.mBuff = kMonsterBuff[s];
				mOutRewards.push_back(r);
			}
			DropCoins(d.mCoins, m.mPos, s >= MON_SQUID ? 6 : 3);
			mMonsterId[s] = 0;
			mMonsterAt[s] = theNow + (uint32_t)(kMonsterRespawnS[s] * 1000);
			Event e;
			e.mType = EV_MONSTER;
			e.mArena = kTrench;
			e.mParam = (uint8_t)s;
			e.mA = m.mPos;
			e.mValue = 1;
			mOutEvents.push_back(e);
			Event a;
			a.mType = EV_ANNOUNCE;
			a.mArena = kTrench;
			a.mParam = s == MON_SQUID ? AN_SQUID : (s == MON_BOSS ? AN_BOSS : AN_CAMP);
			a.mPlayer = aKiller;
			a.mId = (uint32_t)s;
			mOutEvents.push_back(a);
			Burst(m.mPos, d.mRadius * 1.5f, LOOK_NONE);
			Sound(SND_EXPLODE, m.mPos);
			return;
		}
		// A lane minion: coins where it fell, XP to the enemy heroes near (and the killer).
		DropCoins(d.mCoins, m.mPos, d.mCoins >= 60 ? 3 : 1);
		float aXp = m.mKind == MIN_MINI ? kXpMinion : kXpBigAlien;
		bool aKillerPaid = false;
		for (const HeroPresence& h : mHeroes)
		{
			if (h.mTeam == m.mTeam || !h.mAlive)
				continue;
			bool aKiller = h.mPlayer == theHit.mPlayer;
			if (!aKiller && Dist(h.mPos, m.mPos) > kSharedXpR)
				continue;
			Reward r;
			r.mPlayer = (int8_t)h.mPlayer;
			r.mTeam = (int8_t)h.mTeam;
			r.mXp = aXp;
			r.mWhat = aKiller ? ENT_MINION : ENT_NONE;
			mOutRewards.push_back(r);
			aKillerPaid = aKillerPaid || aKiller;
		}
		if (!aKillerPaid && theHit.mPlayer >= 0)
		{
			Reward r;
			r.mPlayer = theHit.mPlayer;
			r.mTeam = (int8_t)theHit.mTeam;
			r.mXp = aXp;
			r.mWhat = ENT_MINION;
			mOutRewards.push_back(r);
		}
		Burst(m.mPos, d.mRadius, LOOK_NONE);
		Sound(SND_EXPLODE, m.mPos);
	}

	ArenaSnap Trench::Snapshot() const
	{
		ArenaSnap s;
		s.mArena = kTrench;
		s.mTeam = 0;
		for (int i = 0; i < MON_COUNT; i++)
			s.mMonsterIn[i] = (uint16_t)std::min<uint32_t>(65000, (MonsterIn(i, mLastStep) + 999) / 1000);
		for (const LaneCoin& c : mCoins)
			s.mCoins.push_back({ c.mId, (uint8_t)(c.mValue >= 70 ? COIN_DIAMOND : (c.mValue >= 25 ? COIN_GOLD : COIN_SILVER)), c.mPos });
		for (const Minion& m : mMinions)
		{
			if (m.mHp <= 0)
				continue;
			uint8_t aFlags = (m.mRight ? MF_RIGHT : 0) | (!Elapsed(mLastStep, m.mCharmUntil) ? MF_CHARMED : 0)
				| (!Elapsed(mLastStep, m.mStunUntil) ? MF_STUNNED : 0) | (!Elapsed(mLastStep, m.mAttackingUntil) ? MF_ATTACKING : 0)
				| (!Elapsed(mLastStep, m.mSleepUntil) ? MF_ASLEEP : 0) | (!Elapsed(mLastStep, m.mRallyUntil) ? MF_RALLIED : 0)
				| (m.mAggroPlayer >= 0 ? MF_ANGRY : 0) | (m.mHoming ? MF_HOMING : 0);
			s.mMinions.push_back({ m.mId, m.mKind, aFlags, (uint8_t)m.Team(mLastStep), m.mHp / std::max(1.0f, m.mMaxHp), m.mPos });
		}
		return s;
	}
}
