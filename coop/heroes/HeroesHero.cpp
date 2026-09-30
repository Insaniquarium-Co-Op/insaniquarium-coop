// Pet Heroes - the hero: orders, movement and pathing, portals, kelp, auto-attacks,
// projectiles, zones and the five heroes' abilities. Part of Side (HeroesSide.h).

#include "HeroesSide.h"
#include "HeroesMap.h"

namespace Heroes
{
	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }
	static const float kWalkerReachUp = 320;	// walkers pounce	// walkers hit things this far above them
	static const float kRevealR = 150;			// enemy heroes this close see you in kelp

	///////////////////////////////////////////////////////////////////////////
	// Orders
	///////////////////////////////////////////////////////////////////////////
	void Side::OrderMove(Vec theWhere)
	{
		HeroState& h = mHero;
		if (!h.mAlive)
			return;
		h.mOrder = HeroState::ORD_MOVE;
		const MapDef& m = TheMap();
		h.mWantCross = Dist(theWhere, m.mPortal) < m.mPortalR + 24;
		if (h.mWantCross)
			theWhere = m.mPortal;
		h.mMoveTo = h.Def().mWalker ? WalkerPos(theWhere.x, h.Def().mRadius) : NearestOpen(theWhere);
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
			OrderStop();						// the keys take over from a right-click order
	}

	bool Side::Hop()
	{
		HeroState& h = mHero;
		if (!h.mAlive || !h.Def().mWalker || Stunned() || Busy() || Hopping())
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
		// The nearest crossing along the floor: the portal's beam or a pad.
		const MapDef& m = TheMap();
		Vec aBest = m.mPortal;
		float aBestD = std::fabs(h.mPos.x - m.mPortal.x);
		for (int i = 0; i < 2; i++)
		{
			float dx = std::fabs(h.mPos.x - m.mPad[i].x) - m.mPadR;
			if (dx < aBestD)
			{
				aBestD = dx;
				aBest = m.mPad[i];
			}
		}
		if (aBestD > kCrossAssistR)
		{
			mLastRefusal = "S crosses near the portal's beam or a floor pad.";
			return false;
		}
		if (WarpSicknessLeft() > 0)
		{
			mLastRefusal = "Warp sickness: " + std::to_string((WarpSicknessLeft() + 999) / 1000) + " s before you can cross again.";
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
	void Side::StepHero(float theDt)
	{
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		if (!h.mAlive)
		{
			if (Elapsed(mNow, h.mRespawnAt))
				Respawn();
			return;
		}
		float aMax = MaxHp();
		// The fountain: heal quickly next to your own core.
		if (h.mArena == mTeam && Dist(h.mPos, TheMap().mCore) < kFountainRadius)
			h.mHp = std::min(aMax, h.mHp + aMax * kFountainHealPct * theDt);
		else
			h.mHp = std::min(aMax, h.mHp + aMax * 0.004f * theDt);	// a slow trickle anywhere
		if (h.mShield > 0 && Elapsed(mNow, h.mShieldUntil))
			h.mShield = 0;
		// Unspent ability points spend themselves after a few seconds.
		if (h.mPoints > 0 && Elapsed(mNow, h.mPointAt + (uint32_t)(kAutoRankS * 1000)))
		{
			int aWant[AB_COUNT] = { 1, 1, 1, 0 };
			for (int i = 0; i < 9; i++)
			{
				int s = d.mRankOrder[i];
				aWant[s]++;
				if (h.mRank[s] < aWant[s] && h.mRank[s] < kMaxRank)
				{
					SpendPoint(s);
					break;
				}
			}
			if (h.mPoints > 0)
				for (int s = AB_Q; s <= AB_E && h.mPoints > 0; s++)
					if (h.mRank[s] < kMaxRank)
						SpendPoint(s);
		}
		if (h.mSharpenLeft > 0 && Elapsed(mNow, h.mSharpenUntil))
			h.mSharpenLeft = 0;

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
			h.mPos = WalkerPos(h.mPos.x, d.mRadius) - Vec(0, aUp);
		}

		// Portals: the warp hole (top middle) and the floor pads (bottom corners).
		const MapDef& m = TheMap();
		int aGate = -1;
		if (Dist(h.mPos, m.mPortal) < m.mPortalR)
			aGate = 0;
		if (d.mWalker && h.mWantCross && std::fabs(h.mPos.x - m.mPortal.x) < 14)
			aGate = 0;							// the portal's beam lifts walkers from the floor below
		for (int i = 0; i < 2; i++)
			if (Dist(h.mPos, m.mPad[i]) < m.mPadR + d.mRadius)
				aGate = 1 + i;
		if (aGate < 0)
			h.mInPortal = false;
		else if (!h.mInPortal && !Stunned() && !Busy() && WarpSicknessLeft() == 0)
		{
			int aTo = h.mArena == mTeam ? 1 - mTeam : mTeam;
			Vec aExit;
			if (aGate == 0)
				aExit = d.mWalker ? WalkerPos(m.mPortalExit.x, d.mRadius) : m.mPortalExit;	// the middle: out of both towers' reach
			else
			{
				float x = m.mPad[aGate - 1].x + (aGate == 1 ? 110.0f : -110.0f);
				aExit = d.mWalker ? WalkerPos(x, d.mRadius) : ClampToWater(Vec(x, m.mPad[aGate - 1].y - 90), d.mRadius);
			}
			CrossTo(aTo, aExit);
		}

		// Kelp hides you unless an enemy hero is close or you just attacked.
		bool aKelp = KelpAt(h.mPos) >= 0;
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
		h.mCrossReadyAt = mNow + (uint32_t)(kWarpSicknessS * 1000);
		h.mOrder = HeroState::ORD_IDLE;
		h.mPath.clear();
		h.mTarget = EntityRef();
		h.mStormUntil = 0;
		h.mSlimeUntil = 0;
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
			if (InReach(h, t, d.mRange))
				return;							// StepAttack does the rest
			aGoal = t.mPos;
		}
		else
			return;

		float aStep = Speed() * theDt;
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
			h.mPos = WalkerPos(x, d.mRadius);
		}
		else
		{
			if (h.mPath.empty() || Elapsed(mNow, h.mRepathAt))
			{
				FindPath(h.mPos, aGoal, h.mPath);
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
			h.mPos = ClampToWater(h.mPos, d.mRadius);
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
		Vec aOld = h.mPos;
		if (d.mWalker)
		{
			if (mSteer.x != 0)
				h.mPos = WalkerPos(Clamp(h.mPos.x + (mSteer.x > 0 ? aStep : -aStep), d.mRadius, kWorldW - d.mRadius), d.mRadius);
		}
		else
		{
			// Swim. Against a wall, slide along it: first the part of the move that fits,
			// then turned 45 and 90 degrees, keeping to one side until clear (never get stuck
			// inside one either).
			bool aFree = !SwimmerFits(h.mPos, d.mRadius);
			auto TryMove = [&](Vec theMove) {
				Vec q = ClampToWater(h.mPos + theMove, d.mRadius);
				if (Dist(q, h.mPos) < 0.01f || (!aFree && !SwimmerFits(q, d.mRadius)))
					return false;
				h.mPos = q;
				return true;
			};
			Vec v = mSteer * aStep;
			bool aMoved = TryMove(v) || TryMove(Vec(v.x, 0)) || TryMove(Vec(0, v.y));
			bool aAtEdge = Dist(ClampToWater(h.mPos + v, d.mRadius), h.mPos + v) > 0.01f;	// turning gets around walls, not along the tank's edges
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

	void Side::StepAttack()
	{
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		if (h.mOrder != HeroState::ORD_ATTACK)
			return;
		Target t;
		if (!FindTarget(h.mTarget, t) || t.mTeam == mTeam)
		{
			h.mOrder = HeroState::ORD_IDLE;
			return;
		}
		if (!InReach(h, t, d.mRange))
			return;
		if (!d.mWalker && d.mProjectile > 0 && !LineOfSight(h.mPos, t.mPos))
			return;								// ranged heroes need a clear shot (StepMovement closes in)
		h.mRight = t.mPos.x > h.mPos.x;
		if (!Elapsed(mNow, h.mNextAttack))
			return;
		h.mNextAttack = mNow + (uint32_t)(AttackPeriod() * 1000);
		h.mAttackingUntil = mNow + 260;
		h.mRevealUntil = mNow + 2000;
		float aDamage = Damage();
		if (h.mSharpenLeft > 0)
		{
			aDamage += d.mAb[AB_W].mDamage * AbilityScale(AB_W);
			h.mSharpenLeft--;
		}
		if (h.mHero == HERO_ITCHY)
		{
			if (t.mRef == h.mStackTarget)
				h.mStacks = std::min(5, h.mStacks + 1);
			else
			{
				h.mStackTarget = t.mRef;
				h.mStacks = 0;
			}
		}
		if (t.mStructure)
			aDamage *= StructBonus();
		h.mAttackCount++;
		bool aChain = h.mHero == HERO_CLYDE && h.mAttackCount % 4 == 0;
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
			p.mLook = h.mHero == HERO_CLYDE ? LOOK_CLYDE_ATTACK : (h.mHero == HERO_SPEEDY ? LOOK_SLIME : LOOK_ANGIE_ATTACK);
			p.mChain = aChain;
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
			Sound(h.mArena, h.mHero == HERO_CLYDE ? SND_ZAP : SND_HIT, h.mPos);
		}
		else
			AttackLanded(t, aDamage, false);
	}

	void Side::AttackLanded(const Target& t, float theDamage, bool theChain)
	{
		HeroState& h = mHero;
		Hit aHit = MakeHit(theDamage, SRC_ATTACK);
		Deal(t.mRef, aHit);
		float aSteal = Lifesteal();
		if (aSteal > 0)
			h.mHp = std::min(MaxHp(), h.mHp + theDamage * aSteal);
		Event e;
		e.mType = EV_SLASH;
		e.mArena = (uint8_t)h.mArena;
		e.mA = t.mPos;
		e.mB = h.mPos;
		e.mParam = h.mHero;
		Emit(e);
		Sound(h.mArena, h.Def().mProjectile > 0 ? SND_HIT : SND_SLASH, t.mPos);
		if (theChain)
		{
			// Clyde's Static: the bolt jumps to two more enemies.
			std::vector<Target> v;
			EnemiesNear(h.mArena, t.mPos, 200, v, false);
			int n = 0;
			for (const Target& o : v)
			{
				if (o.mRef == t.mRef || n >= 2)
					continue;
				n++;
				Deal(o.mRef, MakeHit(theDamage * 0.6f, SRC_ABILITY));
				Event l;
				l.mType = EV_LIGHTNING;
				l.mArena = (uint8_t)h.mArena;
				l.mA = t.mPos;
				l.mB = o.mPos;
				Emit(l);
			}
		}
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
				if (!FindTarget(p.mHoming, t) || (t.mRef.mKind != ENT_HERO && t.mRef.mArena != p.mArena))
					aEnd = true;
				else
				{
					p.mVel = Norm(t.mPos - p.mPos) * aSpeed;
					if (Dist(p.mPos, t.mPos) <= t.mRadius + p.mRadius + aSpeed * theDt)
					{
						AttackLanded(t, p.mHit.mDamage, p.mChain);
						aEnd = true;
					}
				}
			}
			if (!aEnd)
			{
				Vec aFrom = p.mPos;
				p.mPos += p.mVel * theDt;
				p.mLeft -= aSpeed * theDt;
				if (p.mStopAtWalls && (WallBetween(aFrom, p.mPos) || p.mPos.y > FloorY(p.mPos.x)))
					aEnd = true;
				else if (!p.mHoming.Valid())
				{
					// A skillshot hits the first enemy it touches.
					std::vector<Target> v;
					EnemiesNear(p.mArena, p.mPos, p.mRadius, v, true);
					if (!v.empty())
					{
						const Target& a = v.front();
						Hit h = p.mHit;
						if (a.mStructure)
							h.mDamage *= StructBonus();
						Deal(a.mRef, h);
						Event b;
						b.mType = EV_BURST;
						b.mArena = (uint8_t)p.mArena;
						b.mA = p.mPos;
						b.mValue = 40;
						b.mParam = p.mLook;
						Emit(b);
						Sound(p.mArena, SND_ZAP, p.mPos);
						aEnd = true;
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
					Hit h = MakeHit(z.mDps * 0.5f, SRC_ZONE);
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

	///////////////////////////////////////////////////////////////////////////
	// Abilities
	///////////////////////////////////////////////////////////////////////////
	bool Side::CanCast(int theSlot, std::string* theWhy) const
	{
		auto No = [&](const char* s) { if (theWhy != nullptr) *theWhy = s; return false; };
		const HeroState& h = mHero;
		if (theSlot < 0 || theSlot >= AB_COUNT)
			return No("?");
		if (Over())
			return No("The match is over.");
		if (!h.mAlive)
			return No("You're respawning.");
		if (h.mRank[theSlot] <= 0)
			return No("Unlocks at level 5.");
		if (Stunned())
			return No("Stunned!");
		if (Busy())
			return No("Not now.");
		if (!Elapsed(mNow, h.mReadyAt[theSlot]))
			return No("Not ready yet.");
		return true;
	}

	void Side::StepAbilities(float theDt)
	{
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		std::vector<Target> v;

		// Itchy's Lunge.
		if (!Elapsed(mNow, h.mDashUntil))
		{
			Vec aNext = h.mPos + h.mDashDir * (h.mDashSpeed * theDt);
			if (SwimmerFits(aNext, d.mRadius))
				h.mPos = aNext;
			else
				h.mDashUntil = mNow;
			if (!h.mDashHit)
			{
				EnemiesNear(h.mArena, h.mPos, d.mAb[AB_Q].mRadius, v, true);
				if (!v.empty())
				{
					const Target& t = v.front();
					float aDamage = d.mAb[AB_Q].mDamage * AbilityScale(AB_Q) * (t.mStructure ? StructBonus() : 1);
					Deal(t.mRef, MakeHit(aDamage, SRC_ABILITY));
					h.mDashHit = true;
					h.mDashUntil = mNow;
					Event e;
					e.mType = EV_SLASH;
					e.mArena = (uint8_t)h.mArena;
					e.mA = t.mPos;
					e.mB = h.mPos;
					e.mParam = h.mHero;
					Emit(e);
					Sound(h.mArena, SND_SLASH, t.mPos);
					if (t.mRef.mKind == ENT_HERO || t.mRef.mKind == ENT_MINION)
					{
						h.mOrder = HeroState::ORD_ATTACK;	// keep fighting what you hit
						h.mTarget = t.mRef;
					}
				}
			}
		}

		// Rhubarb's Leap.
		if (h.mLeapUntil != 0)
		{
			float aSpan = (float)std::max<uint32_t>(1, h.mLeapUntil - h.mLeapStart);
			float t = Clamp((float)(int32_t)(mNow - h.mLeapStart) / aSpan, 0, 1);
			h.mPos = Lerp(h.mLeapFrom, h.mLeapTo, t) - Vec(0, std::sin(t * 3.14159f) * 150);
			if (t >= 1)
			{
				h.mLeapUntil = 0;
				h.mPos = h.mLeapTo;
				const AbilityDef& a = d.mAb[AB_Q];
				EnemiesNear(h.mArena, h.mPos, a.mRadius, v, true);
				for (const Target& o : v)
				{
					Hit hit = MakeHit(a.mDamage * AbilityScale(AB_Q) * (o.mStructure ? StructBonus() : 1), SRC_ABILITY);
					if (!o.mStructure)
						hit.mStunMs = (uint16_t)(a.mExtra * 1000);
					Deal(o.mRef, hit);
				}
				Event e;
				e.mType = EV_BURST;
				e.mArena = (uint8_t)h.mArena;
				e.mA = h.mPos;
				e.mValue = a.mRadius;
				e.mParam = LOOK_LEAP;
				Emit(e);
				Sound(h.mArena, SND_EXPLODE, h.mPos);
			}
		}

		// Itchy's Swordstorm.
		if (!Elapsed(mNow, h.mStormUntil) && Elapsed(mNow, h.mStormNext))
		{
			const AbilityDef& a = d.mAb[AB_R];
			h.mStormNext = mNow + (uint32_t)(a.mExtra * 1000);
			EnemiesNear(h.mArena, h.mPos, a.mRadius, v, true);
			for (const Target& o : v)
				Deal(o.mRef, MakeHit(a.mDamage * AbilityScale(AB_R) * (o.mStructure ? StructBonus() : 1), SRC_ABILITY));
			if (!v.empty())
				Sound(h.mArena, SND_SLASH, h.mPos);
		}

		// Clyde's Thunderstorm: every enemy in the tank, a few times.
		if (h.mThunderLeft > 0 && Elapsed(mNow, h.mThunderNext))
		{
			const AbilityDef& a = d.mAb[AB_R];
			h.mThunderLeft--;
			h.mThunderNext = mNow + (uint32_t)(a.mDurationS / a.mExtra * 1000);
			std::vector<Target> all;
			Targets(h.mArena, all);
			for (const Target& o : all)
			{
				if (o.mTeam == mTeam || o.mStructure)
					continue;
				float aVsFish = o.mRef.mKind == ENT_FISH ? 0.25f : 1.0f;	// thins a farm, doesn't erase it
				Deal(o.mRef, MakeHit(a.mDamage * AbilityScale(AB_R) * aVsFish, SRC_ABILITY));
				Event e;
				e.mType = EV_LIGHTNING;
				e.mArena = (uint8_t)h.mArena;
				e.mA = Vec(o.mPos.x + 30, kSurfaceY - 20);
				e.mB = o.mPos;
				Emit(e);
			}
			Sound(h.mArena, SND_THUNDER, h.mPos);
		}

		// Speedy's Slime Trail.
		if (!Elapsed(mNow, h.mSlimeUntil) && Dist(h.mPos, h.mLastSlime) > 34)
		{
			const AbilityDef& a = d.mAb[AB_Q];
			h.mLastSlime = h.mPos;
			Zone z;
			z.mArena = h.mArena;
			z.mPos = h.mPos + Vec(0, d.mRadius * 0.6f);
			z.mRadius = a.mRadius;
			z.mUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			z.mSlowPct = a.mExtra;
			z.mLook = LOOK_SLIME;
			mZones.push_back(z);
			Event e;
			e.mType = EV_ZONE;
			e.mArena = (uint8_t)z.mArena;
			e.mA = z.mPos;
			e.mValue = z.mRadius;
			e.mMs = (uint16_t)(a.mDurationS * 1000);
			e.mParam = LOOK_SLIME;
			Emit(e);
		}

		// Angie's Heal Beam.
		if (!Elapsed(mNow, h.mHealUntil) && Elapsed(mNow, h.mHealNext))
		{
			h.mHealNext = mNow + 250;
			if (h.mHealTarget.mKind == ENT_HERO)
				h.mHp = std::min(MaxHp(), h.mHp + h.mHealPerTick);
			else
			{
				Hit hit;
				hit.mTeam = (uint8_t)mTeam;
				hit.mHeal = h.mHealPerTick * 0.6f;		// towers and the core heal slower
				Deal(h.mHealTarget, hit);
			}
		}
	}

	// Angie's friendly aim: an own tower or core near the mouse (in her own tank), else herself.
	static EntityRef FriendNear(const Side& s, Vec theAim, float theRange, Vec& thePos)
	{
		const HeroState& h = s.mHero;
		thePos = h.mPos;
		if (h.mArena != s.mTeam)
			return EntityRef::Hero(s.mPlayer);
		const MapDef& m = TheMap();
		EntityRef aBest = EntityRef::Hero(s.mPlayer);
		float aBestD = 130;
		for (int i = 0; i < 2; i++)
			if (s.mArena.mTower[i].mAlive && Dist(m.mTower[i], theAim) < aBestD && Dist(m.mTower[i], h.mPos) < theRange + kTowerR)
			{
				aBestD = Dist(m.mTower[i], theAim);
				aBest = EntityRef::Of(s.mTeam, ENT_TOWER, (uint32_t)i);
				thePos = m.mTower[i];
			}
		if (Dist(m.mCore, theAim) < aBestD && Dist(m.mCore, h.mPos) < theRange + kCoreR)
		{
			aBest = EntityRef::Of(s.mTeam, ENT_CORE, 0);
			thePos = m.mCore;
		}
		return aBest;
	}

	bool Side::Cast(int theSlot, Vec theAim)
	{
		if (!CanCast(theSlot, &mLastRefusal))
			return false;
		mLastRefusal.clear();
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		const AbilityDef& a = d.mAb[theSlot];
		float aScale = AbilityScale(theSlot);
		int aRank = theSlot == AB_R ? std::max(1, h.mRank[AB_R]) : h.mRank[theSlot];
		h.mReadyAt[theSlot] = mNow + (uint32_t)(a.mCooldownS * RankCooldown(aRank) * CooldownMult() * 1000);
		Vec aDir = Norm(theAim - h.mPos);
		if (aDir.x == 0 && aDir.y == 0)
			aDir = Vec(h.mRight ? 1.0f : -1.0f, 0);
		Vec aOff = theAim - h.mPos;
		if (Len(aOff) > a.mRange)
			aOff = Norm(aOff) * a.mRange;
		Vec aPoint = h.mPos + aOff;
		if (aDir.x != 0)
			h.mRight = aDir.x > 0;
		h.mRevealUntil = mNow + 2000;
		std::vector<Target> v;

		auto Burst = [&](Vec thePos, float theRadius, uint8_t theLook) {
			Event e;
			e.mType = EV_BURST;
			e.mArena = (uint8_t)h.mArena;
			e.mA = thePos;
			e.mValue = theRadius;
			e.mParam = theLook;
			Emit(e);
		};
		auto AddZone = [&](Vec thePos, float theRadius, float theDps, float theSlow, bool theBlind, uint8_t theLook) {
			Zone z;
			z.mArena = h.mArena;
			z.mPos = thePos;
			z.mRadius = theRadius;
			z.mUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			z.mNextTick = mNow;
			z.mDps = theDps;
			z.mSlowPct = theSlow;
			z.mBlind = theBlind;
			z.mLook = theLook;
			mZones.push_back(z);
			Event e;
			e.mType = EV_ZONE;
			e.mArena = (uint8_t)z.mArena;
			e.mA = thePos;
			e.mValue = theRadius;
			e.mMs = (uint16_t)(a.mDurationS * 1000);
			e.mParam = theLook;
			Emit(e);
		};

		switch (h.mHero * AB_COUNT + theSlot)
		{
		// ---- Itchy ----
		case HERO_ITCHY * AB_COUNT + AB_Q:
			h.mDashUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			h.mDashDir = aDir;
			h.mDashSpeed = a.mRange / a.mDurationS;
			h.mDashHit = false;
			h.mPath.clear();
			Burst(h.mPos, 30, LOOK_LUNGE);
			Sound(h.mArena, SND_SLASH, h.mPos);
			break;
		case HERO_ITCHY * AB_COUNT + AB_W:
			h.mSharpenLeft = (int)a.mExtra;
			h.mSharpenUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			Text(h.mArena, h.mPos - Vec(0, 56), "Sharpened!", TC_INFO);
			Sound(h.mArena, SND_SHIELD, h.mPos);
			break;
		case HERO_ITCHY * AB_COUNT + AB_E:
			h.mUntargetableUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			h.mSpeedUntil = mNow + (uint32_t)(a.mExtra * 1000);
			h.mSpeedPct = a.mDamage;
			Burst(h.mPos, 40, LOOK_TELEPORT);
			Sound(h.mArena, SND_WARP, h.mPos);
			break;
		case HERO_ITCHY * AB_COUNT + AB_R:
			h.mStormUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			h.mStormNext = mNow;
			Burst(h.mPos, a.mRadius, LOOK_STORM);
			Sound(h.mArena, SND_ROAR, h.mPos);
			break;

		// ---- Clyde ----
		case HERO_CLYDE * AB_COUNT + AB_Q:
		{
			Projectile p;
			p.mId = ((uint32_t)mPlayer << 24) | (mNextId++ & 0xFFFFFF);
			p.mArena = h.mArena;
			p.mPos = h.mPos;
			p.mVel = aDir * a.mExtra;
			p.mLeft = a.mRange;
			p.mRadius = a.mRadius;
			p.mHit = MakeHit(a.mDamage * aScale, SRC_ABILITY);
			p.mLook = LOOK_ZAP;
			mProj.push_back(p);
			Event e;
			e.mType = EV_PROJECTILE;
			e.mArena = (uint8_t)p.mArena;
			e.mId = p.mId;
			e.mA = p.mPos;
			e.mB = p.mVel;
			e.mMs = (uint16_t)(a.mRange / a.mExtra * 1000);
			e.mParam = LOOK_ZAP;
			Emit(e);
			Sound(h.mArena, SND_ZAP, h.mPos);
			break;
		}
		case HERO_CLYDE * AB_COUNT + AB_W:
			AddZone(aPoint, a.mRadius, a.mDamage * aScale, a.mExtra, false, LOOK_STATIC);
			Sound(h.mArena, SND_ZAP, aPoint);
			break;
		case HERO_CLYDE * AB_COUNT + AB_E:
		{
			Vec aTo = SwimmerFits(aPoint, d.mRadius) ? aPoint : NearestOpen(aPoint);
			Burst(h.mPos, 36, LOOK_TELEPORT);
			h.mPos = aTo;
			h.mPath.clear();
			if (h.mOrder == HeroState::ORD_MOVE)
				h.mOrder = HeroState::ORD_IDLE;
			Burst(h.mPos, 36, LOOK_TELEPORT);
			Sound(h.mArena, SND_WARP, h.mPos);
			break;
		}
		case HERO_CLYDE * AB_COUNT + AB_R:
			h.mThunderLeft = (int)a.mExtra;
			h.mThunderNext = mNow;
			Text(h.mArena, h.mPos - Vec(0, 56), "Thunderstorm!", TC_INFO);
			break;

		// ---- Rhubarb ----
		case HERO_RHUBARB * AB_COUNT + AB_Q:
			h.mLeapFrom = h.mPos;
			h.mLeapTo = WalkerPos(Clamp(theAim.x, h.mPos.x - a.mRange, h.mPos.x + a.mRange), d.mRadius);
			h.mLeapStart = mNow;
			h.mLeapUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			h.mHopUntil = 0;
			h.mOrder = HeroState::ORD_IDLE;
			Sound(h.mArena, SND_ROAR, h.mPos);
			break;
		case HERO_RHUBARB * AB_COUNT + AB_W:
		{
			// The first enemy along the line gets pulled in.
			std::vector<Target> all;
			Targets(h.mArena, all);
			Vec aEnd = h.mPos + aDir * a.mRange;
			const Target* aBest = nullptr;
			float aBestD = 1e9f;
			for (const Target& o : all)
			{
				if (o.mTeam == mTeam || o.mStructure || o.mRef.mKind == ENT_FISH)
					continue;
				if (DistPointSeg(o.mPos, h.mPos, aEnd) > a.mRadius + o.mRadius)
					continue;
				float dd = Dist(o.mPos, h.mPos);
				if (dd < aBestD && FirstBlock(h.mPos, o.mPos) > 1)
				{
					aBestD = dd;
					aBest = &o;
				}
			}
			Event e;
			e.mType = EV_BEAM;
			e.mArena = (uint8_t)h.mArena;
			e.mA = h.mPos;
			e.mB = aBest != nullptr ? aBest->mPos : aEnd;
			e.mMs = 300;
			e.mParam = 1;						// claw
			Emit(e);
			if (aBest != nullptr)
			{
				Hit hit = MakeHit(a.mDamage * aScale, SRC_ABILITY);
				hit.mPull = true;
				hit.mPullTo = h.mPos + aDir * (d.mRadius + aBest->mRadius + 8);
				Deal(aBest->mRef, hit);
				h.mOrder = HeroState::ORD_ATTACK;
				h.mTarget = aBest->mRef;
			}
			Sound(h.mArena, SND_CHOMP, h.mPos);
			break;
		}
		case HERO_RHUBARB * AB_COUNT + AB_E:
			h.mImmuneUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			h.mTauntUntil = h.mImmuneUntil;
			Burst(h.mPos, 50, LOOK_HALO);
			Sound(h.mArena, SND_SHIELD, h.mPos);
			break;
		case HERO_RHUBARB * AB_COUNT + AB_R:
			EnemiesNear(h.mArena, h.mPos, a.mRadius, v, true);
			for (const Target& o : v)
			{
				Hit hit = MakeHit(a.mDamage * aScale * (o.mStructure ? StructBonus() : 1), SRC_ABILITY);
				if (!o.mStructure)
				{
					hit.mStunMs = (uint16_t)(a.mExtra * 1000);
					hit.mPush = Norm(o.mPos - h.mPos) * 110;
				}
				Deal(o.mRef, hit);
			}
			Burst(h.mPos, a.mRadius, LOOK_SLAM);
			Sound(h.mArena, SND_EXPLODE, h.mPos);
			break;

		// ---- Angie ----
		case HERO_ANGIE * AB_COUNT + AB_Q:
		{
			Vec aAt;
			h.mHealTarget = FriendNear(*this, theAim, a.mRange, aAt);
			h.mHealUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			h.mHealNext = mNow;
			h.mHealPerTick = a.mDamage * aScale / (a.mDurationS / 0.25f);
			Event e;
			e.mType = EV_BEAM;
			e.mArena = (uint8_t)h.mArena;
			e.mA = h.mPos;
			e.mB = aAt;
			e.mMs = (uint16_t)(a.mDurationS * 1000);
			e.mParam = 0;						// heal
			Emit(e);
			Sound(h.mArena, SND_HEAL, aAt);
			break;
		}
		case HERO_ANGIE * AB_COUNT + AB_W:
		{
			Vec aAt;
			EntityRef aWho = FriendNear(*this, theAim, a.mRange, aAt);
			if (aWho.mKind == ENT_HERO)
			{
				h.mShield = std::max(h.mShield, a.mDamage * aScale);
				h.mShieldUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			}
			else
			{
				Hit hit;
				hit.mTeam = (uint8_t)mTeam;
				hit.mShield = a.mDamage * aScale;
				hit.mShieldMs = (uint16_t)(a.mDurationS * 1000);
				Deal(aWho, hit);
			}
			Burst(aAt, 60, LOOK_HALO);
			Sound(h.mArena, SND_SHIELD, aAt);
			break;
		}
		case HERO_ANGIE * AB_COUNT + AB_E:
		{
			std::vector<Target> all;
			Targets(h.mArena, all);
			for (const Target& o : all)
				if (o.mRef.mKind == ENT_MINION && o.mTeam != mTeam && Dist(o.mPos, h.mPos) < a.mRadius + o.mRadius)
				{
					Hit hit;
					hit.mTeam = (uint8_t)mTeam;
					hit.mPlayer = (int8_t)mPlayer;
					hit.mCharmMs = (uint16_t)(a.mDurationS * 1000);
					Deal(o.mRef, hit);
				}
			Burst(h.mPos, a.mRadius, LOOK_CHARM);
			Sound(h.mArena, SND_HEAL, h.mPos);
			break;
		}
		case HERO_ANGIE * AB_COUNT + AB_R:
		{
			mArena.Revive((int)a.mExtra, mNow);
			for (int i = 0; i < 2; i++)
				if (mArena.mTower[i].mAlive)
				{
					Hit hit;
					hit.mTeam = (uint8_t)mTeam;
					hit.mHeal = a.mDamage * aScale;
					mArena.ApplyHit(EntityRef::Of(mTeam, ENT_TOWER, (uint32_t)i), hit, mNow);
				}
			h.mHp = MaxHp();
			Burst(h.mPos, 120, LOOK_RESURRECT);
			Burst(TheMap().mCore, 160, LOOK_RESURRECT);
			Sound(h.mArena, SND_HEAL, h.mPos);
			break;
		}

		// ---- Speedy ----
		case HERO_SPEEDY * AB_COUNT + AB_Q:
			h.mSlimeUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			h.mLastSlime = Vec(-1000, -1000);
			Sound(h.mArena, SND_SPLASH, h.mPos);
			break;
		case HERO_SPEEDY * AB_COUNT + AB_W:
			AddZone(aPoint, a.mRadius, a.mDamage * aScale, 0, true, LOOK_STINK);
			Sound(h.mArena, SND_STINK, aPoint);
			break;
		case HERO_SPEEDY * AB_COUNT + AB_E:
			h.mImmuneUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			Burst(h.mPos, 44, LOOK_HALO);
			Sound(h.mArena, SND_SHIELD, h.mPos);
			break;
		case HERO_SPEEDY * AB_COUNT + AB_R:
			h.mGoldRushUntil = mNow + (uint32_t)(a.mDurationS * 1000);
			Burst(h.mPos, 90, LOOK_GOLD);
			Text(h.mArena, h.mPos - Vec(0, 56), "Gold Rush!", TC_MONEY);
			Sound(h.mArena, SND_DIAMOND, h.mPos);
			break;
		default:
			break;
		}
		return true;
	}
}
