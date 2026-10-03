// Pet Heroes - the hero: orders, movement and pathing, crossings between the three
// tanks, kelp, auto-attacks, projectiles, zones, landing impacts, turrets and mines,
// burns. Part of Side (HeroesSide.h); the abilities are in HeroesAbilities.cpp.

#include "HeroesSide.h"
#include "HeroesMap.h"

namespace Heroes
{
	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }

	///////////////////////////////////////////////////////////////////////////
	// Orders
	///////////////////////////////////////////////////////////////////////////
	// A crossing near theWhere in that arena (a tank's portal, a Trench gate), or -1.
	static int GateNear(int theArena, Vec theWhere, float theSlack, Vec& theGate)
	{
		const MapDef& m = MapOf(theArena);
		if (IsTank(theArena))
		{
			if (Dist(theWhere, m.mPortal) < m.mPortalR + theSlack)
			{
				theGate = m.mPortal;
				return 0;
			}
			return -1;
		}
		for (int g = 0; g < 2; g++)
			if (Dist(theWhere, m.mGate[g]) < m.mGateR + theSlack)
			{
				theGate = m.mGate[g];
				return g;
			}
		return -1;
	}

	void Side::OrderMove(Vec theWhere)
	{
		HeroState& h = mHero;
		if (!h.mAlive)
			return;
		h.mOrder = HeroState::ORD_MOVE;
		Vec aGate;
		h.mWantCross = GateNear(h.mArena, theWhere, 24, aGate) >= 0;
		if (h.mWantCross)
			theWhere = aGate;
		h.mMoveTo = h.Def().mWalker ? WalkerPos(h.mArena, theWhere.x, h.Def().mRadius) : NearestOpen(h.mArena, theWhere);
		h.mTarget = EntityRef();
		h.mPath.clear();
		h.mRepathAt = 0;
	}

	void Side::OrderAttack(const EntityRef& theTarget)
	{
		HeroState& h = mHero;
		if (!h.mAlive || !theTarget.Valid())
			return;
		Target t;
		if (!FindTarget(theTarget, t) || t.mTeam == mTeam)
			return;
		h.mOrder = HeroState::ORD_ATTACK;
		h.mTarget = theTarget;
		h.mAutoTarget = theTarget;
		h.mWantCross = false;
		h.mPath.clear();
		h.mRepathAt = 0;
	}

	void Side::OrderStop()
	{
		mHero.mWantCross = false;
		mHero.mOrder = HeroState::ORD_IDLE;
		mHero.mPath.clear();
		mHero.mTarget = EntityRef();
	}

	void Side::Steer(Vec theDir)
	{
		float l = Len(theDir);
		mSteer = l > 0.001f ? theDir * (1.0f / l) : Vec();
		if (l > 0.001f && mHero.mOrder != HeroState::ORD_IDLE)
		{
			// The keys take over from a right-click order; what you were attacking stays
			// your auto-attack's target (kite a camp with WASD and keep hitting it).
			if (mHero.mOrder == HeroState::ORD_ATTACK)
				mHero.mAutoTarget = mHero.mTarget;
			OrderStop();
		}
	}

	bool Side::Hop()
	{
		HeroState& h = mHero;
		if (!h.mAlive || !h.Def().mWalker || Stunned() || Busy() || Hopping() || Rooted())
			return false;
		if (!Elapsed(mNow, h.mHopReadyAt))
		{
			mLastRefusal = "Hop recharging.";
			return false;
		}
		h.mHopStart = mNow;
		h.mHopUntil = mNow + kHopMs;
		h.mHopReadyAt = mNow + kHopCooldownMs;
		return true;
	}

	bool Side::CrossNearby()
	{
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		mLastRefusal.clear();
		if (!h.mAlive || !d.mWalker)
			return false;
		// The nearest crossing along the floor: a portal's beam, a pad, a Trench gate.
		const MapDef& m = MapOf(h.mArena);
		Vec aBest;
		float aBestD = 1e9f;
		auto Try = [&](Vec p, float theSlack) {
			float dx = std::fabs(h.mPos.x - p.x) - theSlack;
			if (dx < aBestD)
			{
				aBestD = dx;
				aBest = p;
			}
		};
		if (IsTank(h.mArena))
		{
			Try(m.mPortal, 0);
			for (int i = 0; i < 2; i++)
				Try(m.mPad[i], m.mPadR);
		}
		else
			for (int g = 0; g < 2; g++)
				Try(m.mGate[g], m.mGateR);
		if (aBestD > kCrossAssistR)
		{
			mLastRefusal = IsTank(h.mArena) ? "S crosses near the portal's beam or a floor pad." : "S crosses near a gate at either end.";
			return false;
		}
		if (WarpSicknessLeft() > 0)
		{
			mLastRefusal = "You can't leave yet: " + std::to_string((WarpSicknessLeft() + 999) / 1000) + " s.";
			return false;
		}
		OrderMove(aBest);
		return true;
	}

	static bool InReach(const HeroState& h, const Target& t, float theRange)
	{
		const HeroDef& d = h.Def();
		float aReach = theRange + d.mRadius + t.mRadius;
		if (!d.mWalker)
			return Dist(h.mPos, t.mPos) <= aReach;
		float dy = h.mPos.y - t.mPos.y;			// > 0: target above
		return std::fabs(h.mPos.x - t.mPos.x) <= aReach && dy <= kWalkerReachUp + t.mRadius && dy >= -(60 + t.mRadius);
	}

	///////////////////////////////////////////////////////////////////////////
	// Step
	///////////////////////////////////////////////////////////////////////////
	Vec Side::ExitPoint(int theArena, int theGate) const
	{
		const HeroDef& d = mHero.Def();
		const MapDef& m = MapOf(theArena);
		Vec p = IsTank(theArena) ? m.mPortalExit : m.mGateExit[std::clamp(theGate, 0, 1)];
		return d.mWalker ? WalkerPos(theArena, p.x, d.mRadius) : p;
	}

	void Side::StepHero(float theDt)
	{
		HeroState& h = mHero;
		if (!h.mAlive)
		{
			if (Elapsed(mNow, h.mRespawnAt))
				Respawn();
			return;
		}
		if (h.mCopyHero >= 0 && Elapsed(mNow, h.mCopyUntil))
			EndCopy();
		const HeroDef& d = h.Def();
		float aMax = MaxHp();
		// The fountain: heal quickly next to your own core.
		if (h.mArena == mTeam && Dist(h.mPos, TankMap().mCore) < kFountainRadius)
			h.mHp = std::min(aMax, h.mHp + aMax * kFountainHealPct * theDt);
		else
			h.mHp = std::min(aMax, h.mHp + aMax * (HasBuff(BUFF_GUS) ? kGusRegenPct : 0.004f) * theDt);	// a trickle anywhere
		h.mHp = std::min(h.mHp, aMax);
		if (h.mShield > 0 && Elapsed(mNow, h.mShieldUntil))
			h.mShield = 0;
		if (h.mSharpenLeft > 0 && Elapsed(mNow, h.mSharpenUntil))
			h.mSharpenLeft = 0;

		Vec aStart = h.mPos;
		StepAbilities(theDt);
		if (!h.mAlive)
			return;
		if (!Stunned() && !Busy())
		{
			StepAttack();
			StepMovement(theDt);
			StepSteer(theDt);
		}
		// A walker's hop: up and down over the floor (Rhubarb's Leap has its own arc).
		if (d.mWalker && h.mLeapUntil == 0)
		{
			float aUp = 0;
			if (Hopping())
			{
				float t = Clamp((float)(int32_t)(mNow - h.mHopStart) / kHopMs, 0, 1);
				aUp = std::sin(t * 3.14159f) * kHopHeight;
			}
			h.mPos = WalkerPos(h.mArena, h.mPos.x, d.mRadius) - Vec(0, aUp);
		}
		if (Dist(aStart, h.mPos) > 0.5f)
			h.mLastMoveAt = mNow;

		// Crossings. In a tank: the warp hole (top middle) leads into the Trench, at this
		// tank's end; the floor pads (bottom corners) lead straight to the other tank. In
		// the Trench: each end's gate leads to that team's tank.
		const MapDef& m = MapOf(h.mArena);
		int aTo = -1;
		Vec aExit;
		if (IsTank(h.mArena))
		{
			bool aPortal = Dist(h.mPos, m.mPortal) < m.mPortalR
				|| (d.mWalker && h.mWantCross && std::fabs(h.mPos.x - m.mPortal.x) < 14);	// walkers ride the beam
			if (aPortal)
			{
				aTo = kTrench;
				aExit = ExitPoint(kTrench, h.mArena);
			}
			for (int i = 0; i < 2; i++)
				if (Dist(h.mPos, m.mPad[i]) < m.mPadR + d.mRadius)
				{
					aTo = 1 - h.mArena;
					float x = m.mPad[i].x + (i == 0 ? 110.0f : -110.0f);
					aExit = d.mWalker ? WalkerPos(aTo, x, d.mRadius) : ClampToWater(aTo, Vec(x, m.mPad[i].y - 90), d.mRadius);
				}
		}
		else
			for (int g = 0; g < 2; g++)
				if (Dist(h.mPos, m.mGate[g]) < m.mGateR)
				{
					aTo = g;
					aExit = ExitPoint(g, 0);
				}
		if (aTo < 0)
			h.mInPortal = false;
		else if (!h.mInPortal && !Stunned() && !Busy() && !Rooted() && WarpSicknessLeft() == 0)
			CrossTo(aTo, aExit);

		// Kelp hides you unless an enemy hero is close or you just attacked.
		bool aKelp = KelpAt(h.mArena, h.mPos) >= 0;
		HeroSnap o;
		bool aSpotted = OtherHeroIn(h.mArena, &o) && (o.mFlags & HF_ALIVE) && Dist(o.mPos, h.mPos) < kRevealR;
		h.mHidden = aKelp && !aSpotted && Elapsed(mNow, h.mRevealUntil);
	}

	void Side::CrossTo(int theArena, Vec thePos)
	{
		HeroState& h = mHero;
		h.mArena = theArena;
		h.mPos = thePos;
		h.mInPortal = true;
		h.mWantCross = false;
		// A raid is a commitment: no leaving the rival's tank for a while (D24/D36).
		bool aRaid = IsTank(theArena) && theArena != mTeam;
		h.mCrossReadyAt = mNow + (uint32_t)((aRaid ? kRaidLockS : kCrossGuardS) * 1000);
		h.mOrder = HeroState::ORD_IDLE;
		h.mPath.clear();
		h.mTarget = EntityRef();
		h.mAutoTarget = EntityRef();
		h.mStormUntil = 0;
		h.mSlimeUntil = 0;
		h.mHealUntil = 0;
		Event e;
		e.mType = EV_CROSS;
		e.mArena = (uint8_t)theArena;
		e.mPlayer = (int8_t)mPlayer;
		e.mA = thePos;
		Emit(e);
		Sound(theArena, SND_WARP, thePos);
	}

	void Side::StepMovement(float theDt)
	{
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		Vec aGoal;
		if (h.mOrder == HeroState::ORD_MOVE)
			aGoal = h.mMoveTo;
		else if (h.mOrder == HeroState::ORD_ATTACK)
		{
			Target t;
			if (!FindTarget(h.mTarget, t))
			{
				h.mOrder = HeroState::ORD_IDLE;
				return;
			}
			bool aClear = d.mWalker || d.mProjectile <= 0 || LineOfSight(h.mArena, h.mPos, t.mPos);
			if (InReach(h, t, d.mRange) && aClear)
				return;							// StepAttack does the rest
			aGoal = t.mPos;
		}
		else
			return;

		float aStep = Speed() * theDt;
		if (aStep <= 0)
			return;
		Vec aOld = h.mPos;
		if (d.mWalker)
		{
			float x = h.mPos.x;
			float dx = aGoal.x - x;
			if (std::fabs(dx) <= aStep)
			{
				x = aGoal.x;
				if (h.mOrder == HeroState::ORD_MOVE)
					h.mOrder = HeroState::ORD_IDLE;
			}
			else
				x += dx > 0 ? aStep : -aStep;
			h.mPos = WalkerPos(h.mArena, x, d.mRadius);
		}
		else
		{
			if (h.mPath.empty() || Elapsed(mNow, h.mRepathAt))
			{
				FindPath(h.mArena, h.mPos, aGoal, h.mPath);
				h.mRepathAt = mNow + (h.mOrder == HeroState::ORD_ATTACK ? 350 : 100000);
			}
			while (aStep > 0 && !h.mPath.empty())
			{
				float l = Dist(h.mPos, h.mPath.front());
				if (l <= aStep)
				{
					h.mPos = h.mPath.front();
					aStep -= l;
					h.mPath.erase(h.mPath.begin());
				}
				else
				{
					StepToward(h.mPos, h.mPath.front(), aStep);
					aStep = 0;
				}
			}
			if (h.mPath.empty() && h.mOrder == HeroState::ORD_MOVE)
				h.mOrder = HeroState::ORD_IDLE;
			h.mPos = ClampToWater(h.mArena, h.mPos, d.mRadius);
		}
		if (std::fabs(h.mPos.x - aOld.x) > 0.05f)
			h.mRight = h.mPos.x > aOld.x;
	}

	void Side::StepSteer(float theDt)
	{
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		if (h.mOrder != HeroState::ORD_IDLE || (mSteer.x == 0 && mSteer.y == 0))
			return;
		float aStep = Speed() * theDt;
		if (aStep <= 0)
			return;
		Vec aOld = h.mPos;
		int a = h.mArena;
		if (d.mWalker)
		{
			if (mSteer.x != 0)
				h.mPos = WalkerPos(a, Clamp(h.mPos.x + (mSteer.x > 0 ? aStep : -aStep), d.mRadius, kWorldW - d.mRadius), d.mRadius);
		}
		else
		{
			// Swim. Against a wall, slide along it: first the part of the move that fits,
			// then turned 45 and 90 degrees, keeping to one side until clear (never get stuck
			// inside one either).
			bool aFree = !SwimmerFits(a, h.mPos, d.mRadius);
			auto TryMove = [&](Vec theMove) {
				Vec q = ClampToWater(a, h.mPos + theMove, d.mRadius);
				if (Dist(q, h.mPos) < 0.01f || (!aFree && !SwimmerFits(a, q, d.mRadius)))
					return false;
				h.mPos = q;
				return true;
			};
			Vec v = mSteer * aStep;
			bool aStraight = TryMove(v);
			bool aMoved = aStraight || TryMove(Vec(v.x, 0)) || TryMove(Vec(0, v.y));
			bool aAtEdge = Dist(ClampToWater(a, aOld + v, d.mRadius), aOld + v) > 0.01f;	// turning gets around walls, not along the tank's edges
			if (!aStraight && !aAtEdge && Elapsed(mNow, mLastBumpAt + 400))
			{
				// Ran into a wall: a sand puff on my own screen (never sent).
				mLastBumpAt = mNow;
				Event e;
				e.mType = EV_BUMP;
				e.mArena = (uint8_t)a;
				e.mId = mEffectSeq + 1;
				e.mA = aOld + mSteer * (d.mRadius / std::max(0.01f, Len(mSteer)));
				Local(e);
			}
			for (int aTurn = 0; !aMoved && !aAtEdge && aTurn < 4; aTurn++)
			{
				float aSide = aTurn < 2 ? mSlideSide : -mSlideSide;
				float c = aTurn % 2 == 0 ? 0.7071f : 0.0f, n = aTurn % 2 == 0 ? 0.7071f : 1.0f;
				Vec aDir(mSteer.x * c - mSteer.y * n * aSide, mSteer.y * c + mSteer.x * n * aSide);
				if (TryMove(aDir * aStep))
				{
					aMoved = true;
					mSlideSide = aSide;
				}
			}
		}
		if (std::fabs(h.mPos.x - aOld.x) > 0.05f)
			h.mRight = h.mPos.x > aOld.x;
	}

	///////////////////////////////////////////////////////////////////////////
	// Attacks
	///////////////////////////////////////////////////////////////////////////
	// Auto-attack (D36): with no right-click order, attack the best enemy in reach. Keep
	// the last target while it's in reach; else whoever just hurt my hero; else the
	// nearest minion or hero; then structures; enemy fish last. Never start on a monster
	// (only keep one you were already fighting), a core the towers still protect, or
	// something asleep (your hit would wake it).
	Target Side::AutoAcquire() const
	{
		const HeroState& h = mHero;
		const HeroDef& d = h.Def();
		std::vector<Target> v;
		Targets(h.mArena, v);
		bool aOpen = ArenaHasOpenCore(h.mArena);
		Target aBest;
		float aBestScore = 1e9f;
		for (const Target& t : v)
		{
			if (t.mTeam == mTeam)
				continue;
			bool aSticky = t.mRef == h.mAutoTarget;
			if ((t.mMonster && !aSticky) || t.mAsleep)
				continue;
			if (t.mRef.mKind == ENT_CORE && !aOpen)
				continue;
			if (!InReach(h, t, d.mRange))
				continue;
			if (!d.mWalker && d.mProjectile > 0 && !LineOfSight(h.mArena, h.mPos, t.mPos))
				continue;
			float aScore = Dist(t.mPos, h.mPos);
			if (aSticky)
				aScore -= 5000;
			else if (t.mHero && !Elapsed(mNow, mHurtByAt[t.mRef.mId % kMaxPlayers] + 3000))
				aScore -= 2000;
			else if (t.mStructure)
				aScore += 1000;
			else if (t.mRef.mKind == ENT_FISH)
				aScore += 3000;
			if (aScore < aBestScore)
			{
				aBestScore = aScore;
				aBest = t;
			}
		}
		return aBest;
	}

	void Side::StepAttack()
	{
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		Target t;
		if (h.mOrder == HeroState::ORD_ATTACK)
		{
			if (!FindTarget(h.mTarget, t) || t.mTeam == mTeam)
			{
				h.mOrder = HeroState::ORD_IDLE;
				return;
			}
			if (!InReach(h, t, d.mRange))
				return;
			if (!d.mWalker && d.mProjectile > 0 && !LineOfSight(h.mArena, h.mPos, t.mPos))
				return;							// ranged heroes need a clear shot (StepMovement closes in)
		}
		else
		{
			if (!Elapsed(mNow, h.mNextAttack) || !Elapsed(mNow, h.mDecoyUntil))
				return;							// (a hidden Presto doesn't give himself away)
			t = AutoAcquire();
			if (!t.mRef.Valid())
			{
				h.mAutoTarget = EntityRef();
				return;
			}
			h.mAutoTarget = t.mRef;
		}
		h.mRight = t.mPos.x > h.mPos.x;
		if (!Elapsed(mNow, h.mNextAttack))
			return;
		h.mNextAttack = mNow + (uint32_t)(AttackPeriod() * 1000);
		h.mAttackingUntil = mNow + 260;
		h.mRevealUntil = mNow + 2000;
		h.mDecoyUntil = std::min(h.mDecoyUntil, mNow);	// attacking ends Presto's vanishing act
		float aDamage = Damage();
		if (h.mSharpenLeft > 0)
		{
			aDamage += HeroDefOf(HERO_ITCHY).mAb[AB_W].mDamage * AbilityScale(AB_W);
			h.mSharpenLeft--;
		}
		if (h.Look() == HERO_ITCHY)
		{
			int aMax = HasTalent(2, 0) ? 8 : 5;
			if (t.mRef == h.mStackTarget)
				h.mStacks = std::min(aMax, h.mStacks + 1);
			else
			{
				h.mStackTarget = t.mRef;
				h.mStacks = 0;
			}
		}
		if (t.mStructure)
			aDamage *= StructBonus();
		h.mAttackCount++;
		int aStaticEvery = HasTalent(2, 1) ? 3 : 4;
		bool aChain = (h.Look() == HERO_CLYDE && h.mAttackCount % aStaticEvery == 0)
			|| (HasItem(ITEM_ELECTRIC_SCALE) && h.mAttackCount % kChainEvery == 0);
		if (d.mProjectile > 0)
		{
			Projectile p;
			p.mId = ((uint32_t)mPlayer << 24) | (mNextId++ & 0xFFFFFF);
			p.mArena = h.mArena;
			p.mPos = h.mPos;
			p.mVel = Norm(t.mPos - h.mPos) * d.mProjectile;
			p.mLeft = d.mRange * 3;
			p.mRadius = 12;
			p.mHit = MakeHit(aDamage, SRC_ATTACK);
			p.mHoming = t.mRef;
			switch (h.Look())
			{
			case HERO_CLYDE: p.mLook = LOOK_CLYDE_ATTACK; break;
			case HERO_SPEEDY: p.mLook = LOOK_SLIME; break;
			case HERO_PRESTO: p.mLook = LOOK_SPARKLE; break;
			case HERO_NIKO: p.mLook = LOOK_PEARL; break;
			case HERO_MERYL: p.mLook = LOOK_NOTE; break;
			case HERO_SHRAPNEL: p.mLook = LOOK_BOMB; break;
			default: p.mLook = LOOK_ANGIE_ATTACK; break;
			}
			p.mChain = aChain;
			p.mAttack = true;
			p.mStopAtWalls = false;
			mProj.push_back(p);
			Event e;
			e.mType = EV_PROJECTILE;
			e.mArena = (uint8_t)p.mArena;
			e.mId = p.mId;
			e.mA = p.mPos;
			e.mB = p.mVel;
			e.mMs = (uint16_t)std::min(4000.0f, Dist(t.mPos, h.mPos) / d.mProjectile * 1000 + 200);
			e.mParam = p.mLook;
			Emit(e);
			Sound(h.mArena, h.Look() == HERO_CLYDE ? SND_ZAP : SND_HIT, h.mPos);
		}
		else
			AttackLanded(t, aDamage, aChain);
	}

	void Side::AttackLanded(const Target& t, float theDamage, bool theChain)
	{
		HeroState& h = mHero;
		Hit aHit = MakeHit(theDamage, SRC_ATTACK);
		if (!t.mStructure && HasItem(ITEM_KRAKEN_TOOTH))
		{
			aHit.mMaxHpPct = kKrakenPct;
			aHit.mSlowMs = 1000;
			aHit.mSlowPct = kKrakenSlowPct;
		}
		if (!Elapsed(mNow, h.mMisdirectUntil))
		{
			// Presto's Misdirection.
			h.mMisdirectUntil = 0;
			aHit.mDamage += 30;
			aHit.mSlowMs = 1000;
			aHit.mSlowPct = std::max(aHit.mSlowPct, 0.3f);
		}
		Deal(t.mRef, aHit);
		float aSteal = Lifesteal();
		if (aSteal > 0)
			h.mHp = std::min(MaxHp(), h.mHp + aHit.mDamage * aSteal);
		if (HasBuff(BUFF_BALROG) && !t.mStructure)
		{
			// Balrog's fire: the hit burns on for a while.
			Burn dot;
			dot.mTarget = t.mRef;
			dot.mTicks = 4;
			dot.mPerTick = aHit.mDamage * kBurnShare / dot.mTicks;
			dot.mNext = mNow + (uint32_t)(kBurnS * 1000 / dot.mTicks);
			mBurns.push_back(dot);
		}
		Event e;
		e.mType = EV_SLASH;
		e.mArena = (uint8_t)h.mArena;
		e.mA = t.mPos;
		e.mB = h.mPos;
		e.mParam = (uint8_t)h.Look();
		Emit(e);
		Sound(h.mArena, h.Def().mProjectile > 0 ? SND_HIT : SND_SLASH, t.mPos);
		if (theChain)
		{
			// Clyde's Static, Electric Scale: the hit jumps to two more enemies.
			std::vector<Target> v;
			EnemiesNear(h.mArena, t.mPos, 200, v, false, false);
			int n = 0;
			for (const Target& o : v)
			{
				if (o.mRef == t.mRef || n >= 2)
					continue;
				n++;
				Deal(o.mRef, MakeHit(theDamage * kChainShare, SRC_ABILITY));
				Event l;
				l.mType = EV_LIGHTNING;
				l.mArena = (uint8_t)h.mArena;
				l.mA = t.mPos;
				l.mB = o.mPos;
				Emit(l);
			}
		}
	}

	void Side::AbilityHit(const Target& t, Hit theHit, float theStructMult)
	{
		if (t.mStructure)
			theHit.mDamage *= StructBonus() * theStructMult;
		Deal(t.mRef, theHit);
	}

	void Side::StepProjectiles(float theDt)
	{
		for (size_t i = 0; i < mProj.size();)
		{
			Projectile& p = mProj[i];
			bool aEnd = false;
			float aSpeed = Len(p.mVel);
			Target t;
			if (p.mHoming.Valid())
			{
				// Follow the target while it's in the projectile's arena.
				std::vector<Target> v;
				Targets(p.mArena, v);
				bool aFound = false;
				for (const Target& o : v)
					if (o.mRef == p.mHoming)
					{
						t = o;
						aFound = true;
					}
				if (!aFound)
					aEnd = true;
				else
				{
					p.mVel = Norm(t.mPos - p.mPos) * aSpeed;
					if (Dist(p.mPos, t.mPos) <= t.mRadius + p.mRadius + aSpeed * theDt)
					{
						if (p.mAttack)
							AttackLanded(t, p.mHit.mDamage, p.mChain);
						else
							AbilityHit(t, p.mHit);
						aEnd = true;
					}
				}
			}
			if (!aEnd)
			{
				Vec aFrom = p.mPos;
				p.mPos += p.mVel * theDt;
				p.mLeft -= aSpeed * theDt;
				if (p.mStopAtWalls && (WallBetween(p.mArena, aFrom, p.mPos) || p.mPos.y > FloorY(p.mArena, p.mPos.x)))
					aEnd = true;
				else if (!p.mHoming.Valid())
				{
					// A skillshot hits the first enemy it touches (a piercing one, everything once).
					std::vector<Target> v;
					EnemiesNear(p.mArena, p.mPos, p.mRadius, v, true);
					for (const Target& a : v)
					{
						if (std::find(p.mHitAlready.begin(), p.mHitAlready.end(), a.mRef) != p.mHitAlready.end())
							continue;
						p.mHitAlready.push_back(a.mRef);
						AbilityHit(a, p.mHit);
						Event b;
						b.mType = EV_BURST;
						b.mArena = (uint8_t)p.mArena;
						b.mA = p.mPos;
						b.mValue = 40;
						b.mParam = p.mLook;
						Emit(b);
						Sound(p.mArena, p.mLook == LOOK_CARD ? SND_CARD : SND_ZAP, p.mPos);
						if (p.mOverload)
						{
							// Clyde's Overcharge: on to the next enemy near.
							std::vector<Target> w;
							EnemiesNear(p.mArena, a.mPos, 300, w, false, false);
							for (const Target& n : w)
								if (n.mRef != a.mRef)
								{
									Deal(n.mRef, p.mHit);
									Event l;
									l.mType = EV_LIGHTNING;
									l.mArena = (uint8_t)p.mArena;
									l.mA = a.mPos;
									l.mB = n.mPos;
									Emit(l);
									break;
								}
						}
						if (!p.mPierce)
						{
							aEnd = true;
							break;
						}
					}
				}
				if (p.mLeft <= 0 || p.mPos.x < -50 || p.mPos.x > kWorldW + 50 || p.mPos.y < 0 || p.mPos.y > kWorldH)
					aEnd = true;
			}
			if (aEnd)
			{
				Event e;
				e.mType = EV_PROJ_END;
				e.mArena = (uint8_t)p.mArena;
				e.mId = p.mId;
				e.mA = p.mPos;
				Emit(e);
				mProj.erase(mProj.begin() + i);
				continue;
			}
			i++;
		}
	}

	void Side::StepZones()
	{
		for (size_t i = 0; i < mZones.size();)
		{
			Zone& z = mZones[i];
			if (Elapsed(mNow, z.mUntil))
			{
				mZones.erase(mZones.begin() + i);
				continue;
			}
			if (Elapsed(mNow, z.mNextTick))
			{
				z.mNextTick = mNow + 500;
				std::vector<Target> v;
				EnemiesNear(z.mArena, z.mPos, z.mRadius, v, z.mBlind);
				for (const Target& t : v)
				{
					Hit h = MakeHit(z.mDps * 0.5f, SRC_ZONE, z.mAbility);
					if (z.mSlowPct > 0)
					{
						h.mSlowMs = 700;
						h.mSlowPct = z.mSlowPct;
					}
					if (t.mStructure)
					{
						if (t.mRef.mKind != ENT_TOWER)
							continue;
						h.mBlindMs = z.mBlind ? 700 : 0;
						h.mSlowMs = 0;
					}
					if (h.mDamage > 0 || h.mSlowMs > 0 || h.mBlindMs > 0)
						Deal(t.mRef, h);
				}
			}
			i++;
		}
	}

	void Side::StepImpacts()
	{
		for (size_t i = 0; i < mImpacts.size();)
		{
			Impact im = mImpacts[i];
			if (!Elapsed(mNow, im.mAt))
			{
				i++;
				continue;
			}
			mImpacts.erase(mImpacts.begin() + i);
			std::vector<Target> v;
			EnemiesNear(im.mArena, im.mPos, im.mRadius, v, true);
			for (const Target& t : v)
			{
				Hit h = im.mHit;
				if (h.mPush.x != 0 || h.mPush.y != 0)
					h.mPush = Norm(t.mPos - im.mPos) * Len(h.mPush);
				AbilityHit(t, h, im.mStructMult);
			}
			Burst(im.mArena, im.mPos, im.mRadius, im.mLook);
			Sound(im.mArena, im.mSound, im.mPos);
		}
	}

	void Side::StepTurrets()
	{
		bool aFortress = Rooted();
		for (size_t i = 0; i < mTurrets.size();)
		{
			Turret& t = mTurrets[i];
			if (Elapsed(mNow, t.mUntil))
			{
				Event e;
				e.mType = EV_TURRET_END;
				e.mArena = (uint8_t)t.mArena;
				e.mId = t.mId;
				e.mA = t.mPos;
				if (t.mMine)
					Local(e);
				else
					Emit(e);
				mTurrets.erase(mTurrets.begin() + i);
				continue;
			}
			if (t.mMine)
			{
				// Shrapnel's mine: armed after a moment, bursts when an enemy touches it.
				std::vector<Target> v;
				if (Elapsed(mNow, t.mNextShot))
					EnemiesNear(t.mArena, t.mPos, 46, v, false, false);
				if (!v.empty())
				{
					const AbilityDef& a = HeroDefOf(HERO_SHRAPNEL).mAb[AB_W];
					Impact im;
					im.mArena = t.mArena;
					im.mPos = t.mPos;
					im.mAt = mNow;
					im.mRadius = a.mRadius;
					im.mHit = MakeHit(a.mDamage * AbilityScale(AB_W), SRC_ABILITY, AB_W);
					im.mHit.mSlowMs = 1500;
					im.mHit.mSlowPct = a.mExtra;
					im.mLook = LOOK_MINE;
					im.mSound = SND_BOOM;
					mImpacts.push_back(im);
					t.mUntil = mNow;			// gone next step
				}
				i++;
				continue;
			}
			// Niko's turret: a pearl at the nearest enemy in reach (twice as fast in Fortress).
			const AbilityDef& a = HeroDefOf(HERO_NIKO).mAb[AB_Q];
			if (Elapsed(mNow, t.mNextShot))
			{
				std::vector<Target> v;
				EnemiesNear(t.mArena, t.mPos, a.mRadius, v, false, false);
				const Target* aBest = nullptr;
				for (const Target& o : v)
					if (o.mRef.mKind != ENT_FISH && (aBest == nullptr || Dist(o.mPos, t.mPos) < Dist(aBest->mPos, t.mPos)))
						aBest = &o;
				if (aBest != nullptr)
				{
					t.mNextShot = mNow + (uint32_t)(a.mExtra * (aFortress ? 0.5f : 1.0f) * 1000);
					Projectile p;
					p.mId = ((uint32_t)mPlayer << 24) | (mNextId++ & 0xFFFFFF);
					p.mArena = t.mArena;
					p.mPos = t.mPos - Vec(0, 14);
					p.mVel = Norm(aBest->mPos - p.mPos) * 620;
					p.mLeft = a.mRadius * 2;
					p.mRadius = 10;
					p.mHit = MakeHit(a.mDamage * AbilityScale(AB_Q) * (HasTalent(0, 1) ? 1.5f : 1.0f), SRC_ABILITY, AB_Q);
					p.mHoming = aBest->mRef;
					p.mLook = LOOK_PEARL;
					p.mStopAtWalls = false;
					mProj.push_back(p);
					Event e;
					e.mType = EV_PROJECTILE;
					e.mArena = (uint8_t)p.mArena;
					e.mId = p.mId;
					e.mA = p.mPos;
					e.mB = p.mVel;
					e.mMs = (uint16_t)std::min(3000.0f, Dist(aBest->mPos, p.mPos) / 620 * 1000 + 150);
					e.mParam = LOOK_PEARL;
					Emit(e);
					Sound(t.mArena, SND_TOWER, t.mPos);
				}
			}
			i++;
		}
	}

	void Side::StepBurns()
	{
		for (size_t i = 0; i < mBurns.size();)
		{
			Burn& d = mBurns[i];
			if (Elapsed(mNow, d.mNext))
			{
				d.mNext = mNow + (uint32_t)(kBurnS * 1000 / 4);
				d.mTicks--;
				Deal(d.mTarget, MakeHit(d.mPerTick, SRC_BURN));
			}
			if (d.mTicks <= 0)
				mBurns.erase(mBurns.begin() + i);
			else
				i++;
		}
	}
}
