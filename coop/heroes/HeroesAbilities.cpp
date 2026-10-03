// Pet Heroes - the nine heroes' abilities and talents (Q, E, R and the ultimate F),
// and what keeps running after a cast. Part of Side (HeroesSide.h).

#include "HeroesSide.h"
#include "HeroesMap.h"

namespace Heroes
{
	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }
	static uint32_t Ms(float theSeconds) { return (uint32_t)std::max(0.0f, theSeconds * 1000); }

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
		if (theSlot == AB_R && h.mCopyHero >= 0)
			return true;						// F ends Copycat early
		if (h.mRank[theSlot] <= 0)
			return No("Your pet evolves at level 6: then F unlocks.");
		if (Stunned())
			return No((int32_t)(mNow - h.mSleepUntil) < 0 ? "Asleep!" : "Stunned!");
		if (Busy())
			return No("Not now.");
		if (!Elapsed(mNow, h.mReadyAt[theSlot]))
			return No("Not ready yet.");
		return true;
	}

	void Side::EndCopy()
	{
		HeroState& h = mHero;
		if (h.mCopyHero < 0)
			return;
		float aFrac = h.mHp / std::max(1.0f, MaxHp());
		h.mCopyHero = -1;
		h.mHp = std::max(1.0f, aFrac * MaxHp());
		for (int i = AB_Q; i <= AB_E; i++)
			h.mReadyAt[i] = Elapsed(mNow, h.mSavedReady[i]) ? mNow : h.mSavedReady[i];
		h.mStormUntil = h.mSlimeUntil = h.mHealUntil = h.mFortressUntil = h.mAnthemUntil = h.mClamUntil = 0;
		h.mSharpenLeft = 0;
		h.mThunderLeft = 0;
		h.mLeapUntil = 0;
		h.mStacks = 0;
		h.mPos = NearestOpen(h.mArena, h.mPos);
		Burst(h.mArena, h.mPos, 70, LOOK_SPARKLE);
		Sound(h.mArena, SND_WARP, h.mPos);
	}

	///////////////////////////////////////////////////////////////////////////
	// What keeps running
	///////////////////////////////////////////////////////////////////////////
	void Side::StepAbilities(float theDt)
	{
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		std::vector<Target> v;

		// Dashes: Itchy's Lunge (hits the first enemy), Shrapnel's Blast Jump (just flies).
		if (!Elapsed(mNow, h.mDashUntil))
		{
			Vec aNext = h.mPos + h.mDashDir * (h.mDashSpeed * theDt);
			if (SwimmerFits(h.mArena, aNext, d.mRadius))
				h.mPos = aNext;
			else
				h.mDashUntil = mNow;
			if (h.mDashKind == 0 && !h.mDashHit)
			{
				const AbilityDef& a = HeroDefOf(HERO_ITCHY).mAb[AB_Q];
				EnemiesNear(h.mArena, h.mPos, a.mRadius, v, true, false);
				if (!v.empty())
				{
					const Target& t = v.front();
					AbilityHit(t, MakeHit(a.mDamage * AbilityScale(AB_Q), SRC_ABILITY, AB_Q));
					h.mDashHit = true;
					h.mDashUntil = mNow;
					if (HasTalent(0, 0) && t.mHpFrac < 0.3f && !t.mStructure)
						h.mReadyAt[AB_Q] = mNow + 200;		// Bloodrush
					Event e;
					e.mType = EV_SLASH;
					e.mArena = (uint8_t)h.mArena;
					e.mA = t.mPos;
					e.mB = h.mPos;
					e.mParam = (uint8_t)h.Look();
					Emit(e);
					Sound(h.mArena, SND_SLASH, t.mPos);
					if (t.mRef.mKind == ENT_HERO || t.mRef.mKind == ENT_MINION)
						h.mAutoTarget = t.mRef;			// keep fighting what you hit
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
				const AbilityDef& a = HeroDefOf(HERO_RHUBARB).mAb[AB_Q];
				EnemiesNear(h.mArena, h.mPos, a.mRadius, v, true);
				for (const Target& o : v)
				{
					Hit hit = MakeHit(a.mDamage * AbilityScale(AB_Q), SRC_ABILITY, AB_Q);
					if (!o.mStructure)
						hit.mStunMs = (uint16_t)Ms(HasTalent(0, 0) ? 1.5f : a.mExtra);
					AbilityHit(o, hit);
				}
				Burst(h.mArena, h.mPos, a.mRadius, LOOK_LEAP);
				Sound(h.mArena, SND_EXPLODE, h.mPos);
			}
		}

		// Itchy's Swordstorm (Whirlpool: pulls enemies in).
		if (!Elapsed(mNow, h.mStormUntil) && Elapsed(mNow, h.mStormNext))
		{
			const AbilityDef& a = HeroDefOf(HERO_ITCHY).mAb[AB_R];
			h.mStormNext = mNow + Ms(a.mExtra);
			EnemiesNear(h.mArena, h.mPos, a.mRadius, v, true);
			for (const Target& o : v)
			{
				Hit hit = MakeHit(a.mDamage * AbilityScale(AB_R), SRC_ABILITY, AB_R);
				if (HasTalent(1, 0) && !o.mStructure)
					hit.mPush = Norm(h.mPos - o.mPos) * std::min(40.0f, Dist(h.mPos, o.mPos) * 0.4f);
				AbilityHit(o, hit);
			}
			if (!v.empty())
				Sound(h.mArena, SND_SLASH, h.mPos);
		}

		// Clyde's Thunderstorm: every enemy in the tank, a few times.
		if (h.mThunderLeft > 0 && Elapsed(mNow, h.mThunderNext))
		{
			const AbilityDef& a = HeroDefOf(HERO_CLYDE).mAb[AB_R];
			h.mThunderLeft--;
			h.mThunderNext = mNow + Ms(a.mDurationS / a.mExtra);
			std::vector<Target> all;
			Targets(h.mArena, all);
			for (const Target& o : all)
			{
				if (o.mTeam == mTeam || o.mStructure || (o.mMonster && !o.mAngry))
					continue;
				float aVsFish = o.mRef.mKind == ENT_FISH ? 0.25f : 1.0f;	// thins a farm, doesn't erase it
				Hit hit = MakeHit(a.mDamage * AbilityScale(AB_R) * aVsFish, SRC_ABILITY, AB_R);
				if (HasTalent(1, 1))
					hit.mStunMs = 300;
				Deal(o.mRef, hit);
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
			const AbilityDef& a = HeroDefOf(HERO_SPEEDY).mAb[AB_Q];
			h.mLastSlime = h.mPos;
			Zone z;
			z.mArena = h.mArena;
			z.mPos = h.mPos + Vec(0, d.mRadius * 0.6f);
			z.mRadius = a.mRadius;
			z.mUntil = mNow + Ms(a.mDurationS);
			z.mSlowPct = HasTalent(0, 0) ? 0.7f : a.mExtra;
			z.mDps = (HasTalent(0, 1) ? 25.0f : 10.0f) * AbilityScale(AB_Q);
			z.mLook = LOOK_SLIME;
			z.mAbility = AB_Q;
			mZones.push_back(z);
			Event e;
			e.mType = EV_ZONE;
			e.mArena = (uint8_t)z.mArena;
			e.mA = z.mPos;
			e.mValue = z.mRadius;
			e.mMs = (uint16_t)Ms(a.mDurationS);
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

		// Niko's Fortress: pearls at every enemy near (up to three a volley).
		if (Rooted() && Elapsed(mNow, h.mFortressNext))
		{
			const AbilityDef& a = HeroDefOf(HERO_NIKO).mAb[AB_R];
			h.mFortressNext = mNow + Ms(a.mExtra);
			EnemiesNear(h.mArena, h.mPos, a.mRadius, v, true, false);
			int n = 0;
			for (const Target& o : v)
			{
				if (n >= 3 || o.mRef.mKind == ENT_FISH)
					continue;
				n++;
				Projectile p;
				p.mId = ((uint32_t)mPlayer << 24) | (mNextId++ & 0xFFFFFF);
				p.mArena = h.mArena;
				p.mPos = h.mPos - Vec(0, 30);
				p.mVel = Norm(o.mPos - p.mPos) * 700;
				p.mLeft = a.mRadius * 2;
				p.mRadius = 12;
				p.mHit = MakeHit(a.mDamage * AbilityScale(AB_R), SRC_ABILITY, AB_R);
				p.mHoming = o.mRef;
				p.mLook = LOOK_PEARL;
				p.mStopAtWalls = false;
				mProj.push_back(p);
				Event e;
				e.mType = EV_PROJECTILE;
				e.mArena = (uint8_t)p.mArena;
				e.mId = p.mId;
				e.mA = p.mPos;
				e.mB = p.mVel;
				e.mMs = (uint16_t)std::min(3000.0f, Dist(o.mPos, p.mPos) / 700 * 1000 + 150);
				e.mParam = LOOK_PEARL;
				Emit(e);
			}
			if (n > 0)
				Sound(h.mArena, SND_TOWER, h.mPos);
		}
	}

	// Angie's friendly aim: an own tower or core near the mouse (in her own tank), else herself.
	static EntityRef FriendNear(const Side& s, Vec theAim, float theRange, Vec& thePos)
	{
		const HeroState& h = s.mHero;
		thePos = h.mPos;
		if (h.mArena != s.mTeam)
			return EntityRef::Hero(s.mPlayer);
		const MapDef& m = TankMap();
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

	///////////////////////////////////////////////////////////////////////////
	// Casting
	///////////////////////////////////////////////////////////////////////////
	bool Side::Cast(int theSlot, Vec theAim)
	{
		if (!CanCast(theSlot, &mLastRefusal))
			return false;
		mLastRefusal.clear();
		HeroState& h = mHero;
		if (theSlot == AB_R && h.mCopyHero >= 0)
		{
			EndCopy();
			return true;
		}
		const HeroDef& d = h.Def();
		const AbilityDef& a = d.mAb[theSlot];
		float aScale = AbilityScale(theSlot);
		int aRank = theSlot == AB_R ? std::max(1, h.mRank[AB_R]) : h.mRank[theSlot];
		float aCooldown = a.mCooldownS;
		// Talents that change a cooldown.
		if (h.mHero == HERO_RHUBARB && theSlot == AB_Q && HasTalent(0, 1)) aCooldown -= 3;
		if (h.mHero == HERO_PRESTO && theSlot == AB_R && HasTalent(1, 1)) aCooldown -= 25;
		if (h.mHero == HERO_PRESTO && theSlot == AB_W && HasTalent(2, 1)) aCooldown = 7;
		if (h.mHero == HERO_SHRAPNEL && theSlot == AB_Q && HasTalent(0, 1)) aCooldown -= 2;
		if (h.mHero == HERO_SHRAPNEL && theSlot == AB_E && HasTalent(2, 1)) aCooldown = 6;
		uint32_t aOldReady = h.mReadyAt[theSlot];
		uint32_t aTotal = Ms(aCooldown * RankCooldown(aRank) * CooldownMult());
		h.mReadyAt[theSlot] = mNow + aTotal;
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
		size_t aEvents = mEffects.size();
		CastSwitch(theSlot, a, theAim, aDir, aPoint, aScale);
		if (!mLastRefusal.empty())
		{
			h.mReadyAt[theSlot] = aOldReady;		// nothing happened (no target): no cooldown
			return false;
		}
		mCastTotal[theSlot] = aTotal;
		if (h.mHero == HERO_PRESTO && h.mCopyHero < 0)
			h.mMisdirectUntil = mNow + 3000;		// Misdirection
		(void)aEvents;
		return true;
	}

	void Side::CastSwitch(int theSlot, const AbilityDef& a, Vec theAim, Vec aDir, Vec aPoint, float aScale)
	{
		HeroState& h = mHero;
		const HeroDef& d = h.Def();
		int A = h.mArena;
		std::vector<Target> v;

		auto AddZone = [&](Vec thePos, float theRadius, float theDps, float theSlow, bool theBlind, uint8_t theLook) {
			Zone z;
			z.mArena = A;
			z.mPos = thePos;
			z.mRadius = theRadius;
			z.mUntil = mNow + Ms(a.mDurationS);
			z.mNextTick = mNow;
			z.mDps = theDps;
			z.mSlowPct = theSlow;
			z.mBlind = theBlind;
			z.mLook = theLook;
			z.mAbility = (uint8_t)theSlot;
			mZones.push_back(z);
			Event e;
			e.mType = EV_ZONE;
			e.mArena = (uint8_t)A;
			e.mA = thePos;
			e.mValue = theRadius;
			e.mMs = (uint16_t)Ms(a.mDurationS);
			e.mParam = theLook;
			Emit(e);
		};
		auto Shot = [&](Vec theFrom, Vec theVel, float theReach, float theRadius, const Hit& theHit, uint8_t theLook) -> Projectile& {
			Projectile p;
			p.mId = ((uint32_t)mPlayer << 24) | (mNextId++ & 0xFFFFFF);
			p.mArena = A;
			p.mPos = theFrom;
			p.mVel = theVel;
			p.mLeft = theReach;
			p.mRadius = theRadius;
			p.mHit = theHit;
			p.mLook = theLook;
			mProj.push_back(p);
			Event e;
			e.mType = EV_PROJECTILE;
			e.mArena = (uint8_t)A;
			e.mId = p.mId;
			e.mA = theFrom;
			e.mB = theVel;
			e.mMs = (uint16_t)std::min(5000.0f, theReach / std::max(1.0f, Len(theVel)) * 1000);
			e.mParam = theLook;
			Emit(e);
			return mProj.back();
		};
		auto Lob = [&](Vec theTo, uint32_t theFlightMs, float theRadius, const Hit& theHit, float theStructMult, uint8_t theLook) {
			Impact im;
			im.mArena = A;
			im.mPos = theTo;
			im.mAt = mNow + theFlightMs;
			im.mRadius = theRadius;
			im.mHit = theHit;
			im.mStructMult = theStructMult;
			im.mLook = theLook;
			im.mSound = SND_BOOM;
			mImpacts.push_back(im);
			Event e;
			e.mType = EV_LOB;
			e.mArena = (uint8_t)A;
			e.mA = h.mPos;
			e.mB = theTo;
			e.mMs = (uint16_t)theFlightMs;
			e.mParam = theLook;
			e.mValue = theRadius;
			Emit(e);
		};

		switch (h.Look() * AB_COUNT + theSlot)
		{
		// ---- Itchy ----
		case HERO_ITCHY * AB_COUNT + AB_Q:
		{
			float aReach = a.mRange * (HasTalent(0, 1) ? 1.6f : 1.0f);
			h.mDashUntil = mNow + Ms(a.mDurationS);
			h.mDashDir = aDir;
			h.mDashSpeed = aReach / a.mDurationS;
			h.mDashHit = false;
			h.mDashKind = 0;
			h.mPath.clear();
			Burst(A, h.mPos, 30, LOOK_LUNGE);
			Sound(A, SND_SLASH, h.mPos);
			break;
		}
		case HERO_ITCHY * AB_COUNT + AB_W:
			h.mSharpenLeft = (int)a.mExtra;
			h.mSharpenUntil = mNow + Ms(a.mDurationS);
			Text(A, h.mPos - Vec(0, 56), "Sharpened!", TC_INFO);
			Sound(A, SND_SHIELD, h.mPos);
			break;
		case HERO_ITCHY * AB_COUNT + AB_E:
			h.mUntargetableUntil = mNow + Ms(a.mDurationS);
			h.mSpeedUntil = mNow + Ms(a.mExtra);
			h.mSpeedPct = a.mDamage;
			if (HasTalent(2, 1))
				h.mHp = std::min(MaxHp(), h.mHp + MaxHp() * 0.15f);	// Slippery
			Burst(A, h.mPos, 40, LOOK_TELEPORT);
			Sound(A, SND_WARP, h.mPos);
			break;
		case HERO_ITCHY * AB_COUNT + AB_R:
			h.mStormUntil = mNow + Ms(HasTalent(1, 1) ? 5.0f : a.mDurationS);
			h.mStormNext = mNow;
			Burst(A, h.mPos, a.mRadius, LOOK_STORM);
			Sound(A, SND_ROAR, h.mPos);
			break;

		// ---- Clyde ----
		case HERO_CLYDE * AB_COUNT + AB_Q:
		{
			float k = HasTalent(0, 1) ? 1.4f : 1.0f;
			Projectile& p = Shot(h.mPos, aDir * a.mExtra * k, a.mRange * k, a.mRadius, MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_Q), LOOK_ZAP);
			p.mOverload = HasTalent(0, 0);
			Sound(A, SND_ZAP, h.mPos);
			break;
		}
		case HERO_CLYDE * AB_COUNT + AB_W:
			AddZone(aPoint, a.mRadius, a.mDamage * aScale, a.mExtra, false, LOOK_STATIC);
			Sound(A, SND_ZAP, aPoint);
			break;
		case HERO_CLYDE * AB_COUNT + AB_E:
		{
			Vec aFrom = h.mPos;
			Vec aTo = SwimmerFits(A, aPoint, d.mRadius) ? aPoint : NearestOpen(A, aPoint);
			Burst(A, h.mPos, 36, LOOK_TELEPORT);
			h.mPos = aTo;
			h.mPath.clear();
			if (h.mOrder == HeroState::ORD_MOVE)
				h.mOrder = HeroState::ORD_IDLE;
			Burst(A, h.mPos, 36, LOOK_TELEPORT);
			Sound(A, SND_WARP, h.mPos);
			if (HasTalent(2, 0))
			{
				// Blink Field: a Static Field where he was.
				const AbilityDef& f = d.mAb[AB_W];
				Zone z;
				z.mArena = A;
				z.mPos = aFrom;
				z.mRadius = f.mRadius;
				z.mUntil = mNow + Ms(f.mDurationS);
				z.mNextTick = mNow;
				z.mDps = f.mDamage * AbilityScale(AB_W);
				z.mSlowPct = f.mExtra;
				z.mLook = LOOK_STATIC;
				z.mAbility = AB_E;
				mZones.push_back(z);
				Event e;
				e.mType = EV_ZONE;
				e.mArena = (uint8_t)A;
				e.mA = aFrom;
				e.mValue = f.mRadius;
				e.mMs = (uint16_t)Ms(f.mDurationS);
				e.mParam = LOOK_STATIC;
				Emit(e);
			}
			break;
		}
		case HERO_CLYDE * AB_COUNT + AB_R:
			h.mThunderLeft = HasTalent(1, 0) ? 6 : (int)a.mExtra;
			h.mThunderNext = mNow;
			Text(A, h.mPos - Vec(0, 56), "Thunderstorm!", TC_INFO);
			break;

		// ---- Rhubarb ----
		case HERO_RHUBARB * AB_COUNT + AB_Q:
			h.mLeapFrom = h.mPos;
			h.mLeapTo = WalkerPos(A, Clamp(theAim.x, h.mPos.x - a.mRange, h.mPos.x + a.mRange), d.mRadius);
			h.mLeapStart = mNow;
			h.mLeapUntil = mNow + Ms(a.mDurationS);
			h.mHopUntil = 0;
			h.mOrder = HeroState::ORD_IDLE;
			Sound(A, SND_ROAR, h.mPos);
			break;
		case HERO_RHUBARB * AB_COUNT + AB_W:
		{
			// The first enemy along the line gets pulled in.
			std::vector<Target> all;
			Targets(A, all);
			Vec aEnd = h.mPos + aDir * a.mRange;
			const Target* aBest = nullptr;
			float aBestD = 1e9f;
			for (const Target& o : all)
			{
				if (o.mTeam == mTeam || o.mStructure || o.mRef.mKind == ENT_FISH || o.mMonster)
					continue;
				if (DistPointSeg(o.mPos, h.mPos, aEnd) > a.mRadius + o.mRadius)
					continue;
				float dd = Dist(o.mPos, h.mPos);
				if (dd < aBestD && FirstBlock(A, h.mPos, o.mPos) > 1)
				{
					aBestD = dd;
					aBest = &o;
				}
			}
			Event e;
			e.mType = EV_BEAM;
			e.mArena = (uint8_t)A;
			e.mA = h.mPos;
			e.mB = aBest != nullptr ? aBest->mPos : aEnd;
			e.mMs = 300;
			e.mParam = 1;						// claw
			Emit(e);
			if (aBest != nullptr)
			{
				Hit hit = MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_W);
				hit.mPull = true;
				hit.mPullTo = h.mPos + aDir * (d.mRadius + aBest->mRadius + 8);
				if (HasTalent(2, 1))
					hit.mStunMs = 1000;			// Iron Claw
				Deal(aBest->mRef, hit);
				h.mOrder = HeroState::ORD_ATTACK;
				h.mTarget = aBest->mRef;
				h.mAutoTarget = aBest->mRef;
			}
			Sound(A, SND_CHOMP, h.mPos);
			break;
		}
		case HERO_RHUBARB * AB_COUNT + AB_E:
			h.mImmuneUntil = mNow + Ms(a.mDurationS);
			h.mTauntUntil = h.mImmuneUntil;
			if (HasTalent(2, 0))
				h.mSpikedUntil = h.mImmuneUntil;	// Spiked Shell
			Burst(A, h.mPos, 50, LOOK_HALO);
			Sound(A, SND_SHIELD, h.mPos);
			break;
		case HERO_RHUBARB * AB_COUNT + AB_R:
		{
			float r = a.mRadius * (HasTalent(1, 1) ? 1.4f : 1.0f);
			EnemiesNear(A, h.mPos, r, v, true);
			for (const Target& o : v)
			{
				Hit hit = MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_R);
				if (!o.mStructure)
				{
					hit.mStunMs = (uint16_t)Ms(a.mExtra);
					hit.mPush = Norm(o.mPos - h.mPos) * 110;
				}
				AbilityHit(o, hit);
			}
			if (HasTalent(1, 0))
			{
				// Aftershock: again, at half, a second later.
				Impact im;
				im.mArena = A;
				im.mPos = h.mPos;
				im.mAt = mNow + 1000;
				im.mRadius = r;
				im.mHit = MakeHit(a.mDamage * aScale * 0.5f, SRC_ABILITY, AB_R);
				im.mHit.mStunMs = 400;
				im.mLook = LOOK_SLAM;
				mImpacts.push_back(im);
			}
			Burst(A, h.mPos, r, LOOK_SLAM);
			Sound(A, SND_EXPLODE, h.mPos);
			break;
		}

		// ---- Angie ----
		case HERO_ANGIE * AB_COUNT + AB_Q:
		{
			Vec aAt;
			h.mHealTarget = FriendNear(*this, theAim, a.mRange, aAt);
			h.mHealUntil = mNow + Ms(a.mDurationS);
			h.mHealNext = mNow;
			h.mHealPerTick = a.mDamage * aScale * (HasTalent(0, 0) ? 1.4f : 1.0f) / (a.mDurationS / 0.25f);
			if (HasTalent(2, 1) && h.mHealTarget.mKind == ENT_HERO)
				h.mStunUntil = h.mSleepUntil = h.mSlowUntil = 0;	// Angelic
			Event e;
			e.mType = EV_BEAM;
			e.mArena = (uint8_t)A;
			e.mA = h.mPos;
			e.mB = aAt;
			e.mMs = (uint16_t)Ms(a.mDurationS);
			e.mParam = 0;						// heal
			Emit(e);
			Sound(A, SND_HEAL, aAt);
			break;
		}
		case HERO_ANGIE * AB_COUNT + AB_W:
		{
			Vec aAt;
			EntityRef aWho = FriendNear(*this, theAim, a.mRange, aAt);
			float aLasts = a.mDurationS * (HasTalent(0, 1) ? 2.0f : 1.0f);
			if (aWho.mKind == ENT_HERO)
			{
				h.mShield = std::max(h.mShield, a.mDamage * aScale);
				h.mShieldUntil = mNow + Ms(aLasts);
				if (HasTalent(2, 1))
					h.mStunUntil = h.mSleepUntil = h.mSlowUntil = 0;
			}
			else
			{
				Hit hit;
				hit.mTeam = (uint8_t)mTeam;
				hit.mShield = a.mDamage * aScale;
				hit.mShieldMs = (uint16_t)Ms(aLasts);
				Deal(aWho, hit);
			}
			Burst(A, aAt, 60, LOOK_HALO);
			Sound(A, SND_SHIELD, aAt);
			break;
		}
		case HERO_ANGIE * AB_COUNT + AB_E:
		{
			std::vector<Target> all;
			Targets(A, all);
			for (const Target& o : all)
				if (o.mRef.mKind == ENT_MINION && o.mTeam != mTeam && !o.mMonster && Dist(o.mPos, h.mPos) < a.mRadius + o.mRadius)
				{
					Hit hit;
					hit.mTeam = (uint8_t)mTeam;
					hit.mPlayer = (int8_t)mPlayer;
					hit.mCharmMs = (uint16_t)Ms(HasTalent(2, 0) ? 8.0f : a.mDurationS);
					hit.mCleanse = HasTalent(2, 0);	// Pied Piper: big aliens too
					Deal(o.mRef, hit);
				}
			Burst(A, h.mPos, a.mRadius, LOOK_CHARM);
			Sound(A, SND_HEAL, h.mPos);
			break;
		}
		case HERO_ANGIE * AB_COUNT + AB_R:
		{
			mArena.Revive(HasTalent(1, 1) ? 10 : (int)a.mExtra, mNow);
			for (int i = 0; i < 2; i++)
				if (mArena.mTower[i].mAlive)
				{
					Hit hit;
					hit.mTeam = (uint8_t)mTeam;
					hit.mHeal = a.mDamage * aScale;
					mArena.ApplyHit(EntityRef::Of(mTeam, ENT_TOWER, (uint32_t)i), hit, mNow);
				}
			if (HasTalent(1, 0))
			{
				Hit hit;
				hit.mTeam = (uint8_t)mTeam;
				hit.mHeal = 600;
				mArena.ApplyHit(EntityRef::Of(mTeam, ENT_CORE, 0), hit, mNow);	// Miracle
			}
			h.mHp = MaxHp();
			Burst(A, h.mPos, 120, LOOK_RESURRECT);
			Burst(mTeam, TankMap().mCore, 160, LOOK_RESURRECT);
			Sound(A, SND_HEAL, h.mPos);
			break;
		}

		// ---- Speedy ----
		case HERO_SPEEDY * AB_COUNT + AB_Q:
			h.mSlimeUntil = mNow + Ms(a.mDurationS);
			h.mLastSlime = Vec(-1000, -1000);
			Sound(A, SND_SPLASH, h.mPos);
			break;
		case HERO_SPEEDY * AB_COUNT + AB_W:
			AddZone(aPoint, a.mRadius * (HasTalent(2, 1) ? 1.5f : 1.0f), a.mDamage * aScale, 0, true, LOOK_STINK);
			Sound(A, SND_STINK, aPoint);
			break;
		case HERO_SPEEDY * AB_COUNT + AB_E:
			h.mImmuneUntil = mNow + Ms(a.mDurationS);
			Burst(A, h.mPos, 44, LOOK_HALO);
			Sound(A, SND_SHIELD, h.mPos);
			break;
		case HERO_SPEEDY * AB_COUNT + AB_R:
			h.mGoldRushUntil = mNow + Ms(HasTalent(1, 0) ? 12.0f : a.mDurationS);
			Burst(A, h.mPos, 90, LOOK_GOLD);
			Text(A, h.mPos - Vec(0, 56), "Gold Rush!", TC_MONEY);
			Sound(A, SND_DIAMOND, h.mPos);
			break;

		// ---- Presto ----
		case HERO_PRESTO * AB_COUNT + AB_Q:
		{
			int n = HasTalent(0, 0) ? 5 : 3;
			float aBase = std::atan2(aDir.y, aDir.x);
			for (int i = 0; i < n; i++)
			{
				float an = aBase + (i - (n - 1) * 0.5f) * 0.21f;
				Hit hit = MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_Q);
				if (HasTalent(0, 1))
				{
					hit.mSlowMs = 1500;
					hit.mSlowPct = 0.3f;
				}
				Shot(h.mPos, Vec(std::cos(an), std::sin(an)) * a.mExtra, a.mRange, a.mRadius, hit, LOOK_CARD);
			}
			Sound(A, SND_CARD, h.mPos);
			break;
		}
		case HERO_PRESTO * AB_COUNT + AB_W:
		{
			// Switcheroo: the enemy nearest the mouse (within reach) and Presto trade places.
			std::vector<Target> all;
			Targets(A, all);
			const Target* aBest = nullptr;
			float aBestD = a.mRadius;
			for (const Target& o : all)
			{
				if (o.mTeam == mTeam || o.mStructure || o.mMonster || o.mRef.mKind == ENT_FISH)
					continue;
				if (Dist(o.mPos, h.mPos) > a.mRange + o.mRadius)
					continue;
				float dd = Dist(o.mPos, theAim);
				if (dd < aBestD + o.mRadius)
				{
					aBestD = dd;
					aBest = &o;
				}
			}
			if (aBest == nullptr)
			{
				mLastRefusal = "No enemy near the mouse to swap with.";
				break;
			}
			Vec aMine = h.mPos;
			Burst(A, aMine, 40, LOOK_SPARKLE);
			Burst(A, aBest->mPos, 40, LOOK_SPARKLE);
			h.mPos = d.mWalker ? WalkerPos(A, aBest->mPos.x, d.mRadius) : NearestOpen(A, aBest->mPos);
			h.mPath.clear();
			Hit hit = MakeHit(0, SRC_ABILITY, AB_W);
			hit.mPull = true;
			hit.mPullTo = aMine;
			Deal(aBest->mRef, hit);
			if (HasTalent(2, 1))
				h.mHp = std::min(MaxHp(), h.mHp + 120);	// Bait and Switch
			Sound(A, SND_WARP, h.mPos);
			break;
		}
		case HERO_PRESTO * AB_COUNT + AB_E:
		{
			float aLasts = HasTalent(2, 0) ? 3.5f : a.mDurationS;
			h.mDecoyUntil = mNow + Ms(aLasts);
			h.mSpeedUntil = h.mDecoyUntil;
			h.mSpeedPct = a.mExtra;
			Event e;
			e.mType = EV_DECOY;
			e.mArena = (uint8_t)A;
			e.mPlayer = (int8_t)mTeam;
			e.mId = (uint32_t)h.Look();
			e.mA = h.mPos;
			e.mMs = (uint16_t)Ms(aLasts);
			e.mValue = h.mRight ? 1.0f : 0.0f;
			Emit(e);
			Impact im;
			im.mArena = A;
			im.mPos = h.mPos;
			im.mAt = h.mDecoyUntil;
			im.mRadius = a.mRadius;
			im.mHit = MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_E);
			if (HasTalent(2, 0))
				im.mHit.mStunMs = 800;
			im.mLook = LOOK_SPARKLE;
			im.mSound = SND_BOOM;
			mImpacts.push_back(im);
			Burst(A, h.mPos, 50, LOOK_SPARKLE);
			Sound(A, SND_WARP, h.mPos);
			break;
		}
		case HERO_PRESTO * AB_COUNT + AB_R:
		{
			const HeroSnap* o = OtherHero();
			int aWho = o != nullptr ? o->mHero : HERO_PRESTO;
			float aFrac = h.mHp / std::max(1.0f, MaxHp());
			for (int i = AB_Q; i <= AB_E; i++)
			{
				h.mSavedReady[i] = h.mReadyAt[i];
				h.mReadyAt[i] = mNow;
			}
			h.mCopyHero = (int8_t)aWho;
			h.mCopyUntil = mNow + Ms(HasTalent(1, 0) ? 18.0f : a.mDurationS);
			h.mHp = std::max(1.0f, aFrac * MaxHp());
			if (h.Def().mWalker)
				h.mPos = WalkerPos(A, h.mPos.x, h.Def().mRadius);
			h.mOrder = HeroState::ORD_IDLE;
			Burst(A, h.mPos, 90, LOOK_SPARKLE);
			Text(A, h.mPos - Vec(0, 64), std::string("Copycat: ") + HeroDefOf(aWho).mName + "!", TC_XP);
			Sound(A, SND_EVOLVE, h.mPos);
			break;
		}

		// ---- Niko ----
		case HERO_NIKO * AB_COUNT + AB_Q:
		{
			size_t aMax = HasTalent(0, 0) ? 3 : 2;
			int aMine = 0;
			for (const Turret& t : mTurrets)
				aMine += !t.mMine;
			while (aMine >= (int)aMax)
			{
				for (size_t i = 0; i < mTurrets.size(); i++)
					if (!mTurrets[i].mMine)
					{
						mTurrets[i].mUntil = mNow;	// the oldest goes
						break;
					}
				StepTurrets();
				aMine--;
			}
			Turret t;
			t.mId = mNextId++;
			t.mArena = A;
			t.mPos = WalkerPos(A, aPoint.x, 22);
			t.mUntil = mNow + Ms(a.mDurationS);
			t.mNextShot = mNow + 400;
			mTurrets.push_back(t);
			Event e;
			e.mType = EV_TURRET;
			e.mArena = (uint8_t)A;
			e.mPlayer = (int8_t)mTeam;
			e.mId = t.mId;
			e.mA = t.mPos;
			e.mMs = (uint16_t)Ms(a.mDurationS);
			Emit(e);
			Sound(A, SND_CLAM_OPEN, t.mPos);
			break;
		}
		case HERO_NIKO * AB_COUNT + AB_W:
			h.mClamUntil = mNow + Ms(a.mDurationS);
			h.mClamStun = true;
			if (HasTalent(2, 1) && A == mTeam)
				for (int i = 0; i < 2; i++)
					if (mArena.mTower[i].mAlive)
					{
						Hit hit;
						hit.mTeam = (uint8_t)mTeam;
						hit.mShield = 250;
						hit.mShieldMs = 4000;
						mArena.ApplyHit(EntityRef::Of(mTeam, ENT_TOWER, (uint32_t)i), hit, mNow);	// Mother of Pearl
					}
			Burst(A, h.mPos, 60, LOOK_PEARL);
			Sound(A, SND_CLAM_CLOSE, h.mPos);
			break;
		case HERO_NIKO * AB_COUNT + AB_E:
		{
			Hit hit = MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_E);
			hit.mStunMs = (uint16_t)Ms(a.mExtra);
			Vec aTo = ClampToWater(A, aPoint, 10);
			Lob(aTo, Ms(a.mDurationS), a.mRadius, hit, 1, LOOK_PEARL);
			if (HasTalent(2, 0))
			{
				Hit small = hit;
				small.mDamage *= 0.6f;
				small.mStunMs = 300;
				Lob(ClampToWater(A, aTo + Vec(-90, 0), 10), Ms(a.mDurationS + 0.15f), a.mRadius * 0.8f, small, 1, LOOK_PEARL);
				Lob(ClampToWater(A, aTo + Vec(90, 0), 10), Ms(a.mDurationS + 0.3f), a.mRadius * 0.8f, small, 1, LOOK_PEARL);
			}
			Sound(A, SND_CLAM_OPEN, h.mPos);
			break;
		}
		case HERO_NIKO * AB_COUNT + AB_R:
			h.mFortressUntil = mNow + Ms(HasTalent(1, 0) ? 12.0f : a.mDurationS);
			h.mFortressNext = mNow;
			h.mOrder = HeroState::ORD_IDLE;
			Burst(A, h.mPos, 110, LOOK_FORTRESS);
			Sound(A, SND_CLAM_CLOSE, h.mPos);
			break;

		// ---- Meryl ----
		case HERO_MERYL * AB_COUNT + AB_Q:
		{
			float aHalf = a.mRadius * (HasTalent(0, 1) ? 2.0f : 1.0f) * 3.14159f / 180;
			float aSleep = HasTalent(0, 0) ? 2.4f : a.mDurationS;
			std::vector<Target> all;
			Targets(A, all);
			for (const Target& o : all)
			{
				if (o.mTeam == mTeam || o.mStructure || o.mRef.mKind == ENT_FISH)
					continue;
				Vec dv = o.mPos - h.mPos;
				float l = Len(dv);
				if (l > a.mRange + o.mRadius || l < 1)
					continue;
				float c = Dot(dv * (1 / l), aDir);
				if (c < std::cos(aHalf) && l > o.mRadius + d.mRadius)
					continue;
				Hit hit = MakeHit(0, SRC_ABILITY, AB_Q);
				hit.mSleepMs = (uint16_t)Ms(aSleep);
				Deal(o.mRef, hit);
			}
			Event e;
			e.mType = EV_CONE;
			e.mArena = (uint8_t)A;
			e.mA = h.mPos;
			e.mB = h.mPos + aDir * a.mRange;
			e.mValue = aHalf * 180 / 3.14159f;
			e.mMs = 700;
			e.mParam = LOOK_SLEEP;
			Emit(e);
			Sound(A, SND_SING, h.mPos);
			break;
		}
		case HERO_MERYL * AB_COUNT + AB_W:
		{
			h.mAnthemUntil = mNow + Ms(a.mDurationS);
			if (HasTalent(2, 1))
				h.mHp = std::min(MaxHp(), h.mHp + MaxHp() * 0.2f);	// Standing Ovation
			std::vector<Target> all;
			Targets(A, all);
			for (const Target& o : all)
				if (o.mRef.mKind == ENT_MINION && o.mTeam == mTeam && Dist(o.mPos, h.mPos) < a.mRadius + o.mRadius)
				{
					Hit hit;
					hit.mTeam = (uint8_t)mTeam;
					hit.mPlayer = (int8_t)mPlayer;
					hit.mRallyMs = (uint16_t)Ms(a.mDurationS);
					Deal(o.mRef, hit);
				}
			Burst(A, h.mPos, a.mRadius, LOOK_ANTHEM);
			Sound(A, SND_SING, h.mPos);
			break;
		}
		case HERO_MERYL * AB_COUNT + AB_E:
		{
			Hit hit = MakeHit(a.mDamage * aScale * (HasTalent(2, 0) ? 1.6f : 1.0f), SRC_ABILITY, AB_E);
			hit.mPush = aDir * a.mExtra;
			Projectile& p = Shot(h.mPos, aDir * 900, a.mRange, a.mRadius, hit, LOOK_NOTE);
			p.mPierce = true;
			p.mStopAtWalls = false;
			Event e;
			e.mType = EV_LINE;
			e.mArena = (uint8_t)A;
			e.mA = h.mPos;
			e.mB = h.mPos + aDir * a.mRange;
			e.mValue = a.mRadius;
			e.mMs = (uint16_t)(a.mRange / 900 * 1000);
			e.mParam = LOOK_NOTE;
			Emit(e);
			Sound(A, SND_TONE, h.mPos);
			break;
		}
		case HERO_MERYL * AB_COUNT + AB_R:
		{
			EnemiesNear(A, h.mPos, a.mRadius, v, false, false);
			for (const Target& o : v)
			{
				if (o.mRef.mKind == ENT_FISH)
					continue;
				Hit hit = MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_R);
				hit.mPull = true;
				hit.mPullTo = h.mPos + Norm(o.mPos - h.mPos) * (d.mRadius + o.mRadius + 30);
				if (o.mHero)
					hit.mStunMs = (uint16_t)Ms(HasTalent(1, 1) ? 2.5f : a.mDurationS);
				else
				{
					hit.mCharmMs = (uint16_t)Ms(HasTalent(1, 0) ? 10.0f : a.mExtra);
					hit.mCleanse = true;			// big aliens too
				}
				Deal(o.mRef, hit);
			}
			Burst(A, h.mPos, a.mRadius, LOOK_SIREN);
			Sound(A, SND_SING, h.mPos);
			break;
		}

		// ---- Shrapnel ----
		case HERO_SHRAPNEL * AB_COUNT + AB_Q:
		{
			float r = a.mRadius * (HasTalent(0, 0) ? 1.4f : 1.0f);
			float aFlight = HasTalent(0, 1) ? 0.45f : a.mDurationS;
			Lob(ClampToWater(A, aPoint, 8), Ms(aFlight), r, MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_Q), 1, LOOK_BOMB);
			Sound(A, SND_MISSILE, h.mPos);
			break;
		}
		case HERO_SHRAPNEL * AB_COUNT + AB_W:
		{
			size_t aMax = HasTalent(2, 0) ? 6 : 3;
			size_t aMines = 0;
			for (const Turret& t : mTurrets)
				aMines += t.mMine;
			if (aMines >= aMax)
				for (Turret& t : mTurrets)
					if (t.mMine)
					{
						t.mUntil = mNow;			// the oldest goes
						break;
					}
			Turret t;
			t.mId = mNextId++;
			t.mArena = A;
			t.mPos = NearestOpen(A, aPoint);
			t.mUntil = mNow + Ms(a.mDurationS);
			t.mNextShot = mNow + 1000;				// armed in a second
			t.mMine = true;
			mTurrets.push_back(t);
			Event e;
			e.mType = EV_TURRET;
			e.mArena = (uint8_t)A;
			e.mPlayer = (int8_t)mTeam;
			e.mParam = 1;							// a mine: only Shrapnel sees it
			e.mId = t.mId;
			e.mA = t.mPos;
			e.mMs = (uint16_t)Ms(a.mDurationS);
			Local(e);
			Sound(A, SND_SPLASH, t.mPos);
			break;
		}
		case HERO_SHRAPNEL * AB_COUNT + AB_E:
		{
			Impact im;
			im.mArena = A;
			im.mPos = h.mPos;
			im.mAt = mNow;
			im.mRadius = a.mRadius;
			im.mHit = MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_E);
			im.mHit.mPush = Vec(a.mExtra, 0);		// pointed away from the blast on impact
			im.mLook = LOOK_BOMB;
			im.mSound = SND_BOOM;
			mImpacts.push_back(im);
			float aReach = a.mRange * (HasTalent(2, 1) ? 2.0f : 1.0f);
			h.mDashUntil = mNow + Ms(a.mDurationS);
			h.mDashDir = aDir;
			h.mDashSpeed = aReach / a.mDurationS;
			h.mDashHit = true;
			h.mDashKind = 1;
			h.mPath.clear();
			h.mOrder = HeroState::ORD_IDLE;
			break;
		}
		case HERO_SHRAPNEL * AB_COUNT + AB_R:
		{
			int n = HasTalent(1, 0) ? 14 : (int)a.mExtra;
			float aStruct = HasTalent(1, 1) ? 3.0f : 2.0f;
			Event z;
			z.mType = EV_ZONE;
			z.mArena = (uint8_t)A;
			z.mA = aPoint;
			z.mValue = a.mRadius;
			z.mMs = (uint16_t)Ms(1.0f + a.mDurationS);
			z.mParam = LOOK_MISSILE;
			Emit(z);
			for (int i = 0; i < n; i++)
			{
				float an = mArena.mRng.Range(0, 6.2832f), r = std::sqrt(mArena.mRng.Float()) * a.mRadius;
				Vec p = ClampToWater(A, aPoint + Vec(std::cos(an), std::sin(an)) * r, 8);
				uint32_t aAt = Ms(1.0f + a.mDurationS * i / std::max(1, n - 1));
				Impact im;
				im.mArena = A;
				im.mPos = p;
				im.mAt = mNow + aAt;
				im.mRadius = 70;
				im.mHit = MakeHit(a.mDamage * aScale, SRC_ABILITY, AB_R);
				im.mStructMult = aStruct;
				im.mLook = LOOK_MISSILE;
				im.mSound = SND_BOOM;
				mImpacts.push_back(im);
				Event e;
				e.mType = EV_LOB;
				e.mArena = (uint8_t)A;
				e.mA = Vec(p.x + 120, -60);
				e.mB = p;
				e.mMs = (uint16_t)aAt;
				e.mParam = LOOK_MISSILE;
				e.mValue = 70;
				Emit(e);
			}
			Text(A, h.mPos - Vec(0, 60), "Missiles away!", TC_INFO);
			Sound(A, SND_MISSILE, h.mPos);
			break;
		}
		default:
			break;
		}
	}
}
