#include "HeroesNet.h"

namespace Heroes
{
	// Positions in snapshots: 1/4 unit fixed point (the world is 1280 x 848).
	static void PutPosQ(Writer& w, Vec v) { w.U16((uint16_t)(int16_t)std::lround(v.x * 4)); w.U16((uint16_t)(int16_t)std::lround(v.y * 4)); }
	static Vec GetPosQ(Reader& r) { float x = (int16_t)r.U16() / 4.0f; float y = (int16_t)r.U16() / 4.0f; return Vec(x, y); }

	void Put(Writer& w, const EntityRef& v) { w.U8(v.mArena); w.U8(v.mKind); w.U32(v.mId); }
	bool Get(Reader& r, EntityRef& v) { v.mArena = r.U8(); v.mKind = r.U8(); v.mId = r.U32(); return !r.mBad; }

	void Put(Writer& w, const Hit& v)
	{
		w.F32(v.mDamage); w.I8(v.mPlayer); w.U8(v.mTeam); w.U8(v.mSource);
		w.U16(v.mStunMs); w.U16(v.mSlowMs); w.F32(v.mSlowPct);
		w.Pos(v.mPush); w.U8(v.mPull ? 1 : 0); w.Pos(v.mPullTo);
		w.U16(v.mCharmMs); w.U16(v.mBlindMs); w.F32(v.mHeal); w.F32(v.mShield); w.U16(v.mShieldMs);
	}
	bool Get(Reader& r, Hit& v)
	{
		v.mDamage = r.F32(); v.mPlayer = r.I8(); v.mTeam = r.U8(); v.mSource = r.U8();
		v.mStunMs = r.U16(); v.mSlowMs = r.U16(); v.mSlowPct = r.F32();
		v.mPush = r.Pos(); v.mPull = r.U8() != 0; v.mPullTo = r.Pos();
		v.mCharmMs = r.U16(); v.mBlindMs = r.U16(); v.mHeal = r.F32(); v.mShield = r.F32(); v.mShieldMs = r.U16();
		return !r.mBad;
	}

	void Put(Writer& w, const Reward& v) { w.I8(v.mPlayer); w.I8(v.mTeam); w.F32(v.mXp); w.U32((uint32_t)v.mMoney); w.U8(v.mWhat); }
	bool Get(Reader& r, Reward& v) { v.mPlayer = r.I8(); v.mTeam = r.I8(); v.mXp = r.F32(); v.mMoney = (int)r.U32(); v.mWhat = r.U8(); return !r.mBad; }

	void Put(Writer& w, const Event& v)
	{
		w.U8(v.mType); w.U8(v.mArena); w.I8(v.mPlayer); w.U8(v.mParam); w.U32(v.mId);
		PutPosQ(w, v.mA); PutPosQ(w, v.mB); w.F32(v.mValue); w.U16(v.mMs);
		if (v.mType == EV_TEXT)
			w.Str(v.mText);
	}
	bool Get(Reader& r, Event& v)
	{
		v.mType = r.U8(); v.mArena = r.U8(); v.mPlayer = r.I8(); v.mParam = r.U8(); v.mId = r.U32();
		v.mA = GetPosQ(r); v.mB = GetPosQ(r); v.mValue = r.F32(); v.mMs = r.U16();
		if (v.mType == EV_TEXT)
			v.mText = r.Str();
		return !r.mBad;
	}

	void Put(Writer& w, const HeroSnap& v)
	{
		w.U8(v.mPlayer); w.U8(v.mTeam); w.U8(v.mHero); w.U8(v.mArena); w.U16(v.mFlags);
		w.Pos(v.mPos); w.F32(v.mHp); w.F32(v.mMaxHp); w.F32(v.mShield);
		w.U8(v.mLevel); w.U16(v.mRespawnMs); w.U16(v.mKills); w.U16(v.mDeaths);
		for (int i = 0; i < kItemSlots; i++)
			w.U8(v.mItems[i]);
		w.U16(v.mFishLost); w.U16(v.mTowers); w.U32(v.mEarned);
	}
	bool Get(Reader& r, HeroSnap& v)
	{
		v.mPlayer = r.U8(); v.mTeam = r.U8(); v.mHero = r.U8(); v.mArena = r.U8(); v.mFlags = r.U16();
		v.mPos = r.Pos(); v.mHp = r.F32(); v.mMaxHp = r.F32(); v.mShield = r.F32();
		v.mLevel = r.U8(); v.mRespawnMs = r.U16(); v.mKills = r.U16(); v.mDeaths = r.U16();
		for (int i = 0; i < kItemSlots; i++)
			v.mItems[i] = r.U8();
		v.mFishLost = r.U16(); v.mTowers = r.U16(); v.mEarned = r.U32();
		return !r.mBad && v.mHero < HERO_COUNT && v.mPlayer < kMaxPlayers;
	}

	void Put(Writer& w, const ArenaSnap& v)
	{
		w.U8(v.mTeam);
		for (int i = 0; i < 2; i++)
		{
			w.F32(v.mTowerHp[i]);
			w.F32(v.mTowerShield[i]);
		}
		w.F32(v.mTowerMax); w.U8(v.mTowerLevel); w.U8(v.mTowerBlind);
		w.F32(v.mCoreHp); w.F32(v.mCoreShield); w.U16(v.mFishCount); w.U8(v.mFoodQuality);
		w.U8(v.mCollectorLevel); PutPosQ(w, v.mCollectorPos); w.U8(v.mCollectorRight ? 1 : 0);
		w.U16((uint16_t)v.mFish.size());
		for (const FishSnap& f : v.mFish) { w.U32(f.mId); w.U8(f.mKind); w.U8(f.mSize); w.U8(f.mFlags); PutPosQ(w, f.mPos); }
		w.U16((uint16_t)v.mCoins.size());
		for (const CoinSnap& c : v.mCoins) { w.U32(c.mId); w.U8(c.mKind); PutPosQ(w, c.mPos); }
		w.U16((uint16_t)v.mFood.size());
		for (const FoodSnap& f : v.mFood) { w.U32(f.mId); PutPosQ(w, f.mPos); }
		w.U16((uint16_t)v.mMinions.size());
		for (const MinionSnap& m : v.mMinions) { w.U32(m.mId); w.U8(m.mKind); w.U8(m.mFlags); w.U8(m.mTeam); w.U8((uint8_t)std::lround(Clamp(m.mHpFrac, 0, 1) * 255)); PutPosQ(w, m.mPos); }
	}
	bool Get(Reader& r, ArenaSnap& v)
	{
		v = ArenaSnap();
		v.mTeam = r.U8();
		for (int i = 0; i < 2; i++)
		{
			v.mTowerHp[i] = r.F32();
			v.mTowerShield[i] = r.F32();
		}
		v.mTowerMax = r.F32(); v.mTowerLevel = r.U8(); v.mTowerBlind = r.U8();
		v.mCoreHp = r.F32(); v.mCoreShield = r.F32(); v.mFishCount = r.U16(); v.mFoodQuality = r.U8();
		v.mCollectorLevel = r.U8(); v.mCollectorPos = GetPosQ(r); v.mCollectorRight = r.U8() != 0;
		size_t n = r.U16();
		for (size_t i = 0; i < n && !r.mBad; i++) { FishSnap f; f.mId = r.U32(); f.mKind = r.U8(); f.mSize = r.U8(); f.mFlags = r.U8(); f.mPos = GetPosQ(r); v.mFish.push_back(f); }
		n = r.U16();
		for (size_t i = 0; i < n && !r.mBad; i++) { CoinSnap c; c.mId = r.U32(); c.mKind = r.U8(); c.mPos = GetPosQ(r); v.mCoins.push_back(c); }
		n = r.U16();
		for (size_t i = 0; i < n && !r.mBad; i++) { FoodSnap f; f.mId = r.U32(); f.mPos = GetPosQ(r); v.mFood.push_back(f); }
		n = r.U16();
		for (size_t i = 0; i < n && !r.mBad; i++) { MinionSnap m; m.mId = r.U32(); m.mKind = r.U8(); m.mFlags = r.U8(); m.mTeam = r.U8(); m.mHpFrac = r.U8() / 255.0f; m.mPos = GetPosQ(r); v.mMinions.push_back(m); }
		return !r.mBad;
	}

	///////////////////////////////////////////////////////////////////////////
	// MemoryLink
	///////////////////////////////////////////////////////////////////////////
	void MemoryLink::MakePair(std::unique_ptr<MemoryLink>& a, std::unique_ptr<MemoryLink>& b, uint32_t theDelayMs)
	{
		a.reset(new MemoryLink());
		b.reset(new MemoryLink());
		std::shared_ptr<Pipe> ab(new Pipe()), ba(new Pipe());
		a->mOut = ab;
		a->mIn = ba;
		b->mOut = ba;
		b->mIn = ab;
		a->mDelay = b->mDelay = theDelayMs;
	}

	void MemoryLink::Send(uint8_t theType, const std::vector<uint8_t>& theData)
	{
		Packet p;
		p.mType = theType;
		p.mData = theData;
		mBytesSent += theData.size() + 5;
		mOut->mQueue.push_back({ mNow + mDelay, p });
	}

	bool MemoryLink::Receive(Packet& thePacket)
	{
		if (mIn->mQueue.empty() || (int32_t)(mNow - mIn->mQueue.front().first) < 0)
			return false;
		thePacket = std::move(mIn->mQueue.front().second);
		mIn->mQueue.pop_front();
		return true;
	}
}
