#include "HeroesSide.h"
#include "HeroesMap.h"

namespace Heroes
{
	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }

	///////////////////////////////////////////////////////////////////////////
	// Mirror
	///////////////////////////////////////////////////////////////////////////
	uint32_t Mirror::LocalTime(uint32_t theNow, int64_t theSentAt)
	{
		if (theSentAt < 0)
			return theNow;
		mStamped = true;
		// The two clocks differ by an unknown amount: the least-delayed snapshot of the
		// last few seconds sets it, and how much later the others came is their lateness.
		int32_t anOffset = (int32_t)(theNow - (uint32_t)theSentAt);
		mSamples.push_back({ theNow, anOffset });
		while (!mSamples.empty() && (int32_t)(theNow - mSamples.front().mAt) > (int32_t)kWindowMs)
			mSamples.pop_front();
		int32_t aMin = anOffset, aMax = anOffset;
		for (const Sample& s : mSamples)
		{
			aMin = std::min(aMin, s.mOffset);
			aMax = std::max(aMax, s.mOffset);
		}
		mWorstLateMs = aMax - aMin;
		return (uint32_t)theSentAt + (uint32_t)aMin;
	}

	uint32_t Mirror::TargetDelayMs() const
	{
		if (!mStamped)
			return kUnstampedDelayMs;
		float aTarget = mIntervalMs + (float)mWorstLateMs + (float)kMarginMs;
		return (uint32_t)Clamp(aTarget, (float)kMinDelayMs, (float)kMaxDelayMs);
	}

	void Mirror::UpdateDelay()
	{
		// Grow quickly (a stall is worse than a little extra delay), shrink slowly.
		uint32_t aTarget = TargetDelayMs();
		if (aTarget > mDelayMs)
			mDelayMs = std::min(aTarget, mDelayMs + 4);
		else if (aTarget < mDelayMs)
			mDelayMs = std::max(aTarget, mDelayMs - 1);
	}

	void Mirror::AddArena(uint32_t theNow, const ArenaSnap& theSnap, int64_t theSentAt)
	{
		uint32_t anAt = LocalTime(theNow, theSentAt);
		std::deque<TimedArena>& q = mArena[theSnap.mArena % kArenaCount];
		if (!q.empty() && (int32_t)(anAt - q.back().mAt) <= 0)
			anAt = q.back().mAt + 1;
		q.push_back({ anAt, theSnap });
		while (q.size() > 8)
			q.pop_front();
	}

	void Mirror::AddHero(uint32_t theNow, const HeroSnap& theSnap, int64_t theSentAt)
	{
		if (theSentAt >= 0)
		{
			if (mLastHeroSentAt >= 0)
			{
				int32_t aGap = (int32_t)((uint32_t)theSentAt - (uint32_t)mLastHeroSentAt);
				if (aGap > 0 && aGap < 500)
					mIntervalMs = mIntervalMs * 0.9f + aGap * 0.1f;
			}
			mLastHeroSentAt = theSentAt;
		}
		uint32_t anAt = LocalTime(theNow, theSentAt);
		std::deque<TimedHero>& q = mHero[theSnap.mPlayer % kMaxPlayers];
		if (!q.empty() && (int32_t)(anAt - q.back().mAt) <= 0)
			anAt = q.back().mAt + 1;
		q.push_back({ anAt, theSnap });
		while (q.size() > 12)
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

	ArenaSnap Mirror::ArenaAt(int theArena, uint32_t theTime) const
	{
		const std::deque<TimedArena>& q = mArena[theArena % kArenaCount];
		if (q.empty())
		{
			ArenaSnap s;
			s.mArena = (uint8_t)theArena;
			return s;
		}
		// The latest snapshot at or before theTime, and the one after it.
		size_t b = q.size() - 1;
		while (b > 0 && (int32_t)(q[b - 1].mAt - theTime) >= 0)
			b--;
		if (b == 0)
			return q.front().mSnap;
		const TimedArena& aOld = q[b - 1];
		const TimedArena& aNew = q[b];
		uint32_t aSpan = std::max<uint32_t>(1, aNew.mAt - aOld.mAt);
		float t = Clamp((float)(int32_t)(theTime - aOld.mAt) / aSpan, 0, 1);
		ArenaSnap s = aNew.mSnap;
		Blend(s.mFish, aOld.mSnap.mFish, t);
		Blend(s.mCoins, aOld.mSnap.mCoins, t);
		Blend(s.mFood, aOld.mSnap.mFood, t);
		Blend(s.mMinions, aOld.mSnap.mMinions, t);
		if (aOld.mSnap.mCollectorLevel > 0)
			s.mCollectorPos = Lerp(aOld.mSnap.mCollectorPos, s.mCollectorPos, t);
		return s;
	}

	bool Mirror::HeroAt(int thePlayer, uint32_t theTime, HeroSnap& theOut) const
	{
		const std::deque<TimedHero>& q = mHero[thePlayer % kMaxPlayers];
		if (q.empty())
			return false;
		mLookups++;
		if ((int32_t)(theTime - q.back().mAt) > 0)
			mStarved++;
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
			return WalkerPos(mTeam, TankMap().mCore.x - 110, d.mRadius);
		return TankMap().mCore - Vec(0, 150);
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
		if (Owns(kTrench))
			mTrench.Init(theSeed * 31 + 7, theNow);
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

	int Side::ItemCount(int theItem) const
	{
		int n = 0;
		for (uint8_t i : mHero.mItems)
			n += i == theItem;
		return n;
	}

	float Side::MaxHp() const
	{
		float h = mHero.Def().mHealth * LevelScale(mHero.mLevel) * (mHero.Evolved() ? 1 + kEvolveBonus : 1.0f);
		if (HasBuff(BUFF_GUS))
			h *= 1 + kGusHealth;
		if (HasBuff(BUFF_BOSS))
			h *= 1 + kBossSurge;
		return h + SumItems(mHero, [](const ItemDef& d) { return d.mHealth; });
	}

	float Side::Damage() const
	{
		float d = mHero.Def().mDamage * LevelScale(mHero.mLevel) * (mHero.Evolved() ? 1 + kEvolveBonus : 1.0f);
		if (HasBuff(BUFF_BOSS))
			d *= 1 + kBossSurge;
		return d + SumItems(mHero, [](const ItemDef& d) { return d.mDamage; });
	}

	float Side::Speed() const
	{
		const HeroState& h = mHero;
		float s = h.Def().mSpeed * (1.0f + SumItems(h, [](const ItemDef& d) { return d.mSpeedPct; }));
		if (HasItem(ITEM_SPEED_KELP) && Elapsed(mNow, h.mLastHurtAt + (uint32_t)(kKelpCalmS * 1000)))
			s *= 1 + kKelpCalmPct;
		if (HasBuff(BUFF_BOSS))
			s *= 1 + kBossSurge * 0.5f;
		if (h.mHero == HERO_ANGIE && HasTalent(2, 1))
			s *= 1.2f;
		if (!Elapsed(mNow, h.mAnthemUntil))
			s *= 1 + h.Def().mAb[AB_W].mDamage;
		if (!Elapsed(mNow, h.mSpeedUntil))
			s *= 1.0f + h.mSpeedPct;
		if (!Elapsed(mNow, h.mSlowUntil))
			s *= 1.0f - h.mSlowPct;
		if (Rooted())
			s *= HasTalent(1, 1) ? 0.4f : 0.0f;
		return s;
	}

	float Side::AttackPeriod() const
	{
		const HeroState& h = mHero;
		float p = h.Def().mAttackS;
		if (h.Look() == HERO_ITCHY)
			p /= 1.0f + 0.08f * h.mStacks;
		if (HasBuff(BUFF_BOSS))
			p /= 1 + kBossSurge;
		if (!Elapsed(mNow, h.mAnthemUntil))
			p /= 1 + h.Def().mAb[AB_W].mDamage;
		return p;
	}

	float Side::CooldownMult() const { return 1.0f - std::min(0.45f, SumItems(mHero, [](const ItemDef& d) { return d.mCdrPct; })); }

	float Side::AbilityScale(int theSlot) const
	{
		int aRank = theSlot == AB_R ? std::max(1, mHero.mRank[AB_R]) : mHero.mRank[theSlot];
		float s = RankScale(aRank) * (1.0f + 0.04f * (mHero.mLevel - 1)) * (1.0f + SumItems(mHero, [](const ItemDef& d) { return d.mAbilityPct; }));
		if (HasBuff(BUFF_BOSS))
			s *= 1 + kBossSurge;
		return s;
	}

	float Side::ArmorVsStructures() const
	{
		float a = 1.0f - std::min(0.5f, SumItems(mHero, [](const ItemDef& d) { return d.mArmorPct; }));
		if (mHero.Look() == HERO_RHUBARB)
			a *= 0.7f;
		return a;
	}

	float Side::StructBonus() const
	{
		return 1.0f + SumItems(mHero, [](const ItemDef& d) { return d.mStructPct; }) + (mHero.Def().mWalker ? 0.25f : 0.0f)
			+ (mHero.Look() == HERO_SHRAPNEL ? 0.2f : 0.0f);
	}

	float Side::Lifesteal() const { return SumItems(mHero, [](const ItemDef& d) { return d.mLifestealPct; }); }
	uint32_t Side::CooldownLeft(int theSlot) const { return Elapsed(mNow, mHero.mReadyAt[theSlot]) ? 0 : mHero.mReadyAt[theSlot] - mNow; }
	uint32_t Side::CooldownTotal(int theSlot) const { return std::max<uint32_t>(1, mCastTotal[theSlot]); }
	bool Side::Busy() const { return !Elapsed(mNow, mHero.mDashUntil) || !Elapsed(mNow, mHero.mLeapUntil); }
	bool Side::Hopping() const { return !Elapsed(mNow, mHero.mHopUntil); }
	bool Side::Rooted() const { return !Elapsed(mNow, mHero.mFortressUntil); }

	///////////////////////////////////////////////////////////////////////////
	// Talents
	///////////////////////////////////////////////////////////////////////////
	int Side::PendingTalent() const
	{
		for (int t = 0; t < kTalentTiers; t++)
			if (mHero.mTalent[t] < 0 && mHero.mLevel >= kTalentLevel[t])
				return t;
		return -1;
	}

	bool Side::PickTalent(int theChoice)
	{
		int t = PendingTalent();
		if (t < 0 || theChoice < 0 || theChoice > 1 || Over())
			return false;
		mHero.mTalent[t] = (int8_t)theChoice;
		Sound(mHero.mArena, SND_LEVEL, mHero.mPos);
		Text(mHero.mArena, mHero.mPos - Vec(0, 70), HeroDefOf(mHero.mHero).mTalent[t][theChoice].mName, TC_XP);
		return true;
	}

	bool Side::HasTalent(int theTier, int theChoice) const
	{
		// A copy (Copycat) fights with the copied hero's kit but none of its talents.
		return mHero.mCopyHero < 0 && theTier >= 0 && theTier < kTalentTiers && mHero.mTalent[theTier] == theChoice;
	}

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
		mOther.UpdateDelay();
		for (size_t i = 0; i < mEffects.size();)
			if (Elapsed(mNow, mEffects[i].mAt + 2500 + mEffects[i].mEvent.mMs))
				mEffects.erase(mEffects.begin() + i);
			else
				i++;
		if (Over())
			return;
		mSuddenDeath = MatchMs() >= (uint32_t)(kSuddenDeathS * 1000);

		StepHero(dt);
		StepProjectiles(dt);
		StepZones();
		StepImpacts();
		StepTurrets();
		StepBurns();
		StepArena();
		if (Owns(kTrench))
			StepTrench();

		// Passive income: items, and Speedy's Scavenger (Hoarder: more).
		float aGold = SumItems(mHero, [](const ItemDef& d) { return d.mGoldPerS; });
		if (mHero.mHero == HERO_SPEEDY)
			aGold += HasTalent(2, 0) ? 4.0f : 1.0f;
		mPassiveGold += aGold * dt;
		if (mPassiveGold >= 1)
		{
			int n = (int)mPassiveGold;
			mPassiveGold -= n;
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

	HeroPresence Side::MyPresence() const
	{
		const HeroState& h = mHero;
		HeroPresence p;
		p.mPlayer = mPlayer;
		p.mTeam = mTeam;
		p.mPos = h.mPos;
		p.mRadius = h.Def().mRadius;
		p.mAlive = h.mAlive;
		p.mTargetable = h.mAlive && Elapsed(mNow, h.mUntargetableUntil) && Elapsed(mNow, h.mImmuneUntil) && !h.mHidden
			&& Elapsed(mNow, h.mDecoyUntil);
		p.mTaunting = !Elapsed(mNow, h.mTauntUntil);
		p.mMagnet = h.mHero == HERO_SPEEDY && HasTalent(1, 1) && !Elapsed(mNow, h.mGoldRushUntil);
		p.mHurtHeroAt = mHitHeroAt;
		return p;
	}

	bool Side::OtherPresence(int theArena, HeroPresence& theOut) const
	{
		HeroSnap o;
		if (!OtherHeroIn(theArena, &o) || !(o.mFlags & HF_ALIVE))
			return false;
		theOut = HeroPresence();
		theOut.mPlayer = o.mPlayer;
		theOut.mTeam = o.mTeam;
		theOut.mPos = o.mPos;
		theOut.mRadius = HeroDefOf(o.mHero).mRadius;
		theOut.mAlive = true;
		theOut.mTargetable = !(o.mFlags & (HF_HIDDEN | HF_UNTARGETABLE | HF_IMMUNE));
		theOut.mTaunting = (o.mFlags & HF_TAUNT) != 0;
		theOut.mMagnet = (o.mFlags & HF_GOLDRUSH) && o.mHero == HERO_SPEEDY && ((o.mTalents >> 2) & 3) == 2;
		theOut.mHurtMyHeroAt = mHurtByAt[o.mPlayer % kMaxPlayers];
		theOut.mHurtHeroAt = mHurtByAt[o.mPlayer % kMaxPlayers];
		return true;
	}

	void Side::ApplyRewardsFrom(std::vector<Reward>& theRewards)
	{
		for (Reward& r : theRewards)
		{
			if (r.mPlayer == mPlayer || (r.mPlayer < 0 && r.mTeam == mTeam))
				GainReward(r);
			else if (r.mPlayer >= 0 || (r.mTeam >= 0 && r.mTeam < kTeams))
				mOutRewards.push_back(r);
		}
		theRewards.clear();
	}

	void Side::StepArena()
	{
		ArenaContext c;
		c.mOwnerPlayer = mPlayer;
		c.mOwnerHeroHere = mHero.mAlive && mHero.mArena == mTeam;
		c.mOwnerHeroPos = mHero.mPos;
		c.mScavenger = mHero.mHero == HERO_SPEEDY;
		c.mGoldRush = mHero.mHero == HERO_SPEEDY && !Elapsed(mNow, mHero.mGoldRushUntil);
		c.mGrace = mHero.mHero == HERO_ANGIE;
		c.mFanClub = mHero.mHero == HERO_MERYL && mHero.mAlive;
		c.mSuddenDeath = mSuddenDeath;
		if (c.mOwnerHeroHere)
			c.mHeroes.push_back(MyPresence());
		HeroPresence o;
		if (OtherPresence(mTeam, o))
			c.mHeroes.push_back(o);
		mArena.Step(mNow, c);

		// Drain what the arena produced.
		for (auto& h : mArena.mOutHits)
			Deal(h.first, h.second);
		mArena.mOutHits.clear();
		ApplyRewardsFrom(mArena.mOutRewards);
		for (Event& e : mArena.mOutEvents)
			Emit(e);
		mArena.mOutEvents.clear();
	}

	void Side::StepTrench()
	{
		TrenchContext c;
		c.mSuddenDeath = mSuddenDeath;
		if (mHero.mAlive && mHero.mArena == kTrench)
			c.mHeroes.push_back(MyPresence());
		HeroPresence o;
		if (OtherPresence(kTrench, o))
			c.mHeroes.push_back(o);
		mTrench.Step(mNow, c);
		for (auto& h : mTrench.mOutHits)
			Deal(h.first, h.second);
		mTrench.mOutHits.clear();
		ApplyRewardsFrom(mTrench.mOutRewards);
		for (Event& e : mTrench.mOutEvents)
			Emit(e);
		mTrench.mOutEvents.clear();
		for (int t = 0; t < kTeams; t++)
			if (!mTrench.mOutArrivals[t].empty())
			{
				Arrivals(t, mTrench.mOutArrivals[t]);
				mTrench.mOutArrivals[t].clear();
			}
	}

	// Minions leaving the Trench for team theTeam's tank.
	void Side::Arrivals(int theTeam, const std::vector<Arrival>& theMinions)
	{
		if (theTeam == mTeam)
		{
			mArena.SpawnWave(theMinions, 1 - mTeam, mNow);
			return;
		}
		Writer w;
		w.U8((uint8_t)(1 - theTeam));
		w.U8((uint8_t)std::min<size_t>(theMinions.size(), 255));
		for (size_t i = 0; i < theMinions.size() && i < 255; i++)
			Put(w, theMinions[i]);
		if (mLink != nullptr)
			mLink->Send(HM_ARRIVE, w.mData);
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
		std::vector<Arrival> aWave;
		int aGrown = (int)(MatchMs() / (kWaveGrowEveryS * 1000));
		std::vector<uint8_t> aKinds(kWaveSize + mWaveSizeRank + aExtra + aGrown, MIN_MINI);
		aKinds.insert(aKinds.end(), mQueuedAliens.begin(), mQueuedAliens.end());
		mQueuedAliens.clear();
		for (uint8_t k : aKinds)
		{
			Arrival a;
			a.mKind = k;
			a.mMult = k == MIN_SQUID ? 1.0f : aMult;	// the Squid is the Squid
			aWave.push_back(a);
		}
		mWavesSent++;
		if (Owns(kTrench))
		{
			mTrench.SpawnWave(aWave, mTeam, mNow);
			return;
		}
		Writer w;
		w.U8((uint8_t)mTeam);
		w.U8((uint8_t)std::min<size_t>(aWave.size(), 255));
		for (size_t i = 0; i < aWave.size() && i < 255; i++)
			Put(w, aWave[i]);
		if (mLink != nullptr)
			mLink->Send(HM_WAVE, w.mData);
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
		// Every step, stamped with our clock (D34).
		{
			mLastHeroSent = mNow;
			Writer w;
			Put(w, MySnap());
			w.U32(mNow);
			mLink->Send(HM_HERO, w.mData);
		}
		uint32_t aEvery = OtherHeroIn(mTeam) ? 0 : 333;
		if (Elapsed(mNow, mLastArenaSent + aEvery))
		{
			mLastArenaSent = mNow;
			Writer w;
			Put(w, mArena.Snapshot());
			w.U32(mNow);
			mLink->Send(HM_ARENA, w.mData);
		}
		if (Owns(kTrench))
		{
			// The Trench: every step while the rival's hero is in it (they fight there), else
			// 3 times a second (their map).
			uint32_t aTrenchEvery = OtherHeroIn(kTrench) ? 0 : 333;
			if (Elapsed(mNow, mLastTrenchSent + aTrenchEvery))
			{
				mLastTrenchSent = mNow;
				Writer w;
				Put(w, mTrench.Snapshot());
				w.U32(mNow);
				mLink->Send(HM_ARENA, w.mData);
			}
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
			{
				int64_t aSentAt = r.Done() ? -1 : (int64_t)r.U32();
				mOther.AddHero(mNow, s, r.mBad ? -1 : aSentAt);
			}
			break;
		}
		case HM_ARENA:
		{
			ArenaSnap s;
			if (Get(r, s) && !Owns(s.mArena))
			{
				int64_t aSentAt = r.Done() ? -1 : (int64_t)r.U32();
				mOther.AddArena(mNow, s, r.mBad ? -1 : aSentAt);
			}
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
				else if (aRef.mArena == kTrench && Owns(kTrench))
					mTrench.ApplyHit(aRef, h, mNow);
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
				{
					if (e.mType == EV_ANNOUNCE && e.mParam == AN_FIRST_BLOOD)
						mFirstBlood = true;
					mEffects.push_back({ e, mNow, ++mEffectSeq });
				}
			}
			break;
		}
		case HM_WAVE:
		case HM_ARRIVE:
		{
			int aFrom = r.U8();
			int n = r.U8();
			std::vector<Arrival> aMinions;
			for (int i = 0; i < n && !r.mBad; i++)
			{
				Arrival a;
				if (Get(r, a))
					aMinions.push_back(a);
			}
			if (r.mBad || Over() || aFrom < 0 || aFrom >= kTeams)
				break;
			if (p.mType == HM_WAVE && Owns(kTrench) && aFrom != mTeam)
				mTrench.SpawnWave(aMinions, aFrom, mNow);
			else if (p.mType == HM_ARRIVE && aFrom != mTeam)
				mArena.SpawnWave(aMinions, aFrom, mNow);
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
	Hit Side::MakeHit(float theDamage, uint8_t theSource, uint8_t theAbility) const
	{
		Hit h;
		h.mDamage = theDamage;
		h.mPlayer = (int8_t)mPlayer;
		h.mTeam = (uint8_t)mTeam;
		h.mSource = theSource;
		h.mAbility = theAbility;
		h.mHero = (uint8_t)mHero.Look();
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
			// Leech Tooth: a little back from abilities too (attacks heal in AttackLanded).
			if ((theHit.mSource == SRC_ABILITY || theHit.mSource == SRC_ZONE) && theHit.mDamage > 0 && HasItem(ITEM_LEECH_TOOTH) && mHero.mAlive)
				mHero.mHp = std::min(MaxHp(), mHero.mHp + theHit.mDamage * kAbilityLifesteal);
		}
		if (theTarget.mKind == ENT_HERO)
		{
			if ((int)theTarget.mId == mPlayer)
				ApplyToMyHero(theHit);
			else
			{
				mOutHits.push_back({ theTarget, theHit });
				if (theHit.mPlayer == mPlayer)
				{
					mHero.mHeroDamage += theHit.mDamage;
					if (theHit.mDamage > 0)
						mHitHeroAt = mNow;
				}
			}
			return;
		}
		if ((theTarget.mKind == ENT_TOWER || theTarget.mKind == ENT_CORE) && theHit.mPlayer == mPlayer)
			mHero.mStructDamage += theHit.mDamage;
		if (theTarget.mArena == mTeam)
			mArena.ApplyHit(theTarget, theHit, mNow);
		else if (theTarget.mArena == kTrench && Owns(kTrench))
			mTrench.ApplyHit(theTarget, theHit, mNow);
		else
			mOutHits.push_back({ theTarget, theHit });
	}

	void Side::CheckInk()
	{
		HeroState& h = mHero;
		if (!HasItem(ITEM_INK_SAC) || !h.mAlive || h.mHp > MaxHp() * kInkAtHp || !Elapsed(mNow, h.mInkReadyAt))
			return;
		h.mInkReadyAt = mNow + (uint32_t)(kInkCooldownS * 1000);
		h.mUntargetableUntil = std::max(h.mUntargetableUntil, mNow + (uint32_t)(kInkUntargetableS * 1000));
		h.mSpeedUntil = mNow + (uint32_t)(kInkSpeedS * 1000);
		h.mSpeedPct = kInkSpeedPct;
		Event e;
		e.mType = EV_ZONE;
		e.mArena = (uint8_t)h.mArena;
		e.mA = h.mPos;
		e.mValue = 90;
		e.mMs = 1200;
		e.mParam = LOOK_INK;
		Emit(e);
		Sound(h.mArena, SND_BIG_SPLASH, h.mPos);
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
		if (theHit.mCleanse)
		{
			h.mStunUntil = h.mSleepUntil = h.mSlowUntil = 0;
		}
		if (theHit.mTeam == mTeam)
			return;								// no friendly fire
		if (!Elapsed(mNow, h.mImmuneUntil) || !Elapsed(mNow, h.mUntargetableUntil))
		{
			if (theHit.mDamage > 0)
				Text(h.mArena, h.mPos - Vec(0, 50), "Immune", TC_INFO);
			// Spiked Shell: hitting Rhubarb in his shell hurts.
			if (theHit.mPlayer >= 0 && theHit.mDamage > 0 && !Elapsed(mNow, h.mSpikedUntil))
			{
				Hit s = MakeHit(40, SRC_ABILITY, AB_E);
				Deal(EntityRef::Hero(theHit.mPlayer), s);
			}
			return;
		}
		if (Hopping() && (theHit.mSource == SRC_MINION || (theHit.mSource == SRC_ZONE && theHit.mSlowMs > 0)))
		{
			if (theHit.mDamage > 0)
				Text(h.mArena, h.mPos - Vec(0, 50), "Dodged", TC_INFO);
			return;								// minion attacks and slowing ground miss a hopping walker
		}
		if (theHit.mPlayer >= 0)
			mHurtByAt[theHit.mPlayer % kMaxPlayers] = mNow;
		// Niko's Clam Shield: the first hero to hit him is stunned.
		if (h.mClamStun && !Elapsed(mNow, h.mClamUntil) && theHit.mPlayer >= 0 && theHit.mDamage > 0)
		{
			h.mClamStun = false;
			Hit s = MakeHit(0, SRC_ABILITY, AB_W);
			s.mStunMs = (uint16_t)(h.Def().mAb[AB_W].mExtra * 1000);
			Deal(EntityRef::Hero(theHit.mPlayer), s);
			Burst(h.mArena, h.mPos, 60, LOOK_PEARL);
			Sound(h.mArena, SND_CLAM_CLOSE, h.mPos);
		}
		if (theHit.mStunMs > 0)
			h.mStunUntil = std::max(h.mStunUntil, mNow + theHit.mStunMs);
		if (theHit.mSleepMs > 0)
			h.mSleepUntil = std::max(h.mSleepUntil, mNow + theHit.mSleepMs);
		if (theHit.mSlowMs > 0)
		{
			// The strongest slow still running wins.
			if (Elapsed(mNow, h.mSlowUntil) || theHit.mSlowPct >= h.mSlowPct)
				h.mSlowPct = theHit.mSlowPct;
			h.mSlowUntil = std::max(h.mSlowUntil, mNow + theHit.mSlowMs);
		}
		const HeroDef& d = h.Def();
		if (!Rooted())
		{
			if (theHit.mPull)
				h.mPos = d.mWalker ? WalkerPos(h.mArena, theHit.mPullTo.x, d.mRadius) : NearestOpen(h.mArena, theHit.mPullTo);
			if (theHit.mPush.x != 0 || theHit.mPush.y != 0)
			{
				Vec p = h.mPos + theHit.mPush;
				h.mPos = d.mWalker ? WalkerPos(h.mArena, p.x, d.mRadius) : NearestOpen(h.mArena, p);
			}
			if (theHit.mPull || theHit.mPush.x != 0 || theHit.mPush.y != 0)
				h.mPath.clear();
		}
		float aDamage = theHit.mDamage + theHit.mMaxHpPct * MaxHp();
		if (aDamage <= 0)
			return;
		if (theHit.mSource == SRC_TOWER || theHit.mSource == SRC_LASER)
			aDamage *= ArmorVsStructures();
		else if ((theHit.mSource == SRC_MINION || theHit.mSource == SRC_MONSTER) && HasItem(ITEM_CORAL_ARMOR))
			aDamage *= 1 - ItemDefOf(ITEM_CORAL_ARMOR).mArmorPct;
		if (h.mArena == mTeam)
			aDamage *= kHomeArmor;						// home waters
		if (!Elapsed(mNow, h.mClamUntil))
			aDamage *= 1 - h.Def().mAb[AB_W].mDamage;	// Clam Shield (Niko's, or a copy's)
		if (Rooted())
			aDamage *= 0.55f;							// Fortress
		else if (h.Look() == HERO_NIKO && Elapsed(mNow, h.mLastMoveAt + 400))
			aDamage *= 0.8f;							// Hard Shell
		if (h.mShield > 0)
		{
			float a = std::min(h.mShield, aDamage);
			h.mShield -= a;
			aDamage -= a;
		}
		h.mHp -= aDamage;
		h.mLastHurtAt = mNow;
		h.mSleepUntil = theHit.mSleepMs > 0 ? h.mSleepUntil : 0;	// damage wakes you
		if (aDamage >= 1)
			Text(h.mArena, h.mPos - Vec(0, 46), std::to_string((int)std::lround(aDamage)), TC_DAMAGE);
		// The death recap: the last 10 s.
		RecapHit rh;
		rh.mAt = mNow;
		rh.mSource = theHit.mSource;
		rh.mPlayer = theHit.mPlayer;
		rh.mHero = theHit.mHero;
		rh.mAbility = theHit.mAbility;
		rh.mDamage = aDamage;
		mRecap.push_back(rh);
		while (!mRecap.empty() && Elapsed(mNow, mRecap.front().mAt + 10000))
			mRecap.erase(mRecap.begin());
		if (h.mHp <= 0)
			Die(theHit);
		else
			CheckInk();
	}

	void Side::Die(const Hit& theKiller)
	{
		HeroState& h = mHero;
		if (h.mCopyHero >= 0)
			EndCopy();
		h.mHp = 0;
		h.mAlive = false;
		h.mDeaths++;
		h.mRespawnAt = mNow + (uint32_t)(RespawnS(h.mLevel) * 1000);
		h.mOrder = HeroState::ORD_IDLE;
		h.mPath.clear();
		h.mStormUntil = h.mSlimeUntil = h.mHealUntil = h.mDashUntil = h.mLeapUntil = h.mHopUntil = 0;
		h.mFortressUntil = h.mAnthemUntil = h.mDecoyUntil = h.mClamUntil = 0;
		h.mThunderLeft = 0;
		h.mBuffUntil[BUFF_GUS] = h.mBuffUntil[BUFF_BALROG] = 0;	// camp buffs end; the Boss's surge stays
		// The bounty: more for ending a streak (a shutdown).
		int aShutdown = h.mStreak >= kShutdownStreak ? std::min(kShutdownMax, kShutdownPerKill * h.mStreak) : 0;
		Reward r;
		r.mPlayer = theKiller.mPlayer;
		r.mTeam = (int8_t)theKiller.mTeam;
		r.mXp = kXpHeroKill + kXpHeroKillPerLevel * h.mLevel;
		r.mMoney = kBountyHeroKill + kBountyHeroKillPerLevel * h.mLevel + aShutdown;
		r.mWhat = ENT_HERO;
		if (theKiller.mTeam < kTeams)
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
		// Banners: first blood, the killer's streak, a shutdown.
		if (theKiller.mPlayer >= 0 && theKiller.mTeam != mTeam)
		{
			if (!mFirstBlood)
			{
				mFirstBlood = true;
				Announce(AN_FIRST_BLOOD, theKiller.mPlayer, mPlayer);
			}
			if (aShutdown > 0)
				Announce(AN_SHUTDOWN, theKiller.mPlayer, mPlayer, (float)aShutdown);
			const HeroSnap* o = OtherHero();
			int aStreak = (o != nullptr ? o->mStreak : 0) + 1;
			if (aStreak == 3) Announce(AN_SPREE, theKiller.mPlayer, mPlayer, (float)aStreak);
			else if (aStreak == 5) Announce(AN_RAMPAGE, theKiller.mPlayer, mPlayer, (float)aStreak);
			else if (aStreak == 7) Announce(AN_UNSTOPPABLE, theKiller.mPlayer, mPlayer, (float)aStreak);
			else if (aStreak >= 9 && aStreak % 2 == 1) Announce(AN_GODLIKE, theKiller.mPlayer, mPlayer, (float)aStreak);
		}
		h.mStreak = 0;
		// Shrapnel's Volatile: he goes out with a bang.
		if (h.mHero == HERO_SHRAPNEL)
		{
			Impact im;
			im.mArena = h.mArena;
			im.mPos = h.mPos;
			im.mAt = mNow;
			im.mRadius = 160;
			im.mHit = MakeHit(150, SRC_ABILITY);
			im.mLook = LOOK_BOMB;
			im.mSound = SND_BOOM;
			mImpacts.push_back(im);
		}
		mBurns.clear();
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
		h.mAutoTarget = EntityRef();
		h.mStunUntil = h.mSleepUntil = h.mSlowUntil = h.mUntargetableUntil = h.mImmuneUntil = h.mSpeedUntil = h.mTauntUntil = 0;
		h.mShield = 0;
		h.mInPortal = false;
		h.mStacks = 0;
		mRecap.clear();
		Burst(mTeam, h.mPos, 60, LOOK_HALO);
	}

	void Side::StartBuff(int theBuff)
	{
		HeroState& h = mHero;
		if (theBuff == BUFF_SQUID)
		{
			mQueuedAliens.push_back(MIN_SQUID);
			Text(h.mArena, h.mPos - Vec(0, 84), "The Squid joins your next wave!", TC_XP);
			return;
		}
		if (theBuff < 0 || theBuff >= BUFF_COUNT)
			return;
		float aOldMax = MaxHp();
		h.mBuffUntil[theBuff] = mNow + (uint32_t)(kBuffS[theBuff] * 1000);
		if (h.mAlive)
			h.mHp += std::max(0.0f, MaxHp() - aOldMax);
		static const char* kName[BUFF_COUNT] = { "Gus's might!", "Balrog's fire!", "THE BOSS'S POWER!" };
		Text(h.mArena, h.mPos - Vec(0, 84), kName[theBuff], TC_XP);
		Burst(h.mArena, h.mPos, theBuff == BUFF_BOSS ? 140 : 80, theBuff == BUFF_BOSS ? LOOK_BOSS : (theBuff == BUFF_BALROG ? LOOK_BURN : LOOK_GOLD));
	}

	void Side::GainReward(const Reward& r)
	{
		GainXp(r.mXp);
		int aMoney = r.mMoney;
		if (r.mWhat == ENT_COIN && HasItem(ITEM_GOLDEN_SCALE))
			aMoney = (int)std::lround(aMoney * (1 + kGoldenCoinBonus));
		if (aMoney > 0)
		{
			mArena.Earn(aMoney, Vec(), false);
			if (mHero.mAlive)
				Text(mHero.mArena, mHero.mPos - Vec(0, 64), "+$" + std::to_string(aMoney), TC_MONEY);
		}
		if (r.mWhat == ENT_HERO)
		{
			mHero.mKills++;
			mHero.mStreak++;
		}
		else if (r.mWhat == ENT_MINION)
			mHero.mMinionKills++;
		else if (r.mWhat == ENT_TOWER)
			mHero.mTowers++;
		else if (r.mWhat == ENT_COIN)
			mHero.mLaneCoins += aMoney;
		if (r.mBuff != 255)
		{
			mHero.mObjectives++;
			StartBuff(r.mBuff);
		}
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
			// Abilities rank up on their own (D36), in the hero's order.
			const HeroDef& d = HeroDefOf(h.mHero);
			int s = d.mRankOrder[std::clamp(h.mLevel - 2, 0, 8)];
			if (h.mRank[s] < kMaxRank)
				h.mRank[s]++;
			else
				for (int k = AB_Q; k <= AB_E; k++)
					if (h.mRank[k] < kMaxRank)
					{
						h.mRank[k]++;
						break;
					}
			if (h.mLevel == kUltLevel)
				h.mRank[AB_R] = 1;
			else if (h.mLevel == kUlt2Level)
				h.mRank[AB_R] = 2;
			h.mHp += MaxHp() - aOldMax;
			Event e;
			e.mType = EV_LEVEL_UP;
			e.mArena = (uint8_t)h.mArena;
			e.mPlayer = (int8_t)mPlayer;
			e.mA = h.mPos;
			e.mValue = (float)h.mLevel;
			Emit(e);
			Sound(h.mArena, SND_LEVEL, h.mPos);
			if (h.mLevel == kEvolveLevel)
			{
				// The pet evolves.
				Burst(h.mArena, h.mPos, 150, LOOK_EVOLVE);
				Sound(h.mArena, SND_EVOLVE, h.mPos);
				Announce(AN_EVOLVE, mPlayer, -1, (float)h.mHero);
			}
		}
		if (h.mLevel >= kMaxLevel)
			h.mXp = 0;
	}

	void Side::Emit(const Event& e)
	{
		mEffects.push_back({ e, mNow, ++mEffectSeq });
		mOutEvents.push_back(e);
	}

	void Side::Local(const Event& e)
	{
		mEffects.push_back({ e, mNow, ++mEffectSeq });
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

	void Side::Burst(int theArena, Vec theAt, float theRadius, uint8_t theLook)
	{
		Event e;
		e.mType = EV_BURST;
		e.mArena = (uint8_t)theArena;
		e.mA = theAt;
		e.mValue = theRadius;
		e.mParam = theLook;
		Emit(e);
	}

	void Side::Announce(uint8_t theKind, int thePlayer, int theOther, float theValue)
	{
		Event e;
		e.mType = EV_ANNOUNCE;
		e.mArena = (uint8_t)mHero.mArena;
		e.mParam = theKind;
		e.mPlayer = (int8_t)thePlayer;
		e.mId = (uint32_t)(theOther < 0 ? 255 : theOther);
		e.mValue = theValue;
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
			if (theAsDrawn && mOther.HeroAt(mOtherPlayer, mNow - mOther.DelayMs(), aSmooth) && aSmooth.mArena == s->mArena)
				theOut->mPos = aSmooth.mPos;
		}
		return true;
	}

	const ArenaSnap* Side::LatestArena(int theArena) const { return mOther.LatestArena(theArena); }

	bool Side::ArenaHasOpenCore(int theArena) const
	{
		if (!IsTank(theArena))
			return false;
		if (theArena == mTeam)
			return mArena.CoreOpen();
		const ArenaSnap* s = mOther.LatestArena(theArena);
		return s != nullptr && s->mTowerHp[0] <= 0 && s->mTowerHp[1] <= 0;
	}

	uint32_t Side::MonsterIn(int theSlot) const
	{
		if (Owns(kTrench))
			return mTrench.MonsterIn(theSlot, mNow);
		const ArenaSnap* s = mOther.LatestArena(kTrench);
		return s != nullptr ? s->mMonsterIn[theSlot] * 1000u : (uint32_t)(kMonsterFirstS[theSlot] * 1000);
	}

	bool Side::MonsterAlive(int theSlot, Vec* thePos, float* theHpFrac) const
	{
		static const uint8_t kKind[MON_COUNT] = { MIN_CAMP_GUS, MIN_CAMP_BALROG, MIN_PSYCHO, MIN_BOSS };
		auto Found = [&](Vec p, float f) { if (thePos) *thePos = p; if (theHpFrac) *theHpFrac = f; return true; };
		if (Owns(kTrench))
		{
			const Minion* m = mTrench.Monster(theSlot);
			return m != nullptr && Found(m->mPos, m->mHp / std::max(1.0f, m->mMaxHp));
		}
		const ArenaSnap* s = mOther.LatestArena(kTrench);
		if (s != nullptr)
			for (const MinionSnap& m : s->mMinions)
				if (m.mKind == kKind[theSlot] && m.mTeam == kNeutralTeam)
					return Found(m.mPos, m.mHpFrac);
		return false;
	}

	void Side::Targets(int theArena, std::vector<Target>& theOut, bool theAsDrawn) const
	{
		theOut.clear();
		const MapDef& aMap = MapOf(theArena);
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
			t.mAsleep = (s.mFlags & HF_ASLEEP) != 0;
			theOut.push_back(t);
		};
		ArenaSnap s;
		if (theAsDrawn || Owns(theArena))
			s = ViewArena(theArena);
		else if (const ArenaSnap* aLatest = mOther.LatestArena(theArena))
			s = *aLatest;
		else
			s.mArena = (uint8_t)theArena;
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
			t.mMonster = m.mTeam == kNeutralTeam;
			t.mAngry = (m.mFlags & MF_ANGRY) != 0;
			t.mAsleep = (m.mFlags & MF_ASLEEP) != 0;
			t.mKind = m.mKind;
			theOut.push_back(t);
		}
		if (IsTank(theArena))
		{
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

	void Side::EnemiesNear(int theArena, Vec thePos, float theRadius, std::vector<Target>& theOut, bool theStructures, bool theMonsters) const
	{
		std::vector<Target> v;
		Targets(theArena, v);
		theOut.clear();
		bool aOpen = ArenaHasOpenCore(theArena);
		for (const Target& t : v)
		{
			if (t.mTeam == mTeam)
				continue;
			if (t.mMonster && !theMonsters)
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
		s.mHero = (uint8_t)h.Look();
		s.mBaseHero = h.mHero;
		s.mArena = (uint8_t)h.mArena;
		uint32_t f = 0;
		if (h.mAlive) f |= HF_ALIVE;
		if (h.mHidden || !Elapsed(mNow, h.mDecoyUntil)) f |= HF_HIDDEN;
		if (!Elapsed(mNow, h.mUntargetableUntil)) f |= HF_UNTARGETABLE;
		if (!Elapsed(mNow, h.mStunUntil)) f |= HF_STUNNED;
		if (!Elapsed(mNow, h.mSleepUntil)) f |= HF_ASLEEP;
		if (!Elapsed(mNow, h.mImmuneUntil)) f |= HF_IMMUNE;
		if (h.mRight) f |= HF_RIGHT;
		if (!Elapsed(mNow, h.mAttackingUntil)) f |= HF_ATTACKING;
		if (!Elapsed(mNow, h.mStormUntil)) f |= HF_STORM;
		if (!Elapsed(mNow, h.mLeapUntil)) f |= HF_LEAPING;
		if (!Elapsed(mNow, h.mSlowUntil)) f |= HF_SLOWED;
		if (!Elapsed(mNow, h.mGoldRushUntil)) f |= HF_GOLDRUSH;
		if (!Elapsed(mNow, h.mTauntUntil)) f |= HF_TAUNT;
		if (!Elapsed(mNow, h.mSpeedUntil)) f |= HF_SPEED;
		if (h.mCopyHero >= 0) f |= HF_COPY;
		if (Rooted()) f |= HF_FORTRESS;
		if (!Elapsed(mNow, h.mAnthemUntil)) f |= HF_ANTHEM;
		if (!Elapsed(mNow, h.mClamUntil)) f |= HF_CLAM;
		if (h.mInkReadyAt != 0 && !Elapsed(mNow, h.mInkReadyAt - (uint32_t)((kInkCooldownS - kInkSpeedS) * 1000))) f |= HF_INK;
		if (HasBuff(BUFF_BALROG)) f |= HF_BURNING;
		s.mFlags = f;
		s.mPos = h.mPos;
		s.mHp = h.mHp;
		s.mMaxHp = MaxHp();
		s.mShield = h.mShield;
		s.mLevel = (uint8_t)h.mLevel;
		s.mRespawnMs = h.mAlive ? 0 : (uint16_t)std::min<uint32_t>(65000, Elapsed(mNow, h.mRespawnAt) ? 0 : h.mRespawnAt - mNow);
		s.mKills = (uint16_t)h.mKills;
		s.mDeaths = (uint16_t)h.mDeaths;
		s.mStreak = (uint8_t)std::min(255, h.mStreak);
		uint8_t aTalents = 0;
		for (int t = 0; t < kTalentTiers; t++)
			aTalents |= (uint8_t)((h.mTalent[t] + 1) << (2 * t));
		s.mTalents = aTalents;
		for (int b = 0; b < BUFF_COUNT; b++)
			s.mBuffS[b] = HasBuff(b) ? (uint8_t)std::min<uint32_t>(255, (h.mBuffUntil[b] - mNow + 999) / 1000) : 0;
		for (int i = 0; i < kItemSlots; i++)
			s.mItems[i] = h.mItems[i];
		s.mFishLost = (uint16_t)std::min(65535, mArena.mFishLost);
		s.mTowers = (uint16_t)h.mTowers;
		s.mEarned = (uint32_t)std::max(0, mArena.mMoneyEarned);
		s.mStructDamage = (uint32_t)std::max(0.0f, h.mStructDamage);
		s.mLaneCoins = (uint16_t)std::min(65535, h.mLaneCoins);
		s.mMinionKills = (uint16_t)std::min(65535, h.mMinionKills);
		s.mObjectives = (uint8_t)std::min(255, h.mObjectives);
		return s;
	}

	ArenaSnap Side::ViewArena(int theArena) const
	{
		if (theArena == mTeam)
			return mArena.Snapshot();
		if (theArena == kTrench && Owns(kTrench))
			return mTrench.Snapshot();
		return mOther.ArenaAt(theArena, mNow - mOther.DelayMs());
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
		case SHOP_COLLECTOR: return a.mCollectorLevel < 2 ? kCollectorPrice[a.mCollectorLevel] : 0;
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
		case SHOP_COLLECTOR: return mArena.mCollectorLevel;
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
				return ItemCount(theShop - SHOP_ITEM_FIRST);
			return 0;
		}
	}

	int Side::SuggestedItem() const
	{
		// The build in order; an item counts once per time it appears in the build.
		const HeroDef& d = HeroDefOf(mHero.mHero);
		int aHave[ITEM_COUNT] = {};
		for (uint8_t i : mHero.mItems)
			if (i != ITEM_NONE && i < ITEM_COUNT)
				aHave[i]++;
		for (int k = 0; k < kItemSlots; k++)
		{
			int i = d.mBuild[k];
			if (aHave[i] > 0)
				aHave[i]--;
			else
				return i;
		}
		return ITEM_NONE;
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
		case SHOP_COLLECTOR: if (a.mCollectorLevel >= 2) return No("Stinky is as fast as he gets."); break;
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
		case SHOP_COLLECTOR: a.mCollectorLevel++; break;
		case SHOP_REPAIR_LEFT: case SHOP_REPAIR_RIGHT:
		{
			Hit h;
			h.mTeam = (uint8_t)mTeam;
			h.mHeal = kTowerRepair;
			int i = theShop == SHOP_REPAIR_LEFT ? 0 : 1;
			a.ApplyHit(EntityRef::Of(mTeam, ENT_TOWER, (uint32_t)i), h, mNow);
			Text(mTeam, TankMap().mTower[i] - Vec(0, 90), "+" + std::to_string((int)kTowerRepair), TC_HEAL);
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

	Arena::ClickKind Side::HomeClick(Vec theWhere, float theCoinReach)
	{
		if (Over())
			return Arena::CLICK_NONE;
		// The home window draws coins small: let a click there reach a little farther.
		if (theCoinReach > kCoinClickR)
		{
			float aBest = theCoinReach;
			Vec aAt = theWhere;
			for (const Coin& c : mArena.mCoins)
				if (Dist(c.mPos, theWhere) < aBest)
				{
					aBest = Dist(c.mPos, theWhere);
					aAt = c.mPos;
				}
			theWhere = aAt;
		}
		ArenaContext c;
		c.mOwnerPlayer = mPlayer;
		HeroPresence o;
		if (OtherPresence(mTeam, o))
			c.mHeroes.push_back(o);
		Arena::ClickKind k = mArena.Click(theWhere, mNow, c);
		for (auto& h : mArena.mOutHits)
			Deal(h.first, h.second);
		mArena.mOutHits.clear();
		return k;
	}
}
