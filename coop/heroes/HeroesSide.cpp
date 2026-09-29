#include "HeroesSide.h"
#include "HeroesMap.h"

namespace Heroes
{
	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }

	///////////////////////////////////////////////////////////////////////////
	// Mirror
	///////////////////////////////////////////////////////////////////////////
	void Mirror::AddArena(uint32_t theNow, const ArenaSnap& theSnap)
	{
		mArena.push_back({ theNow, theSnap });
		while (mArena.size() > 4)
			mArena.pop_front();
	}

	void Mirror::AddHero(uint32_t theNow, const HeroSnap& theSnap)
	{
		std::deque<TimedHero>& q = mHero[theSnap.mPlayer % kMaxPlayers];
		q.push_back({ theNow, theSnap });
		while (q.size() > 6)
			q.pop_front();
	}

	const HeroSnap* Mirror::LatestHero(int thePlayer) const
	{
		const std::deque<TimedHero>& q = mHero[thePlayer % kMaxPlayers];
		return q.empty() ? nullptr : &q.back().mSnap;
	}

	template<class T>
	static void Blend(std::vector<T>& theOut, const std::vector<T>& theOld, float t)
	{
		// Entities in both snapshots move smoothly; new ones pop in where they are.
		for (T& e : theOut)
			for (const T& o : theOld)
				if (o.mId == e.mId)
				{
					if (Dist2(o.mPos, e.mPos) < 200 * 200)
						e.mPos = Lerp(o.mPos, e.mPos, t);
					break;
				}
	}

	ArenaSnap Mirror::ArenaAt(uint32_t theTime) const
	{
		if (mArena.empty())
			return ArenaSnap();
		// The latest snapshot at or before theTime, and the one after it.
		size_t b = mArena.size() - 1;
		while (b > 0 && (int32_t)(mArena[b - 1].mAt - theTime) >= 0)
			b--;
		if (b == 0)
			return mArena.front().mSnap;
		const TimedArena& aOld = mArena[b - 1];
		const TimedArena& aNew = mArena[b];
		uint32_t aSpan = std::max<uint32_t>(1, aNew.mAt - aOld.mAt);
		float t = Clamp((float)(int32_t)(theTime - aOld.mAt) / aSpan, 0, 1);
		ArenaSnap s = aNew.mSnap;
		Blend(s.mFish, aOld.mSnap.mFish, t);
		Blend(s.mCoins, aOld.mSnap.mCoins, t);
		Blend(s.mFood, aOld.mSnap.mFood, t);
		Blend(s.mMinions, aOld.mSnap.mMinions, t);
		return s;
	}

	bool Mirror::HeroAt(int thePlayer, uint32_t theTime, HeroSnap& theOut) const
	{
		const std::deque<TimedHero>& q = mHero[thePlayer % kMaxPlayers];
		if (q.empty())
			return false;
		size_t b = q.size() - 1;
		while (b > 0 && (int32_t)(q[b - 1].mAt - theTime) >= 0)
			b--;
		theOut = q[b].mSnap;
		if (b == 0)
			return true;
		const TimedHero& aOld = q[b - 1];
		uint32_t aSpan = std::max<uint32_t>(1, q[b].mAt - aOld.mAt);
		float t = Clamp((float)(int32_t)(theTime - aOld.mAt) / aSpan, 0, 1);
		if (aOld.mSnap.mArena == theOut.mArena && Dist2(aOld.mSnap.mPos, theOut.mPos) < 300 * 300)
			theOut.mPos = Lerp(aOld.mSnap.mPos, theOut.mPos, t);
		return true;
	}

	///////////////////////////////////////////////////////////////////////////
	// Setup
	///////////////////////////////////////////////////////////////////////////
	Vec Side::SpawnPoint() const
	{
		const HeroDef& d = mHero.Def();
		if (d.mWalker)
			return WalkerPos(TheMap().mCore.x - 110, d.mRadius);
		return TheMap().mCore - Vec(0, 150);
	}

	void Side::Init(int theTeam, int thePlayer, int theHero, int theOtherPlayer, uint64_t theSeed, uint32_t theNow)
	{
		Link* aLink = mLink;
		*this = Side();
		mLink = aLink;
		mTeam = theTeam;
		mPlayer = thePlayer;
		mOtherPlayer = theOtherPlayer;
		mNow = mStart = mLastStep = theNow;
		mNextId = 1;
		mArena.Init(theTeam, theSeed, theNow);
		mHero = HeroState();
		mHero.mHero = (uint8_t)std::clamp(theHero, 0, (int)HERO_COUNT - 1);
		mHero.mPlayer = thePlayer;
		mHero.mTeam = theTeam;
		mHero.mArena = theTeam;
		mHero.mPos = SpawnPoint();
		mHero.mHp = MaxHp();
		mNextWave = theNow + (uint32_t)(kWaveFirstS * 1000);
	}

	///////////////////////////////////////////////////////////////////////////
	// Hero numbers
	///////////////////////////////////////////////////////////////////////////
	template<class F>
	static float SumItems(const HeroState& h, F theField)
	{
		float s = 0;
		for (uint8_t i : h.mItems)
			if (i != ITEM_NONE)
				s += theField(ItemDefOf(i));
		return s;
	}

	float Side::MaxHp() const { return mHero.Def().mHealth * LevelScale(mHero.mLevel) + SumItems(mHero, [](const ItemDef& d) { return d.mHealth; }); }
	float Side::Damage() const { return mHero.Def().mDamage * LevelScale(mHero.mLevel) + SumItems(mHero, [](const ItemDef& d) { return d.mDamage; }); }
	float Side::Speed() const
	{
		float s = mHero.Def().mSpeed * (1.0f + SumItems(mHero, [](const ItemDef& d) { return d.mSpeedPct; }));
		if (!Elapsed(mNow, mHero.mSpeedUntil))
			s *= 1.0f + mHero.mSpeedPct;
		if (!Elapsed(mNow, mHero.mSlowUntil))
			s *= 1.0f - mHero.mSlowPct;
		return s;
	}
	float Side::AttackPeriod() const
	{
		float p = mHero.Def().mAttackS;
		if (mHero.mHero == HERO_ITCHY)
			p /= 1.0f + 0.08f * mHero.mStacks;
		return p;
	}
	float Side::CooldownMult() const { return 1.0f - std::min(0.4f, SumItems(mHero, [](const ItemDef& d) { return d.mCdrPct; })); }
	float Side::AbilityScale(int theSlot) const
	{
		int aRank = theSlot == AB_R ? std::max(1, mHero.mRank[AB_R]) : mHero.mRank[theSlot];
		return RankScale(aRank) * (1.0f + 0.04f * (mHero.mLevel - 1));
	}
	float Side::ArmorVsStructures() const
	{
		float a = 1.0f - std::min(0.5f, SumItems(mHero, [](const ItemDef& d) { return d.mArmorPct; }));
		if (mHero.mHero == HERO_RHUBARB)
			a *= 0.7f;
		return a;
	}
	float Side::StructBonus() const { return 1.0f + SumItems(mHero, [](const ItemDef& d) { return d.mStructPct; }) + (mHero.Def().mWalker ? 0.25f : 0.0f); }
	float Side::Lifesteal() const { return SumItems(mHero, [](const ItemDef& d) { return d.mLifestealPct; }); }
	uint32_t Side::CooldownLeft(int theSlot) const { return Elapsed(mNow, mHero.mReadyAt[theSlot]) ? 0 : mHero.mReadyAt[theSlot] - mNow; }
	bool Side::Busy() const { return !Elapsed(mNow, mHero.mDashUntil) || !Elapsed(mNow, mHero.mLeapUntil); }

	///////////////////////////////////////////////////////////////////////////
	// Step
	///////////////////////////////////////////////////////////////////////////
	void Side::Step(uint32_t theNow)
	{
		float dt = std::min(0.1f, (theNow - mLastStep) / 1000.0f);
		mLastStep = theNow;
		mNow = theNow;
		Packet p;
		while (mLink != nullptr && mLink->Receive(p))
			Receive(p);
		for (size_t i = 0; i < mEffects.size();)
			if (Elapsed(mNow, mEffects[i].mAt + 1500 + mEffects[i].mEvent.mMs))
				mEffects.erase(mEffects.begin() + i);
			else
				i++;
		if (Over())
			return;
		mSuddenDeath = MatchMs() >= (uint32_t)(kSuddenDeathS * 1000);

		StepHero(dt);
		StepProjectiles(dt);
		StepZones();
		StepArena();

		// Passive income: items, and Stinky's Scavenger.
		float aGold = SumItems(mHero, [](const ItemDef& d) { return d.mGoldPerS; }) + (mHero.mHero == HERO_STINKY ? 1.0f : 0.0f);
		mStinkyGold += aGold * dt;
		if (mStinkyGold >= 1)
		{
			int n = (int)mStinkyGold;
			mStinkyGold -= n;
			mArena.Earn(n, Vec(), false);
		}
		if (mHero.mAlive)
			GainXp(kXpPerSecond * dt);
		StepWaves();

		if (mArena.mCoreDead && !mLost)
		{
			mLost = true;
			Writer w;
			w.U8(ST_CORE_DEAD);
			if (mLink != nullptr)
				mLink->Send(HM_STATUS, w.mData);
		}
		SendState();
	}

	void Side::StepArena()
	{
		ArenaContext c;
		c.mOwnerPlayer = mPlayer;
		c.mOwnerHeroHere = mHero.mAlive && mHero.mArena == mTeam;
		c.mOwnerHeroPos = mHero.mPos;
		c.mScavenger = mHero.mHero == HERO_STINKY;
		c.mGoldRush = mHero.mHero == HERO_STINKY && !Elapsed(mNow, mHero.mGoldRushUntil);
		c.mGrace = mHero.mHero == HERO_ANGIE;
		c.mSuddenDeath = mSuddenDeath;
		if (c.mOwnerHeroHere)
		{
			HeroPresence h;
			h.mPlayer = mPlayer;
			h.mTeam = mTeam;
			h.mPos = mHero.mPos;
			h.mRadius = mHero.Def().mRadius;
			h.mTargetable = Elapsed(mNow, mHero.mUntargetableUntil) && Elapsed(mNow, mHero.mImmuneUntil);
			c.mHeroes.push_back(h);
		}
		HeroSnap o;
		if (OtherHeroIn(mTeam, &o) && (o.mFlags & HF_ALIVE))
		{
			HeroPresence h;
			h.mPlayer = o.mPlayer;
			h.mTeam = o.mTeam;
			h.mPos = o.mPos;
			h.mRadius = HeroDefOf(o.mHero).mRadius;
			h.mTargetable = !(o.mFlags & (HF_HIDDEN | HF_UNTARGETABLE | HF_IMMUNE));
			h.mTaunting = (o.mFlags & HF_TAUNT) != 0;
			h.mHurtMyHeroAt = mHurtByAt[o.mPlayer % kMaxPlayers];
			c.mHeroes.push_back(h);
		}
		mArena.Step(mNow, c);

		// Drain what the arena produced.
		for (auto& h : mArena.mOutHits)
			Deal(h.first, h.second);
		mArena.mOutHits.clear();
		for (Reward& r : mArena.mOutRewards)
		{
			if (r.mPlayer == mPlayer || (r.mPlayer < 0 && r.mTeam == mTeam))
				GainReward(r);
			else
				mOutRewards.push_back(r);
		}
		mArena.mOutRewards.clear();
		for (Event& e : mArena.mOutEvents)
			Emit(e);
		mArena.mOutEvents.clear();
	}

	void Side::StepWaves()
	{
		if (!Elapsed(mNow, mNextWave))
			return;
		mNextWave += (uint32_t)(kWaveEveryS * 1000);
		int aExtra = 0;
		float aMult = 1.0f + 0.25f * mWaveToughRank;
		if (mSuddenDeath)
		{
			int aMin = 1 + (int)((MatchMs() - kSuddenDeathS * 1000) / 60000);
			aExtra = aMin;
			aMult *= 1.0f + kSuddenGrowthPerMin * aMin;
		}
		Writer w;
		w.U8((uint8_t)mTeam);
		w.F32(aMult);
		std::vector<uint8_t> aKinds(kWaveSize + mWaveSizeRank + aExtra, MIN_MINI);
		aKinds.insert(aKinds.end(), mQueuedAliens.begin(), mQueuedAliens.end());
		mQueuedAliens.clear();
		w.U8((uint8_t)std::min<size_t>(aKinds.size(), 255));
		for (size_t i = 0; i < aKinds.size() && i < 255; i++)
			w.U8(aKinds[i]);
		if (mLink != nullptr)
			mLink->Send(HM_WAVE, w.mData);
		mWavesSent++;
	}

	void Side::SendState()
	{
		if (mLink == nullptr)
			return;
		if (!mOutHits.empty())
		{
			Writer w;
			w.U8((uint8_t)std::min<size_t>(mOutHits.size(), 255));
			for (size_t i = 0; i < mOutHits.size() && i < 255; i++)
			{
				Put(w, mOutHits[i].first);
				Put(w, mOutHits[i].second);
			}
			mLink->Send(HM_HITS, w.mData);
			mOutHits.clear();
		}
		if (!mOutRewards.empty())
		{
			Writer w;
			w.U8((uint8_t)std::min<size_t>(mOutRewards.size(), 255));
			for (size_t i = 0; i < mOutRewards.size() && i < 255; i++)
				Put(w, mOutRewards[i]);
			mLink->Send(HM_REWARDS, w.mData);
			mOutRewards.clear();
		}
		while (!mOutEvents.empty())
		{
			Writer w;
			size_t n = std::min<size_t>(mOutEvents.size(), 200);
			w.U8((uint8_t)n);
			for (size_t i = 0; i < n; i++)
				Put(w, mOutEvents[i]);
			mLink->Send(HM_EVENTS, w.mData);
			mOutEvents.erase(mOutEvents.begin(), mOutEvents.begin() + n);
		}
		if (Elapsed(mNow, mLastHeroSent + 50))
		{
			mLastHeroSent = mNow;
			Writer w;
			Put(w, MySnap());
			mLink->Send(HM_HERO, w.mData);
		}
		uint32_t aEvery = OtherHeroIn(mTeam) ? 66 : 333;
		if (Elapsed(mNow, mLastArenaSent + aEvery))
		{
			mLastArenaSent = mNow;
			Writer w;
			Put(w, mArena.Snapshot());
			mLink->Send(HM_ARENA, w.mData);
		}
	}

	void Side::Receive(const Packet& p)
	{
		Reader r(p.mData);
		switch (p.mType)
		{
		case HM_HERO:
		{
			HeroSnap s;
			if (Get(r, s) && s.mPlayer != mPlayer)
				mOther.AddHero(mNow, s);
			break;
		}
		case HM_ARENA:
		{
			ArenaSnap s;
			if (Get(r, s) && s.mTeam != mTeam)
				mOther.AddArena(mNow, s);
			break;
		}
		case HM_HITS:
		{
			int n = r.U8();
			for (int i = 0; i < n && !r.mBad; i++)
			{
				EntityRef aRef;
				Hit h;
				if (!Get(r, aRef) || !Get(r, h))
					break;
				if (aRef.mKind == ENT_HERO)
				{
					if ((int)aRef.mId == mPlayer)
						ApplyToMyHero(h);
				}
				else if (aRef.mArena == mTeam)
					mArena.ApplyHit(aRef, h, mNow);
			}
			break;
		}
		case HM_REWARDS:
		{
			int n = r.U8();
			for (int i = 0; i < n && !r.mBad; i++)
			{
				Reward w;
				if (Get(r, w) && (w.mPlayer == mPlayer || (w.mPlayer < 0 && w.mTeam == mTeam)))
					GainReward(w);
			}
			break;
		}
		case HM_EVENTS:
		{
			int n = r.U8();
			for (int i = 0; i < n && !r.mBad; i++)
			{
				Event e;
				if (Get(r, e))
					mEffects.push_back({ e, mNow, ++mEffectSeq });
			}
			break;
		}
		case HM_WAVE:
		{
			int aFrom = r.U8();
			float aMult = r.F32();
			int n = r.U8();
			std::vector<uint8_t> aKinds;
			for (int i = 0; i < n && !r.mBad; i++)
				aKinds.push_back((uint8_t)std::min<int>(r.U8(), MIN_KIND_COUNT - 1));
			if (!r.mBad && aFrom != mTeam && !Over())
				mArena.SpawnWave(aKinds, Clamp(aMult, 0.5f, 10), aFrom, mNow);
			break;
		}
		case HM_STATUS:
		{
			int s = r.U8();
			if (s == ST_CORE_DEAD || s == ST_GAVE_UP)
			{
				if (!mLost)
					mWon = true;
				mOtherGaveUp = s == ST_GAVE_UP;
			}
			break;
		}
		default:
			break;
		}
	}

	void Side::GiveUp()
	{
		if (Over())
			return;
		mLost = true;
		Writer w;
		w.U8(ST_GAVE_UP);
		if (mLink != nullptr)
			mLink->Send(HM_STATUS, w.mData);
	}

	///////////////////////////////////////////////////////////////////////////
	// Damage, rewards, events
	///////////////////////////////////////////////////////////////////////////
	Hit Side::MakeHit(float theDamage, uint8_t theSource) const
	{
		Hit h;
		h.mDamage = theDamage;
		h.mPlayer = (int8_t)mPlayer;
		h.mTeam = (uint8_t)mTeam;
		h.mSource = theSource;
		return h;
	}

	void Side::Deal(const EntityRef& theTarget, const Hit& theIn)
	{
		Hit theHit = theIn;
		if (theHit.mPlayer == mPlayer && theHit.mSource != SRC_LASER)
		{
			if (mHero.mArena == mTeam)
				theHit.mDamage *= kHomeDamage;			// home waters
			if (theTarget.mKind == ENT_FISH)
				theHit.mDamage *= kHeroVsFish;
		}
		if (theTarget.mKind == ENT_HERO)
		{
			if ((int)theTarget.mId == mPlayer)
				ApplyToMyHero(theHit);
			else
			{
				mOutHits.push_back({ theTarget, theHit });
				if (theHit.mPlayer == mPlayer)
					mHero.mHeroDamage += theHit.mDamage;
			}
			return;
		}
		if ((theTarget.mKind == ENT_TOWER || theTarget.mKind == ENT_CORE) && theHit.mPlayer == mPlayer)
			mHero.mStructDamage += theHit.mDamage;
		if (theTarget.mArena == mTeam)
			mArena.ApplyHit(theTarget, theHit, mNow);
		else
			mOutHits.push_back({ theTarget, theHit });
	}

	void Side::ApplyToMyHero(const Hit& theHit)
	{
		HeroState& h = mHero;
		if (!h.mAlive)
			return;
		if (theHit.mHeal > 0)
			h.mHp = std::min(MaxHp(), h.mHp + theHit.mHeal);
		if (theHit.mShield > 0)
		{
			h.mShield = std::max(h.mShield, theHit.mShield);
			h.mShieldUntil = mNow + theHit.mShieldMs;
		}
		if (theHit.mTeam == mTeam)
			return;								// no friendly fire
		if (!Elapsed(mNow, h.mImmuneUntil) || !Elapsed(mNow, h.mUntargetableUntil))
		{
			if (theHit.mDamage > 0)
				Text(h.mArena, h.mPos - Vec(0, 50), "Immune", TC_INFO);
			return;
		}
		if (theHit.mPlayer >= 0)
			mHurtByAt[theHit.mPlayer % kMaxPlayers] = mNow;
		if (theHit.mStunMs > 0)
			h.mStunUntil = std::max(h.mStunUntil, mNow + theHit.mStunMs);
		if (theHit.mSlowMs > 0)
		{
			// The strongest slow still running wins.
			if (Elapsed(mNow, h.mSlowUntil) || theHit.mSlowPct >= h.mSlowPct)
				h.mSlowPct = theHit.mSlowPct;
			h.mSlowUntil = std::max(h.mSlowUntil, mNow + theHit.mSlowMs);
		}
		const HeroDef& d = h.Def();
		if (theHit.mPull)
			h.mPos = d.mWalker ? WalkerPos(theHit.mPullTo.x, d.mRadius) : NearestOpen(theHit.mPullTo);
		if (theHit.mPush.x != 0 || theHit.mPush.y != 0)
		{
			Vec p = h.mPos + theHit.mPush;
			h.mPos = d.mWalker ? WalkerPos(p.x, d.mRadius) : NearestOpen(p);
		}
		if (theHit.mPull || theHit.mPush.x != 0 || theHit.mPush.y != 0)
			h.mPath.clear();
		float aDamage = theHit.mDamage;
		if (aDamage <= 0)
			return;
		if (theHit.mSource == SRC_TOWER || theHit.mSource == SRC_LASER)
			aDamage *= ArmorVsStructures();
		if (h.mArena == mTeam)
			aDamage *= kHomeArmor;						// home waters
		if (h.mShield > 0)
		{
			float a = std::min(h.mShield, aDamage);
			h.mShield -= a;
			aDamage -= a;
		}
		h.mHp -= aDamage;
		if (aDamage >= 1)
			Text(h.mArena, h.mPos - Vec(0, 46), std::to_string((int)std::lround(aDamage)), TC_DAMAGE);
		if (h.mHp <= 0)
			Die(theHit);
	}

	void Side::Die(const Hit& theKiller)
	{
		HeroState& h = mHero;
		h.mHp = 0;
		h.mAlive = false;
		h.mDeaths++;
		h.mRespawnAt = mNow + (uint32_t)(RespawnS(h.mLevel) * 1000);
		h.mOrder = HeroState::ORD_IDLE;
		h.mPath.clear();
		h.mStormUntil = h.mSlimeUntil = h.mHealUntil = h.mDashUntil = h.mLeapUntil = 0;
		h.mThunderLeft = 0;
		Reward r;
		r.mPlayer = theKiller.mPlayer;
		r.mTeam = (int8_t)theKiller.mTeam;
		r.mXp = kXpHeroKill + kXpHeroKillPerLevel * h.mLevel;
		r.mMoney = kBountyHeroKill + kBountyHeroKillPerLevel * h.mLevel;
		r.mWhat = ENT_HERO;
		mOutRewards.push_back(r);
		Event e;
		e.mType = EV_KILL;
		e.mArena = (uint8_t)h.mArena;
		e.mPlayer = theKiller.mPlayer;
		e.mId = (uint32_t)mPlayer;
		e.mA = h.mPos;
		e.mValue = (float)r.mMoney;
		e.mParam = theKiller.mSource;
		Emit(e);
		Sound(h.mArena, SND_DIE, h.mPos);
	}

	void Side::Respawn()
	{
		HeroState& h = mHero;
		h.mAlive = true;
		h.mHp = MaxHp();
		h.mArena = mTeam;
		h.mPos = SpawnPoint();
		h.mOrder = HeroState::ORD_IDLE;
		h.mPath.clear();
		h.mStunUntil = h.mSlowUntil = h.mUntargetableUntil = h.mImmuneUntil = h.mSpeedUntil = h.mTauntUntil = 0;
		h.mShield = 0;
		h.mInPortal = false;
		h.mStacks = 0;
		Event e;
		e.mType = EV_BURST;
		e.mArena = (uint8_t)mTeam;
		e.mA = h.mPos;
		e.mValue = 60;
		e.mParam = LOOK_HALO;
		Emit(e);
	}

	void Side::GainReward(const Reward& r)
	{
		GainXp(r.mXp);
		if (r.mMoney > 0)
		{
			mArena.Earn(r.mMoney, Vec(), false);
			if (mHero.mAlive)
				Text(mHero.mArena, mHero.mPos - Vec(0, 64), "+$" + std::to_string(r.mMoney), TC_MONEY);
		}
		if (r.mWhat == ENT_HERO)
			mHero.mKills++;
		else if (r.mWhat == ENT_MINION)
			mHero.mMinionKills++;
		else if (r.mWhat == ENT_TOWER)
			mHero.mTowers++;
	}

	void Side::GainXp(float theXp)
	{
		HeroState& h = mHero;
		if (h.mLevel >= kMaxLevel || theXp <= 0)
			return;
		h.mXp += theXp;
		while (h.mLevel < kMaxLevel && h.mXp >= XpForLevel(h.mLevel))
		{
			h.mXp -= XpForLevel(h.mLevel);
			float aOldMax = MaxHp();
			h.mLevel++;
			h.mHp += MaxHp() - aOldMax;
			if (h.mLevel == kUltLevel)
				h.mRank[AB_R] = 1;
			else if (h.mLevel == kUlt2Level)
				h.mRank[AB_R] = 2;
			if (h.mPoints == 0)
				h.mPointAt = mNow;
			h.mPoints++;
			Event e;
			e.mType = EV_LEVEL_UP;
			e.mArena = (uint8_t)h.mArena;
			e.mPlayer = (int8_t)mPlayer;
			e.mA = h.mPos;
			e.mValue = (float)h.mLevel;
			Emit(e);
			Sound(h.mArena, SND_LEVEL, h.mPos);
		}
		if (h.mLevel >= kMaxLevel)
			h.mXp = 0;
	}

	bool Side::SpendPoint(int theSlot)
	{
		HeroState& h = mHero;
		if (theSlot < AB_Q || theSlot > AB_E || h.mPoints <= 0 || h.mRank[theSlot] >= kMaxRank)
			return false;
		h.mRank[theSlot]++;
		h.mPoints--;
		h.mPointAt = mNow;
		return true;
	}

	void Side::Emit(const Event& e)
	{
		mEffects.push_back({ e, mNow, ++mEffectSeq });
		mOutEvents.push_back(e);
	}

	void Side::Text(int theArena, Vec theAt, const std::string& theText, uint8_t theColor)
	{
		Event e;
		e.mType = EV_TEXT;
		e.mArena = (uint8_t)theArena;
		e.mA = theAt;
		e.mText = theText;
		e.mParam = theColor;
		Emit(e);
	}

	void Side::Sound(int theArena, uint8_t theSound, Vec theAt)
	{
		Event e;
		e.mType = EV_SOUND;
		e.mArena = (uint8_t)theArena;
		e.mParam = theSound;
		e.mA = theAt;
		Emit(e);
	}

	///////////////////////////////////////////////////////////////////////////
	// What's around
	///////////////////////////////////////////////////////////////////////////
	bool Side::OtherHeroIn(int theArena, HeroSnap* theOut, bool theAsDrawn) const
	{
		const HeroSnap* s = OtherHero();
		if (s == nullptr || s->mArena != theArena)
			return false;
		if (theOut != nullptr)
		{
			*theOut = *s;
			HeroSnap aSmooth;
			if (theAsDrawn && mOther.HeroAt(mOtherPlayer, mNow - Mirror::kDelayMs, aSmooth) && aSmooth.mArena == s->mArena)
				theOut->mPos = aSmooth.mPos;
		}
		return true;
	}

	bool Side::ArenaHasOpenCore(int theArena) const
	{
		if (theArena == mTeam)
			return mArena.CoreOpen();
		const ArenaSnap* s = mOther.LatestArena();
		return s != nullptr && s->mTowerHp[0] <= 0 && s->mTowerHp[1] <= 0;
	}

	void Side::Targets(int theArena, std::vector<Target>& theOut, bool theAsDrawn) const
	{
		theOut.clear();
		const MapDef& aMap = TheMap();
		auto AddHero = [&](const HeroSnap& s) {
			if (!(s.mFlags & HF_ALIVE) || (s.mFlags & (HF_UNTARGETABLE)))
				return;
			if ((s.mFlags & HF_HIDDEN) && s.mTeam != mTeam)
				return;
			Target t;
			t.mRef = EntityRef::Hero(s.mPlayer);
			t.mPos = s.mPos;
			t.mRadius = HeroDefOf(s.mHero).mRadius;
			t.mTeam = s.mTeam;
			t.mHpFrac = s.mHp / std::max(1.0f, s.mMaxHp);
			t.mHero = true;
			theOut.push_back(t);
		};
		ArenaSnap s;
		if (theAsDrawn || theArena == mTeam)
			s = ViewArena(theArena);
		else if (const ArenaSnap* aLatest = mOther.LatestArena())
			s = *aLatest;
		for (const FishSnap& f : s.mFish)
		{
			if (f.mFlags & FF_DYING)
				continue;
			Target t;
			t.mRef = EntityRef::Of(theArena, ENT_FISH, f.mId);
			t.mPos = f.mPos;
			t.mRadius = FishDefOf(f.mKind).mRadius[std::min<int>(f.mSize, 2)];
			t.mTeam = theArena;
			theOut.push_back(t);
		}
		for (const MinionSnap& m : s.mMinions)
		{
			Target t;
			t.mRef = EntityRef::Of(theArena, ENT_MINION, m.mId);
			t.mPos = m.mPos;
			t.mRadius = MinionDefOf(m.mKind).mRadius;
			t.mTeam = m.mTeam;
			t.mHpFrac = m.mHpFrac;
			theOut.push_back(t);
		}
		for (int i = 0; i < 2; i++)
			if (s.mTowerHp[i] > 0)
			{
				Target t;
				t.mRef = EntityRef::Of(theArena, ENT_TOWER, (uint32_t)i);
				t.mPos = aMap.mTower[i];
				t.mRadius = kTowerR;
				t.mTeam = theArena;
				t.mHpFrac = s.mTowerHp[i] / kTowerHealth;
				t.mStructure = true;
				theOut.push_back(t);
			}
		if (s.mCoreHp > 0)
		{
			Target t;
			t.mRef = EntityRef::Of(theArena, ENT_CORE, 0);
			t.mPos = aMap.mCore;
			t.mRadius = kCoreR;
			t.mTeam = theArena;
			t.mHpFrac = s.mCoreHp / kCoreHealth;
			t.mStructure = true;
			theOut.push_back(t);
		}
		if (mHero.mArena == theArena)
			AddHero(MySnap());
		HeroSnap o;
		if (OtherHeroIn(theArena, &o, theAsDrawn))
			AddHero(o);
	}

	bool Side::FindTarget(const EntityRef& theRef, Target& theOut) const
	{
		if (!theRef.Valid())
			return false;
		std::vector<Target> v;
		Targets(theRef.mKind == ENT_HERO ? mHero.mArena : theRef.mArena, v);
		for (const Target& t : v)
			if (t.mRef == theRef)
			{
				theOut = t;
				return true;
			}
		return false;
	}

	void Side::EnemiesNear(int theArena, Vec thePos, float theRadius, std::vector<Target>& theOut, bool theStructures) const
	{
		std::vector<Target> v;
		Targets(theArena, v);
		theOut.clear();
		bool aOpen = ArenaHasOpenCore(theArena);
		for (const Target& t : v)
		{
			if (t.mTeam == mTeam)
				continue;
			if (t.mStructure && (!theStructures || (t.mRef.mKind == ENT_CORE && !aOpen)))
				continue;
			if (Dist(t.mPos, thePos) <= theRadius + t.mRadius)
				theOut.push_back(t);
		}
	}

	HeroSnap Side::MySnap() const
	{
		const HeroState& h = mHero;
		HeroSnap s;
		s.mPlayer = (uint8_t)mPlayer;
		s.mTeam = (uint8_t)mTeam;
		s.mHero = h.mHero;
		s.mArena = (uint8_t)h.mArena;
		uint16_t f = 0;
		if (h.mAlive) f |= HF_ALIVE;
		if (h.mHidden) f |= HF_HIDDEN;
		if (!Elapsed(mNow, h.mUntargetableUntil)) f |= HF_UNTARGETABLE;
		if (!Elapsed(mNow, h.mStunUntil)) f |= HF_STUNNED;
		if (!Elapsed(mNow, h.mImmuneUntil)) f |= HF_IMMUNE;
		if (h.mRight) f |= HF_RIGHT;
		if (!Elapsed(mNow, h.mAttackingUntil)) f |= HF_ATTACKING;
		if (!Elapsed(mNow, h.mStormUntil)) f |= HF_STORM;
		if (!Elapsed(mNow, h.mLeapUntil)) f |= HF_LEAPING;
		if (!Elapsed(mNow, h.mSlowUntil)) f |= HF_SLOWED;
		if (!Elapsed(mNow, h.mGoldRushUntil)) f |= HF_GOLDRUSH;
		if (!Elapsed(mNow, h.mTauntUntil)) f |= HF_TAUNT;
		if (!Elapsed(mNow, h.mSpeedUntil)) f |= HF_SPEED;
		s.mFlags = f;
		s.mPos = h.mPos;
		s.mHp = h.mHp;
		s.mMaxHp = MaxHp();
		s.mShield = h.mShield;
		s.mLevel = (uint8_t)h.mLevel;
		s.mRespawnMs = h.mAlive ? 0 : (uint16_t)std::min<uint32_t>(65000, Elapsed(mNow, h.mRespawnAt) ? 0 : h.mRespawnAt - mNow);
		s.mKills = (uint16_t)h.mKills;
		s.mDeaths = (uint16_t)h.mDeaths;
		for (int i = 0; i < kItemSlots; i++)
			s.mItems[i] = h.mItems[i];
		s.mFishLost = (uint16_t)std::min(65535, mArena.mFishLost);
		s.mTowers = (uint16_t)h.mTowers;
		s.mEarned = (uint32_t)std::max(0, mArena.mMoneyEarned);
		return s;
	}

	ArenaSnap Side::ViewArena(int theArena) const
	{
		if (theArena == mTeam)
			return mArena.Snapshot();
		return mOther.ArenaAt(mNow - Mirror::kDelayMs);
	}

	void Side::ViewHeroes(int theArena, std::vector<HeroSnap>& theOut) const
	{
		theOut.clear();
		if (mHero.mArena == theArena)
			theOut.push_back(MySnap());
		HeroSnap o;
		if (OtherHeroIn(theArena, &o, true) && !(o.mFlags & HF_HIDDEN))
			theOut.push_back(o);
	}

	///////////////////////////////////////////////////////////////////////////
	// The shop
	///////////////////////////////////////////////////////////////////////////
	int Side::Price(int theShop) const
	{
		const Arena& a = mArena;
		switch (theShop)
		{
		case SHOP_GUPPY: return FishDefOf(FISH_GUPPY).mPrice;
		case SHOP_BREEDER: return FishDefOf(FISH_BREEDER).mPrice;
		case SHOP_CARNIVORE: return FishDefOf(FISH_CARNIVORE).mPrice;
		case SHOP_FOOD_QUALITY: return a.mFoodQuality < 2 ? kFoodQualityPrice[a.mFoodQuality] : 0;
		case SHOP_FOOD_COUNT: return kFoodCountPrice;
		case SHOP_LASER: return LaserUpgradePrice(a.mLaserLevel);
		case SHOP_REPAIR_LEFT:
		case SHOP_REPAIR_RIGHT: return TowerRepairPrice();
		case SHOP_TOWER_UPGRADE: return TowerUpgradePrice(a.mTowerLevel);
		case SHOP_WAVE_SIZE: return WaveSizePrice(mWaveSizeRank);
		case SHOP_WAVE_TOUGH: return WaveToughPrice(mWaveToughRank);
		case SHOP_SEND_SYLV: return MinionDefOf(MIN_SYLV).mPrice;
		case SHOP_SEND_GUS: return MinionDefOf(MIN_GUS).mPrice;
		case SHOP_SEND_BALROG: return MinionDefOf(MIN_BALROG).mPrice;
		case SHOP_SEND_DESTRUCTOR: return MinionDefOf(MIN_DESTRUCTOR).mPrice;
		default:
			if (theShop >= SHOP_ITEM_FIRST && theShop <= SHOP_ITEM_LAST)
				return ItemDefOf(theShop - SHOP_ITEM_FIRST).mPrice;
			return 0;
		}
	}

	int Side::Owned(int theShop) const
	{
		switch (theShop)
		{
		case SHOP_FOOD_QUALITY: return mArena.mFoodQuality;
		case SHOP_FOOD_COUNT: return mArena.mPellets;
		case SHOP_LASER: return mArena.mLaserLevel;
		case SHOP_TOWER_UPGRADE: return mArena.mTowerLevel;
		case SHOP_WAVE_SIZE: return mWaveSizeRank;
		case SHOP_WAVE_TOUGH: return mWaveToughRank;
		case SHOP_SEND_SYLV: case SHOP_SEND_GUS: case SHOP_SEND_BALROG: case SHOP_SEND_DESTRUCTOR:
		{
			int k = MIN_SYLV + (theShop - SHOP_SEND_SYLV), n = 0;
			for (uint8_t q : mQueuedAliens)
				if (q == k)
					n++;
			return n;
		}
		default:
			if (theShop >= SHOP_ITEM_FIRST && theShop <= SHOP_ITEM_LAST)
			{
				int n = 0;
				for (uint8_t i : mHero.mItems)
					if (i == theShop - SHOP_ITEM_FIRST)
						n++;
				return n;
			}
			return 0;
		}
	}

	bool Side::CanBuy(int theShop, std::string* theWhy) const
	{
		auto No = [&](const char* s) { if (theWhy != nullptr) *theWhy = s; return false; };
		if (Over())
			return No("The match is over.");
		const Arena& a = mArena;
		switch (theShop)
		{
		case SHOP_GUPPY: case SHOP_BREEDER: case SHOP_CARNIVORE:
			if (a.FishCount() >= kMaxFish) return No("Your tank is full.");
			break;
		case SHOP_FOOD_QUALITY: if (a.mFoodQuality >= 2) return No("Food quality is maxed."); break;
		case SHOP_FOOD_COUNT: if (a.mPellets >= kMaxPellets) return No("Food quantity is maxed."); break;
		case SHOP_LASER: if (a.mLaserLevel >= kLaserLevels - 1) return No("Your laser is maxed."); break;
		case SHOP_REPAIR_LEFT: case SHOP_REPAIR_RIGHT:
		{
			const Tower& t = a.mTower[theShop == SHOP_REPAIR_LEFT ? 0 : 1];
			if (!t.mAlive) return No("That tower is gone.");
			if (t.mHp >= kTowerHealth - 1) return No("That tower isn't damaged.");
			if (!Elapsed(mNow, t.mLastHurtAt + (uint32_t)(kTowerRepairQuietS * 1000)) && t.mLastHurtAt != 0) return No("Not while it's under attack.");
			break;
		}
		case SHOP_TOWER_UPGRADE:
			if (a.mTowerLevel >= kTowerMaxLevel) return No("Your towers are maxed.");
			if (a.AliveTowers() == 0) return No("No towers left.");
			break;
		case SHOP_WAVE_SIZE: if (mWaveSizeRank >= kWaveUpgradeRanks) return No("Waves are as big as they get."); break;
		case SHOP_WAVE_TOUGH: if (mWaveToughRank >= kWaveUpgradeRanks) return No("Minions are as tough as they get."); break;
		case SHOP_SEND_SYLV: case SHOP_SEND_GUS: case SHOP_SEND_BALROG: case SHOP_SEND_DESTRUCTOR:
			if (mQueuedAliens.size() >= 6) return No("Your next wave is full.");
			break;
		default:
			if (theShop >= SHOP_ITEM_FIRST && theShop <= SHOP_ITEM_LAST)
			{
				bool aFree = false;
				for (uint8_t i : mHero.mItems)
					if (i == ITEM_NONE)
						aFree = true;
				if (!aFree) return No("Your item slots are full.");
				break;
			}
			return No("?");
		}
		if (a.mMoney < Price(theShop))
			return No("Not enough money.");
		return true;
	}

	bool Side::Buy(int theShop)
	{
		std::string aWhy;
		if (!CanBuy(theShop, &aWhy))
		{
			mLastRefusal = aWhy;
			Sound(mTeam, SND_BUZZER, Vec());
			return false;
		}
		mLastRefusal.clear();
		mArena.Spend(Price(theShop));
		Arena& a = mArena;
		Vec aDrop(a.mRng.Range(200, kWorldW - 200), kSurfaceY + 40);
		switch (theShop)
		{
		case SHOP_GUPPY: a.AddFish(FISH_GUPPY, mNow, aDrop); Sound(mTeam, SND_SPLASH, aDrop); break;
		case SHOP_BREEDER: a.AddFish(FISH_BREEDER, mNow, aDrop); Sound(mTeam, SND_SPLASH, aDrop); break;
		case SHOP_CARNIVORE: a.AddFish(FISH_CARNIVORE, mNow, aDrop); Sound(mTeam, SND_SPLASH, aDrop); break;
		case SHOP_FOOD_QUALITY: a.mFoodQuality++; break;
		case SHOP_FOOD_COUNT: a.mPellets++; break;
		case SHOP_LASER: a.mLaserLevel++; break;
		case SHOP_REPAIR_LEFT: case SHOP_REPAIR_RIGHT:
		{
			Hit h;
			h.mTeam = (uint8_t)mTeam;
			h.mHeal = kTowerRepair;
			int i = theShop == SHOP_REPAIR_LEFT ? 0 : 1;
			a.ApplyHit(EntityRef::Of(mTeam, ENT_TOWER, (uint32_t)i), h, mNow);
			Text(mTeam, TheMap().mTower[i] - Vec(0, 90), "+" + std::to_string((int)kTowerRepair), TC_HEAL);
			break;
		}
		case SHOP_TOWER_UPGRADE: a.mTowerLevel++; break;
		case SHOP_WAVE_SIZE: mWaveSizeRank++; break;
		case SHOP_WAVE_TOUGH: mWaveToughRank++; break;
		case SHOP_SEND_SYLV: case SHOP_SEND_GUS: case SHOP_SEND_BALROG: case SHOP_SEND_DESTRUCTOR:
			mQueuedAliens.push_back((uint8_t)(MIN_SYLV + (theShop - SHOP_SEND_SYLV)));
			break;
		default:
			if (theShop >= SHOP_ITEM_FIRST && theShop <= SHOP_ITEM_LAST)
			{
				float aOldMax = MaxHp();
				for (uint8_t& i : mHero.mItems)
					if (i == ITEM_NONE)
					{
						i = (uint8_t)(theShop - SHOP_ITEM_FIRST);
						break;
					}
				if (mHero.mAlive)
					mHero.mHp += MaxHp() - aOldMax;
			}
			break;
		}
		Sound(mTeam, SND_BUY, Vec());
		return true;
	}

	Arena::ClickKind Side::HomeClick(Vec theWhere)
	{
		if (Over())
			return Arena::CLICK_NONE;
		ArenaContext c;
		c.mOwnerPlayer = mPlayer;
		HeroSnap o;
		if (OtherHeroIn(mTeam, &o) && (o.mFlags & HF_ALIVE))
		{
			HeroPresence h;
			h.mPlayer = o.mPlayer;
			h.mTeam = o.mTeam;
			h.mPos = o.mPos;
			h.mRadius = HeroDefOf(o.mHero).mRadius;
			h.mTargetable = !(o.mFlags & (HF_HIDDEN | HF_UNTARGETABLE | HF_IMMUNE));
			c.mHeroes.push_back(h);
		}
		Arena::ClickKind k = mArena.Click(theWhere, mNow, c);
		for (auto& h : mArena.mOutHits)
			Deal(h.first, h.second);
		mArena.mOutHits.clear();
		return k;
	}
}
