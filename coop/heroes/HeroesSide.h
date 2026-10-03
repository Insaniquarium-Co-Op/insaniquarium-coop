// Pet Heroes - one player's side of a match: their tank (Arena), their hero, the Trench
// when they're team 0 (the host), and a mirror of everything else built from the other
// side's snapshots. A Side is stepped every tick, takes orders from the input or the
// bot, and talks to the other side over a Link.

#ifndef __HEROES_SIDE_H__
#define __HEROES_SIDE_H__

#include "HeroesTrench.h"
#include "HeroesNet.h"
#include <deque>

namespace Heroes
{
	// Something a hero can aim at, in whichever arena it's in.
	struct Target
	{
		EntityRef	mRef;
		Vec			mPos;
		float		mRadius = 20;
		int			mTeam = 0;			// whose side it's on (2: a monster)
		float		mHpFrac = 1;
		bool		mStructure = false;	// tower or core
		bool		mHero = false;
		bool		mMonster = false;
		bool		mAngry = false;		// a monster chasing someone
		bool		mAsleep = false;	// Lullaby: auto-attack leaves it be (damage would wake it)
		uint8_t		mKind = 0;			// MinionKind for minions
	};

	// The other side's tank and hero (and, for team 1, the Trench), as far as we know them.
	class Mirror
	{
	public:
		struct TimedArena { uint32_t mAt; ArenaSnap mSnap; };
		struct TimedHero { uint32_t mAt; HeroSnap mSnap; };
		std::deque<TimedArena>	mArena[kArenaCount];
		std::deque<TimedHero>	mHero[kMaxPlayers];

		// The screen draws the other side this far in the past so movement between
		// snapshots is smooth (D34). Snapshots carry the sender's clock: the delay is one
		// snapshot interval plus the worst lateness seen in the last few seconds, plus a
		// margin. Senders before 2.2.1 don't stamp them: a fixed 90 ms then.
		static const uint32_t	kUnstampedDelayMs = 90, kMinDelayMs = 30, kMaxDelayMs = 120, kMarginMs = 6;
		static const uint32_t	kWindowMs = 3000;
		uint32_t		DelayMs() const { return mDelayMs; }
		uint32_t		TargetDelayMs() const;
		void			UpdateDelay();		// once per step: move toward the target
		// theSentAt < 0: not stamped. The snapshot's time on our clock is returned.
		void			AddArena(uint32_t theNow, const ArenaSnap& theSnap, int64_t theSentAt = -1);
		void			AddHero(uint32_t theNow, const HeroSnap& theSnap, int64_t theSentAt = -1);
		// Diagnostics: lookups that ran past the newest hero snapshot (a stall on screen).
		mutable uint32_t mLookups = 0, mStarved = 0;
		int				mWorstLateMs = 0;
		float			mIntervalMs = 28;

	private:
		uint32_t		LocalTime(uint32_t theNow, int64_t theSentAt);
		struct Sample { uint32_t mAt; int32_t mOffset; };
		std::deque<Sample> mSamples;	// receive time minus send time, last kWindowMs
		bool			mStamped = false;
		int64_t			mLastHeroSentAt = -1;
		uint32_t		mDelayMs = kUnstampedDelayMs;
	public:
		bool			HasArena(int theArena) const { return !mArena[theArena % kArenaCount].empty(); }
		const ArenaSnap* LatestArena(int theArena) const { const auto& q = mArena[theArena % kArenaCount]; return q.empty() ? nullptr : &q.back().mSnap; }
		const HeroSnap*	LatestHero(int thePlayer) const;
		ArenaSnap		ArenaAt(int theArena, uint32_t theTime) const;	// positions interpolated
		bool			HeroAt(int thePlayer, uint32_t theTime, HeroSnap& theOut) const;
	};

	struct Projectile
	{
		uint32_t	mId = 0;
		int			mArena = 0;
		Vec			mPos, mVel;
		float		mLeft = 0;			// distance it may still travel
		float		mRadius = 10;
		Hit			mHit;
		EntityRef	mHoming;			// auto-attacks follow their target
		uint8_t		mLook = LOOK_NONE;
		bool		mChain = false;		// Clyde's Static / Electric Scale
		bool		mStopAtWalls = true;
		bool		mPierce = false;	// hits everything along its way (High Note)
		bool		mAttack = false;	// an auto-attack (lifesteal, on-hit effects)
		bool		mOverload = false;	// Clyde's Overcharge: jump on to a second enemy
		std::vector<EntityRef> mHitAlready;	// piercing shots hit each thing once
	};

	struct Zone
	{
		int			mArena = 0;
		Vec			mPos;
		float		mRadius = 100;
		uint32_t	mUntil = 0, mNextTick = 0;
		float		mDps = 0;
		float		mSlowPct = 0;
		bool		mBlind = false;
		uint8_t		mLook = LOOK_NONE;
		uint8_t		mAbility = kNoAbility;
	};

	// Something that lands later: a lobbed bomb or pearl, a missile, an aftershock, a decoy's blast.
	struct Impact
	{
		int			mArena = 0;
		Vec			mPos;
		uint32_t	mAt = 0;
		float		mRadius = 80;
		Hit			mHit;
		float		mStructMult = 1;	// against towers and the core
		uint8_t		mLook = LOOK_NONE;
		uint8_t		mSound = SND_EXPLODE;
	};

	// Niko's turrets and Shrapnel's mines.
	struct Turret
	{
		uint32_t	mId = 0;
		int			mArena = 0;
		Vec			mPos;
		uint32_t	mUntil = 0, mNextShot = 0;
		bool		mMine = false;		// a mine: armed from mNextShot, bursts on touch
	};

	// Damage over time from my hero (Balrog's burn).
	struct Burn
	{
		EntityRef	mTarget;
		float		mPerTick = 0;
		int			mTicks = 0;
		uint32_t	mNext = 0;
	};

	// A thing to draw for a while (from our own events or the other side's).
	struct Effect
	{
		Event		mEvent;
		uint32_t	mAt = 0;
		uint32_t	mSeq = 0;			// increases by one per effect (sounds play once)
	};

	// What hurt my hero lately (the death recap).
	struct RecapHit
	{
		uint32_t	mAt = 0;
		uint8_t		mSource = SRC_ATTACK;
		int8_t		mPlayer = -1;
		uint8_t		mHero = 255;
		uint8_t		mAbility = kNoAbility;
		float		mDamage = 0;
	};

	struct HeroState
	{
		uint8_t		mHero = HERO_ITCHY;
		int			mPlayer = 0, mTeam = 0;
		int			mArena = 0;				// which tank it's in (kTrench: the Trench)
		Vec			mPos;
		bool		mRight = true;
		float		mHp = 1;
		bool		mAlive = true;
		uint32_t	mRespawnAt = 0;
		int			mLevel = 1;
		float		mXp = 0;
		int			mRank[AB_COUNT] = { 1, 1, 1, 0 };
		int8_t		mTalent[kTalentTiers] = { -1, -1, -1 };
		uint8_t		mItems[kItemSlots] = { ITEM_NONE, ITEM_NONE, ITEM_NONE, ITEM_NONE, ITEM_NONE, ITEM_NONE };
		uint32_t	mReadyAt[AB_COUNT] = { 0, 0, 0, 0 };

		// Orders.
		enum Order : uint8_t { ORD_IDLE, ORD_MOVE, ORD_ATTACK };
		uint8_t		mOrder = ORD_IDLE;
		Vec			mMoveTo;
		std::vector<Vec> mPath;
		EntityRef	mTarget;
		uint32_t	mRepathAt = 0;
		uint32_t	mNextAttack = 0, mAttackingUntil = 0;
		EntityRef	mAutoTarget;			// auto-attack's pick (no order)
		int			mStacks = 0;
		EntityRef	mStackTarget;
		int			mAttackCount = 0;

		// Status.
		uint32_t	mStunUntil = 0, mSleepUntil = 0, mSlowUntil = 0, mUntargetableUntil = 0, mImmuneUntil = 0, mSpeedUntil = 0;
		float		mSlowPct = 0, mSpeedPct = 0;
		float		mShield = 0;
		uint32_t	mShieldUntil = 0, mRevealUntil = 0, mTauntUntil = 0;
		bool		mInPortal = false;
		bool		mWantCross = false;		// ordered into a portal (walkers ride its beam from the floor below)
		uint32_t	mCrossReadyAt = 0;		// no crossing until then (raid lock or the guard)
		bool		mHidden = false;
		uint32_t	mBuffUntil[BUFF_COUNT] = { 0, 0, 0 };
		int			mStreak = 0;			// kills since the last death
		uint32_t	mLastHurtAt = 0, mLastMoveAt = 0;
		uint32_t	mInkReadyAt = 0;		// Ink Sac

		// Abilities in progress.
		int			mSharpenLeft = 0;
		uint32_t	mSharpenUntil = 0;
		uint32_t	mStormUntil = 0, mStormNext = 0;
		int			mThunderLeft = 0;
		uint32_t	mThunderNext = 0;
		uint32_t	mLeapStart = 0, mLeapUntil = 0;
		uint32_t	mHopStart = 0, mHopUntil = 0, mHopReadyAt = 0;	// walkers' W
		Vec			mLeapFrom, mLeapTo;
		uint32_t	mDashUntil = 0;
		Vec			mDashDir;
		float		mDashSpeed = 0;
		bool		mDashHit = false;
		uint8_t		mDashKind = 0;			// 0 Lunge, 1 Blast Jump (no hit)
		uint32_t	mSlimeUntil = 0;
		Vec			mLastSlime;
		uint32_t	mGoldRushUntil = 0;
		uint32_t	mHealUntil = 0, mHealNext = 0;
		float		mHealPerTick = 0;
		EntityRef	mHealTarget;
		uint32_t	mMisdirectUntil = 0;	// Presto: the next attack hits harder
		uint32_t	mDecoyUntil = 0;		// Presto: hidden while his decoy stands
		int8_t		mCopyHero = -1;			// Presto's Copycat
		uint32_t	mCopyUntil = 0;
		uint32_t	mSavedReady[AB_COUNT] = { 0, 0, 0, 0 };
		uint32_t	mClamUntil = 0;			// Niko's Clam Shield
		bool		mClamStun = false;		// the first hero to hit him gets stunned
		uint32_t	mFortressUntil = 0, mFortressNext = 0;
		uint32_t	mAnthemUntil = 0;		// Meryl
		uint32_t	mSpikedUntil = 0;		// Rhubarb's Spiked Shell

		// Score.
		int			mKills = 0, mDeaths = 0, mMinionKills = 0, mTowers = 0, mLaneCoins = 0, mObjectives = 0;
		float		mHeroDamage = 0, mStructDamage = 0;
		float		mGoldFrac = 0;

		int			Look() const { return mCopyHero >= 0 ? mCopyHero : mHero; }		// whom it looks and fights like
		const HeroDef& Def() const { return HeroDefOf(Look()); }
		bool		Evolved() const { return mLevel >= kEvolveLevel; }
	};

	class Side
	{
	public:
		int			mTeam = 0, mPlayer = 0;
		Link*		mLink = nullptr;
		Arena		mArena;
		Trench		mTrench;				// run only by team 0 (the host)
		HeroState	mHero;
		Mirror		mOther;
		int			mOtherPlayer = 1;
		std::vector<Projectile>	mProj;
		std::vector<Zone>		mZones;
		std::vector<Impact>		mImpacts;
		std::vector<Turret>		mTurrets;
		std::vector<Burn>		mBurns;
		std::vector<RecapHit>	mRecap;
		std::vector<Effect>		mEffects;
		uint32_t	mNow = 0, mStart = 0;
		uint32_t	mNextId = 1;
		uint32_t	mEffectSeq = 0;

		// Waves I send.
		uint32_t	mNextWave = 0;
		int			mWaveSizeRank = 0, mWaveToughRank = 0;
		std::vector<uint8_t>	mQueuedAliens;
		int			mWavesSent = 0;

		bool		mSuddenDeath = false;
		bool		mLost = false, mWon = false, mOtherGaveUp = false;
		bool		mFirstBlood = false;			// someone has drawn first blood (as far as we know)
		uint32_t	mHurtByAt[kMaxPlayers] = { 0, 0, 0, 0 };	// my hero was hurt by that player
		uint32_t	mHitHeroAt = 0;					// my hero last hurt the other hero
		std::string	mLastRefusal;

		void		Init(int theTeam, int thePlayer, int theHero, int theOtherPlayer, uint64_t theSeed, uint32_t theNow);
		void		Step(uint32_t theNow);
		uint32_t	MatchMs() const { return mNow - mStart; }
		bool		Over() const { return mLost || mWon; }
		bool		Owns(int theArena) const { return theArena == mTeam || (theArena == kTrench && mTeam == 0); }

		// ---- orders (input and bot) ----
		void		OrderMove(Vec theWhere);
		void		OrderAttack(const EntityRef& theTarget);
		void		OrderStop();
		void		Steer(Vec theDir);					// WASD, every tick (zero: no keys held); cancels orders
		bool		Hop();								// walkers: W
		bool		CrossNearby();						// walkers: S near a portal or a pad
		bool		Cast(int theSlot, Vec theAim);
		bool		CanCast(int theSlot, std::string* theWhy = nullptr) const;
		int			PendingTalent() const;				// the tier waiting for a pick, or -1
		bool		PickTalent(int theChoice);			// 0 left, 1 right
		bool		HasTalent(int theTier, int theChoice) const;
		Arena::ClickKind HomeClick(Vec theWhere, float theCoinReach = kCoinClickR);
		bool		Buy(int theShop);
		int			Price(int theShop) const;
		bool		CanBuy(int theShop, std::string* theWhy = nullptr) const;
		int			Owned(int theShop) const;			// levels/items owned (for the shop)
		int			SuggestedItem() const;				// the next item of my hero's build (ITEM_NONE: done)
		void		GiveUp();
		void		TestXp(float theXp) { GainXp(theXp); }	// tests and the tutorial

		// ---- the hero's numbers ----
		float		MaxHp() const;
		float		Damage() const;
		float		Speed() const;
		float		AttackPeriod() const;
		float		CooldownMult() const;
		float		AbilityScale(int theSlot) const;
		float		ArmorVsStructures() const;			// share taken from towers and lasers
		float		StructBonus() const;
		float		Lifesteal() const;
		int			ItemCount(int theItem) const;
		bool		HasItem(int theItem) const { return ItemCount(theItem) > 0; }
		bool		HasBuff(int theBuff) const { return theBuff < BUFF_COUNT && (int32_t)(mNow - mHero.mBuffUntil[theBuff]) < 0; }
		uint32_t	CooldownLeft(int theSlot) const;
		uint32_t	CooldownTotal(int theSlot) const;	// what the last cast set (for the HUD)
		bool		Stunned() const { return (int32_t)(mNow - mHero.mStunUntil) < 0 || (int32_t)(mNow - mHero.mSleepUntil) < 0; }
		uint32_t	WarpSicknessLeft() const { return (int32_t)(mNow - mHero.mCrossReadyAt) >= 0 ? 0 : mHero.mCrossReadyAt - mNow; }
		bool		Busy() const;						// dashing or leaping
		bool		Hopping() const;					// a walker in the air (W)
		bool		Rooted() const;						// Fortress

		// ---- what's around ----
		// Everything targetable in that arena. Gameplay uses the newest positions; the screen
		// (and clicking on what you see) the smoothed ones, a moment behind.
		void		Targets(int theArena, std::vector<Target>& theOut, bool theAsDrawn = false) const;
		bool		FindTarget(const EntityRef& theRef, Target& theOut) const;
		const HeroSnap*	OtherHero() const { return mOther.LatestHero(mOtherPlayer); }
		bool		OtherHeroIn(int theArena, HeroSnap* theOut = nullptr, bool theAsDrawn = false) const;
		HeroSnap	MySnap() const;
		const ArenaSnap* LatestArena(int theArena) const;	// the newest known state of any arena (mine: none, use ViewArena)
		uint32_t	MonsterIn(int theSlot) const;		// ms until that monster is back (0: there)
		bool		MonsterAlive(int theSlot, Vec* thePos = nullptr, float* theHpFrac = nullptr) const;

		// For drawing.
		ArenaSnap	ViewArena(int theArena) const;		// mine live, the others interpolated
		void		ViewHeroes(int theArena, std::vector<HeroSnap>& theOut) const;

	private:
		void		Receive(const Packet& p);
		void		StepHero(float theDt);
		void		StepAbilities(float theDt);
		void		StepMovement(float theDt);
		void		StepAttack();
		void		StepProjectiles(float theDt);
		void		StepZones();
		void		StepImpacts();
		void		StepTurrets();
		void		StepBurns();
		void		StepArena();
		void		StepTrench();
		void		StepWaves();
		void		SendState();
		void		Deal(const EntityRef& theTarget, const Hit& theHit);
		void		ApplyToMyHero(const Hit& theHit);
		void		GainReward(const Reward& r);
		void		GainXp(float theXp);
		void		Emit(const Event& e);				// show here and send to the other side
		void		Local(const Event& e);				// show here only
		void		Text(int theArena, Vec theAt, const std::string& theText, uint8_t theColor);
		void		Sound(int theArena, uint8_t theSound, Vec theAt);
		void		Burst(int theArena, Vec theAt, float theRadius, uint8_t theLook);
		void		Announce(uint8_t theKind, int thePlayer, int theOther = -1, float theValue = 0);
		void		Respawn();
		void		Die(const Hit& theKiller);
		void		CrossTo(int theArena, Vec thePos);
		bool		ArenaHasOpenCore(int theArena) const;
		void		EnemiesNear(int theArena, Vec thePos, float theRadius, std::vector<Target>& theOut, bool theStructures, bool theMonsters = true) const;
		Hit			MakeHit(float theDamage, uint8_t theSource, uint8_t theAbility = kNoAbility) const;
		void		AttackLanded(const Target& t, float theDamage, bool theChain);
		void		AbilityHit(const Target& t, Hit theHit, float theStructMult = 1);	// ability damage with its bonuses
		Vec			SpawnPoint() const;
		Vec			ExitPoint(int theArena, int theGate) const;
		void		Arrivals(int theTeam, const std::vector<Arrival>& theMinions);
		void		ApplyRewardsFrom(std::vector<Reward>& theRewards);
		HeroPresence MyPresence() const;
		bool		OtherPresence(int theArena, HeroPresence& theOut) const;
		void		EndCopy();
		void		StartBuff(int theBuff);
		void		CheckInk();
		Target		AutoAcquire() const;				// the best enemy in reach, or none

		void		StepSteer(float theDt);
		void		CastSwitch(int theSlot, const AbilityDef& a, Vec theAim, Vec aDir, Vec aPoint, float aScale);

		Vec			mSteer;
		float		mSlideSide = 1;					// which way WASD slides around a wall
		uint32_t	mLastBumpAt = 0;				// the last sand puff (EV_BUMP)
		uint32_t	mCastTotal[AB_COUNT] = { 1, 1, 1, 1 };
		std::vector<std::pair<EntityRef, Hit>>	mOutHits;
		std::vector<Reward>		mOutRewards;
		std::vector<Event>		mOutEvents;
		uint32_t	mLastHeroSent = 0, mLastArenaSent = 0, mLastTrenchSent = 0, mLastStep = 0;
		float		mPassiveGold = 0;
	};
}

#endif
