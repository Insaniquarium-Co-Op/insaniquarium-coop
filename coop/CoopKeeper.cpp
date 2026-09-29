#include <SexyAppFramework/Font.h>
#include "CoopKeeper.h"
#include "CoopRace.h"
#include "CoopSession.h"
#include "CoopProtocol.h"
#include "CoopNet.h"
#include "WinFishApp.h"
#include "Board.h"
#include "Alien.h"
#include "Fish.h"
#include "Breeder.h"
#include "Res.h"
#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/Image.h>
#include <SexyAppFramework/GLImage.h>
#include <SexyAppFramework/Widget.h>
#include <SexyAppFramework/WidgetManager.h>
#include <SexyAppFramework/KeyCodes.h>
#include <SexyAppFramework/MusicInterface.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

using namespace Sexy;

namespace Coop
{
	///////////////////////////////////////////////////////////////////////////
	// Tuning (first guesses: see docs/PROJECT.md)
	///////////////////////////////////////////////////////////////////////////
	struct KeeperAlienInfo
	{
		const char*	mName;
		const char*	mShort;		// fits a shop button
		int			mBabyCost;
		int			mCrystal;	// an adult's crystal; a juvenile's is half
		int			mFromTank;	// first tank (1-4) whose lair can raise it
		int			mGameType;	// ALIEN_*
	};
	static const KeeperAlienInfo kAliens[KA_COUNT] =
	{
		{ "Sylvester",     "Sylv",   100,  30, 1, ALIEN_WEAK_SYLV },
		{ "Big Sylvester", "Big Sy", 200,  42, 2, ALIEN_STRONG_SYLV },
		{ "Gus",           "Gus",    250,  48, 2, ALIEN_GUS },
		{ "Balrog",        "Balrog", 400,  72, 3, ALIEN_BALROG },
		{ "Destructor",    "Destro", 450,  72, 3, ALIEN_DESTRUCTOR },
		{ "Psychosquid",   "Squid",  700, 108, 4, ALIEN_PSYCHOSQUID },
	};

	static const int	kStartMoney = 250;
	static const int	kGooCost = 5;
	static const int	kShieldCost = 60, kShieldMs = 5000;
	static const int	kFrenzyCost = 40, kFrenzyMs = 8000;
	static const int	kBlackoutCost = 150, kBlackoutMs = 6000, kBlackoutCooldownMs = 30000;
	static const int	kMealsToJuvenile = 3, kMealsToAdult = 4;
	static const int	kPeckishMs = 5000, kHungryMs = 15000, kStarveMs = 45000;
	static const int	kCrystalEveryMs = 7000;
	static const int	kMaxLairAliens = 8, kMaxGoo = 6;
	static const int	kWarpMs = 2500;			// siren and crosshair before an alien lands
	static const int	kPortalGapMs = 3000;	// a wave comes through one alien at a time
	static const double	kFrenzySpeed = 1.8;

	// Tests shorten the round and the grace period.
	static int EnvMs(const char* theName, int theDefault)
	{
		const char* v = getenv(theName);
		return v != nullptr && atoi(v) > 0 ? atoi(v) : theDefault;
	}
	static int RoundMs() { return EnvMs("INSANIQ_KEEPER_ROUND_MS", 480000); }
	static int GraceMs() { return EnvMs("INSANIQ_KEEPER_GRACE_MS", 45000); }

	enum KeeperEvent { KE_BOUNTY = 1, KE_ALIEN_DOWN, KE_LANDED, KE_CLOCK };
	enum LaunchFlags { LF_SHIELD = 1, LF_FRENZY = 2, LF_JUVENILE = 4 };
	static const double	kJuvenileHealth = 0.55;	// a juvenile lands at about half an adult's strength
	static const int	kArrivalShieldMs = 1000;	// every alien comes through the portal shielded

	// The lair screen: top bar, the tank, bottom bar.
	static const int kTopH = 62, kBottomY = 402;
	static const int kTankTop = kTopH, kTankBottom = kBottomY;
	static const int kPortalX = 588, kPortalY = 250;
	static const int kMapX = 488, kMapY = 406, kMapW = 146, kMapH = 70;

	///////////////////////////////////////////////////////////////////////////
	// State
	///////////////////////////////////////////////////////////////////////////
	class LairTank;

	enum AlienState { AS_LIVING, AS_SENDING, AS_DYING };

	struct LairAlien
	{
		int			mId = 0;
		int			mKind = 0;
		int			mStage = 0;			// 0 baby, 1 juvenile, 2 adult
		int			mMeals = 0;
		double		mX = 300, mY = 200, mVX = 0, mVY = 0;
		double		mTX = 300, mTY = 200;
		uint32_t	mLastMeal = 0;
		uint32_t	mNextCrystal = 0;
		uint32_t	mNextWander = 0;
		bool		mSelected = false;
		bool		mShield = false, mFrenzy = false;
		int			mState = AS_LIVING;
		uint32_t	mStateAt = 0;
		int			mLandX = -1, mLandY = -1;	// where it lands in the fish tank (-1: anywhere)
		bool		mFacingRight = false;
	};

	struct Goo
	{
		double		mX, mY;
		uint32_t	mLandedAt = 0;
	};

	struct Crystal
	{
		double		mX, mY;
		int			mValue;
		uint32_t	mLandedAt = 0;
	};

	struct PendingLaunch
	{
		int			mKind;
		int			mX, mY;
		int			mFlags;
		uint32_t	mAt;
	};

	struct AlienBuff
	{
		uint32_t	mShieldUntil = 0, mFrenzyUntil = 0;
		uint32_t	mLandedAt = 0;
	};

	struct KeeperState
	{
		bool		mActive = false;
		bool		mFish = false;			// I keep the fish this round

		// ---- the lair (alien keeper) ----
		int			mMoney = 0;
		std::vector<LairAlien> mAliens;
		std::vector<Goo> mGoo;
		std::vector<Crystal> mCrystals;
		int			mNextId = 1;
		uint32_t	mLastTick = 0;
		int			mMarkerX = -1, mMarkerY = -1;	// chosen landing spot in the fish tank
		uint32_t	mBlackoutReadyAt = 0;
		bool		mTabView = false;
		bool		mHunting = false;
		int			mCurX = 320, mCurY = 240;
		bool		mCursorDirty = false;
		uint32_t	mLastCursorSent = 0, mLastLairSent = 0, mLastDefeatCheck = 0;
		int			mSent = 0, mEaten = 0, mInFlight = 0;
		uint32_t	mForfeitArmedAt = 0;
		std::string	mNote;
		uint32_t	mNoteAt = 0;
		LairTank*	mWidget = nullptr;
		struct FloatText { double mX, mY; std::string mText; Color mColor; uint32_t mBorn; };
		std::vector<FloatText> mFloats;
		// The round clock as the fish keeper counts it (their pauses don't count).
		int			mFishRunMs = -1;
		int			mBrokeChecks = 0;		// consecutive checks that found the lair out of aliens and money
		uint32_t	mFishRunAt = 0;
		bool		mFishPaused = false;

		// ---- the tank (fish keeper) ----
		std::vector<PendingLaunch> mLaunches;
		std::map<GameObject*, AlienBuff> mBuffs;
		uint32_t	mBlackoutUntil = 0, mBlackoutStart = 0;
		bool		mRivalHunting = false;
		int			mRivalX = 0, mRivalY = 0;
		bool		mLairKnown = false;
		int			mLairMoney = 0, mLairAliens = 0, mLairGrown = 0;
		int			mShotDown = 0;
		uint32_t	mPausedMs = 0, mLastFishTick = 0, mLastClockSent = 0;
		int			mWaveCount = 0, mWaveKind = 0;
		uint32_t	mWaveToastAt = 0;
		uint32_t	mLastPortalAt = 0;		// when the last alien came through
	};
	static KeeperState gK;

	static WinFishApp* App() { return (WinFishApp*)gSexyAppBase; }
	static uint32_t Now() { return NetMillis(); }
	static bool Elapsed(uint32_t theDeadline) { return (int32_t)(Now() - theDeadline) >= 0; }
	static int Them() { return 1 - S().LocalPlayer(); }
	static int Rand(int n) { return n > 0 ? (int)(App()->mSeed->Next() % (unsigned)n) : 0; }

	static void Send(uint8_t theType, const ByteWriter& w)
	{
		S().SendMsg(theType, w.mData);
	}

	static void PlayLocal(int theSound)
	{
		if (App() != nullptr)
			App()->PlaySample(theSound);
	}

	// The fish keeper's tank, while the round runs on it.
	static Board* FishTank()
	{
		if (!gK.mActive || !gK.mFish)
			return nullptr;
		Board* b = RaceTankBoard();
		return b != nullptr && App()->mBoard == b ? b : nullptr;
	}

	// The round clock: the fish keeper's running time without their pauses. The lair
	// follows the fish keeper's reports so both screens agree.
	static int KeeperRunMs()
	{
		if (!RaceRunning())
			return 0;
		if (gK.mFish)
			return std::max(0, (int)RaceRunningMs() - (int)gK.mPausedMs);
		if (gK.mFishRunMs < 0)
			return (int)RaceRunningMs();
		return gK.mFishRunMs + (gK.mFishPaused ? 0 : (int)std::min<uint32_t>(Now() - gK.mFishRunAt, 2000));
	}

	static bool InGrace() { return KeeperRunMs() < GraceMs(); }

	static void Note(const std::string& theText)
	{
		gK.mNote = theText;
		gK.mNoteAt = Now();
	}

	static void Float(double x, double y, const std::string& theText, const Color& theColor)
	{
		gK.mFloats.push_back({ x, y, theText, theColor, Now() });
	}

	static std::string Money(int v)
	{
		char aBuf[32];
		if (v >= 1000)
			snprintf(aBuf, sizeof(aBuf), "%d,%03d", v / 1000, v % 1000);
		else
			snprintf(aBuf, sizeof(aBuf), "%d", v);
		return aBuf;
	}

	static std::string ClockText(int theMs)
	{
		int s = std::max(0, theMs) / 1000;
		char aBuf[16];
		snprintf(aBuf, sizeof(aBuf), "%d:%02d", s / 60, s % 60);
		return aBuf;
	}

	///////////////////////////////////////////////////////////////////////////
	// The lair (alien keeper)
	///////////////////////////////////////////////////////////////////////////
	static bool KindAvailable(int theKind) { return RaceRoundTank() >= kAliens[theKind].mFromTank; }
	static int PortalFee(int theKind) { return kAliens[theKind].mBabyCost / 4; }
	static bool Grown(const LairAlien& a) { return a.mStage >= 1; }
	static int CrystalValue(const LairAlien& a) { return a.mStage >= 2 ? kAliens[a.mKind].mCrystal : std::max(10, kAliens[a.mKind].mCrystal / 2); }
	static int Size(const LairAlien& a) { return a.mStage == 0 ? 52 : (a.mStage == 1 ? 82 : 116); }

	static int CheapestBaby()
	{
		int aMin = 1 << 30;
		for (int i = 0; i < KA_COUNT; i++)
			if (KindAvailable(i))
				aMin = std::min(aMin, kAliens[i].mBabyCost);
		return aMin;
	}

	static int CountLiving(bool theGrownOnly)
	{
		int n = 0;
		for (const LairAlien& a : gK.mAliens)
			if (a.mState == AS_LIVING && (!theGrownOnly || Grown(a)))
				n++;
		return n;
	}

	static int CountSelected()
	{
		int n = 0;
		for (const LairAlien& a : gK.mAliens)
			if (a.mState == AS_LIVING && a.mSelected)
				n++;
		return n;
	}

	static bool Buy(int theKind)
	{
		if (gK.mFish || theKind < 0 || theKind >= KA_COUNT || !RaceRunning())
			return false;
		const KeeperAlienInfo& k = kAliens[theKind];
		if (!KindAvailable(theKind))
		{
			Note(std::string(k.mName) + " lives in Tank " + std::to_string(k.mFromTank) + "'s lair and later.");
			PlayLocal(SOUND_BUZZER);
			return false;
		}
		if ((int)gK.mAliens.size() >= kMaxLairAliens)
		{
			Note("The lair is full (8 aliens).");
			PlayLocal(SOUND_BUZZER);
			return false;
		}
		if (gK.mMoney < k.mBabyCost)
		{
			Note("Not enough money for a baby " + std::string(k.mName) + ".");
			PlayLocal(SOUND_BUZZER);
			return false;
		}
		gK.mMoney -= k.mBabyCost;
		LairAlien a;
		a.mId = gK.mNextId++;
		a.mKind = theKind;
		a.mX = kPortalX - 40;
		a.mY = kPortalY;
		a.mVX = -60;
		a.mTX = 120 + Rand(360);
		a.mTY = kTankTop + 60 + Rand(240);
		a.mLastMeal = Now();
		a.mNextWander = Now() + 2000;
		gK.mAliens.push_back(a);
		PlayLocal(SOUND_HATCH);
		S().Log("Keeper: bought a baby %s", k.mName);
		return true;
	}

	static bool DropGoo(int theX, int theY)
	{
		if ((int)gK.mGoo.size() >= kMaxGoo || !RaceRunning())
			return false;
		if (gK.mMoney < kGooCost)
		{
			Note("Not enough money for goo.");
			return false;
		}
		gK.mMoney -= kGooCost;
		gK.mGoo.push_back({ (double)std::clamp(theX, 30, 560), (double)std::clamp(theY, kTankTop + 10, kTankBottom - 30), 0 });
		PlayLocal(SOUND_DROPFOOD);
		return true;
	}

	static bool BuyBuff(int theBuff)
	{
		// One power-up per alien: an alien both shielded and frenzied ate a tank in seconds.
		int aCost = theBuff == 0 ? kShieldCost : kFrenzyCost;
		int aDone = 0, aTaken = 0;
		for (LairAlien& a : gK.mAliens)
		{
			if (a.mState != AS_LIVING || !a.mSelected || !Grown(a))
				continue;
			if (a.mShield || a.mFrenzy)
			{
				aTaken++;
				continue;
			}
			if (gK.mMoney < aCost)
			{
				Note("Not enough money for more power-ups.");
				break;
			}
			gK.mMoney -= aCost;
			(theBuff == 0 ? a.mShield : a.mFrenzy) = true;
			aDone++;
		}
		if (aDone > 0)
			PlayLocal(theBuff == 0 ? SOUND_ZZAM : SOUND_ROAR);
		else if (CountSelected() == 0)
			Note("Select grown aliens first (click them).");
		else if (aTaken > 0)
			Note("Each alien can carry one power-up: Shield or Frenzy.");
		return aDone > 0;
	}

	static int SendFee()
	{
		int aFee = 0;
		for (const LairAlien& a : gK.mAliens)
			if (a.mState == AS_LIVING && a.mSelected && Grown(a))
				aFee += PortalFee(a.mKind);
		return aFee;
	}

	static bool SendSelected()
	{
		if (!RaceRunning())
			return false;
		if (InGrace())
		{
			Note("The portal opens in " + std::to_string((GraceMs() - KeeperRunMs()) / 1000 + 1) + " seconds.");
			PlayLocal(SOUND_BUZZER);
			return false;
		}
		int aCount = 0, aFee = SendFee();
		for (const LairAlien& a : gK.mAliens)
			if (a.mState == AS_LIVING && a.mSelected && Grown(a))
				aCount++;
		if (aCount == 0)
		{
			Note("Select grown aliens to send (click them).");
			return false;
		}
		if (gK.mMoney < aFee)
		{
			Note("The portal fee is " + Money(aFee) + ".");
			PlayLocal(SOUND_BUZZER);
			return false;
		}
		gK.mMoney -= aFee;
		int i = 0;
		for (LairAlien& a : gK.mAliens)
		{
			if (a.mState != AS_LIVING || !a.mSelected || !Grown(a))
				continue;
			a.mState = AS_SENDING;
			a.mStateAt = Now();
			a.mSelected = false;
			if (gK.mMarkerX >= 0)
			{
				// A wave spreads out around the chosen spot.
				int aDX = (i % 2 == 0 ? 1 : -1) * ((i + 1) / 2) * 60;
				a.mLandX = std::clamp(gK.mMarkerX + aDX, 40, 600);
				a.mLandY = std::clamp(gK.mMarkerY + (i % 3 - 1) * 30, 90, 380);
			}
			i++;
		}
		PlayLocal(SOUND_UNLEASH);
		Note(aCount == 1 ? "Through the portal!" : std::to_string(aCount) + " aliens through the portal!");
		return true;
	}

	static void AlienArrivedAtPortal(const LairAlien& a)
	{
		ByteWriter w;
		w.U8((uint8_t)a.mKind);
		w.I16(a.mLandX);
		w.I16(a.mLandY);
		w.U8((uint8_t)((a.mShield ? LF_SHIELD : 0) | (a.mFrenzy ? LF_FRENZY : 0) | (a.mStage < 2 ? LF_JUVENILE : 0)));
		Send(MSG_KEEPER_LAUNCH, w);
		gK.mSent++;
		gK.mInFlight++;
		S().Log("Keeper: sent %s%s%s", kAliens[a.mKind].mName, a.mShield ? " (shield)" : "", a.mFrenzy ? " (frenzy)" : "");
	}

	static bool UseBlackout()
	{
		if (!RaceRunning() || InGrace() || !Elapsed(gK.mBlackoutReadyAt))
		{
			if (RaceRunning() && InGrace())
				Note("Powers wake up when the portal opens.");
			return false;
		}
		if (gK.mMoney < kBlackoutCost)
		{
			Note("Blackout costs " + Money(kBlackoutCost) + ".");
			PlayLocal(SOUND_BUZZER);
			return false;
		}
		gK.mMoney -= kBlackoutCost;
		gK.mBlackoutReadyAt = Now() + kBlackoutCooldownMs;
		ByteWriter w;
		w.U8(0);
		Send(MSG_KEEPER_POWER, w);
		PlayLocal(SOUND_EVILLAFF);
		Note("Blackout!");
		return true;
	}

	static void SetHunt(bool theOn, int theX, int theY)
	{
		if (gK.mHunting != theOn || theX != gK.mCurX || theY != gK.mCurY)
			gK.mCursorDirty = true;
		gK.mHunting = theOn;
		gK.mCurX = theX;
		gK.mCurY = theY;
	}

	static void SendCursor(bool theForce)
	{
		if (!gK.mCursorDirty && !theForce)
			return;
		if (!theForce && !Elapsed(gK.mLastCursorSent + 50))
			return;
		ByteWriter w;
		w.I16(gK.mCurX);
		w.I16(gK.mCurY);
		w.U8(gK.mHunting ? 1 : 0);
		Send(MSG_KEEPER_CURSOR, w);
		gK.mCursorDirty = false;
		gK.mLastCursorSent = Now();
	}

	static void TryForfeit()
	{
		if (gK.mForfeitArmedAt != 0 && !Elapsed(gK.mForfeitArmedAt + 3000))
		{
			gK.mForfeitArmedAt = 0;
			RaceKeeperForfeit();
			return;
		}
		gK.mForfeitArmedAt = Now();
		Note("Press Esc again to give up the round.");
	}

	// One simulation step of the lair (theDt in seconds).
	static void UpdateLair(double theDt)
	{
		uint32_t aNow = Now();
		// Goo sinks, and melts a moment after landing.
		for (size_t i = 0; i < gK.mGoo.size();)
		{
			Goo& g = gK.mGoo[i];
			if (g.mLandedAt == 0)
			{
				g.mY += 45 * theDt;
				if (g.mY >= kTankBottom - 20)
				{
					g.mY = kTankBottom - 20;
					g.mLandedAt = aNow;
				}
			}
			if (g.mLandedAt != 0 && aNow - g.mLandedAt > 1500)
				gK.mGoo.erase(gK.mGoo.begin() + i);
			else
				i++;
		}
		// Crystals drift down and fade away if nobody picks them up.
		for (size_t i = 0; i < gK.mCrystals.size();)
		{
			Crystal& c = gK.mCrystals[i];
			if (c.mLandedAt == 0)
			{
				c.mY += 30 * theDt;
				if (c.mY >= kTankBottom - 26)
				{
					c.mY = kTankBottom - 26;
					c.mLandedAt = aNow;
				}
			}
			if (c.mLandedAt != 0 && aNow - c.mLandedAt > 10000)
				gK.mCrystals.erase(gK.mCrystals.begin() + i);
			else
				i++;
		}

		for (size_t i = 0; i < gK.mAliens.size();)
		{
			LairAlien& a = gK.mAliens[i];
			if (a.mState == AS_DYING)
			{
				if (aNow - a.mStateAt > 1000)
				{
					gK.mAliens.erase(gK.mAliens.begin() + i);
					continue;
				}
				i++;
				continue;
			}
			if (a.mState == AS_SENDING)
			{
				double dx = kPortalX - a.mX, dy = kPortalY - a.mY, d = std::sqrt(dx * dx + dy * dy);
				if (d < 12)
				{
					AlienArrivedAtPortal(a);
					gK.mAliens.erase(gK.mAliens.begin() + i);
					continue;
				}
				double v = std::min(d, 260 * theDt);
				a.mX += dx / d * v;
				a.mY += dy / d * v;
				a.mFacingRight = dx > 0;
				i++;
				continue;
			}

			uint32_t aHunger = aNow - a.mLastMeal;
			if (aHunger >= (uint32_t)kStarveMs)
			{
				a.mState = AS_DYING;
				a.mStateAt = aNow;
				a.mSelected = false;
				PlayLocal(SOUND_DIE);
				Note("A " + std::string(kAliens[a.mKind].mName) + " starved. Feed them goo!");
				i++;
				continue;
			}

			// Hungry aliens go for the nearest goo; the rest wander.
			int aTarget = -1;
			if (aHunger >= (uint32_t)kPeckishMs)
			{
				double aBest = 1e18;
				for (size_t j = 0; j < gK.mGoo.size(); j++)
				{
					double dx = gK.mGoo[j].mX - a.mX, dy = gK.mGoo[j].mY - a.mY, d = dx * dx + dy * dy;
					if (d < aBest)
					{
						aBest = d;
						aTarget = (int)j;
					}
				}
			}
			if (aTarget >= 0)
			{
				a.mTX = gK.mGoo[aTarget].mX;
				a.mTY = gK.mGoo[aTarget].mY;
			}
			else if (Elapsed(a.mNextWander) || std::fabs(a.mTX - a.mX) + std::fabs(a.mTY - a.mY) < 20)
			{
				a.mTX = 60 + Rand(460);
				a.mTY = kTankTop + 50 + Rand(kTankBottom - kTankTop - 110);
				a.mNextWander = aNow + 3000 + Rand(3000);
			}
			double aSpeed = (a.mStage == 0 ? 50 : (a.mStage == 1 ? 60 : 70)) * (aTarget >= 0 ? 1.6 : 1.0);
			double dx = a.mTX - a.mX, dy = a.mTY - a.mY, d = std::sqrt(dx * dx + dy * dy);
			double aWantVX = d > 1 ? dx / d * aSpeed : 0, aWantVY = d > 1 ? dy / d * aSpeed : 0;
			double k = std::min(1.0, 3.0 * theDt);
			a.mVX += (aWantVX - a.mVX) * k;
			a.mVY += (aWantVY - a.mVY) * k;
			a.mX = std::clamp(a.mX + a.mVX * theDt, 40.0, 560.0);
			a.mY = std::clamp(a.mY + a.mVY * theDt, (double)kTankTop + 30, (double)kTankBottom - 40);
			if (std::fabs(a.mVX) > 5)
				a.mFacingRight = a.mVX > 0;

			// Eat goo in reach.
			if (aTarget >= 0 && d < 16 + Size(a) * 0.2)
			{
				gK.mGoo.erase(gK.mGoo.begin() + aTarget);
				a.mLastMeal = aNow;
				a.mMeals++;
				PlayLocal(SOUND_SLURP);
				if ((a.mStage == 0 && a.mMeals >= kMealsToJuvenile) || (a.mStage == 1 && a.mMeals >= kMealsToAdult))
				{
					a.mStage++;
					a.mMeals = 0;
					a.mNextCrystal = aNow + kCrystalEveryMs;
					PlayLocal(SOUND_GROW);
					Float(a.mX, a.mY - Size(a) / 2, a.mStage == 1 ? "Juvenile!" : "Adult!", Color(140, 255, 160));
				}
			}

			// Grown aliens drop crystals.
			if (Grown(a) && Elapsed(a.mNextCrystal))
			{
				gK.mCrystals.push_back({ a.mX, a.mY + 10, CrystalValue(a), 0 });
				a.mNextCrystal = aNow + kCrystalEveryMs + Rand(1500);
			}
			i++;
		}
	}

	static bool CollectCrystalAt(int x, int y)
	{
		for (size_t i = 0; i < gK.mCrystals.size(); i++)
		{
			Crystal& c = gK.mCrystals[i];
			if (std::fabs(c.mX - x) < 24 && std::fabs(c.mY - y) < 24)
			{
				gK.mMoney += c.mValue;
				Float(c.mX, c.mY - 20, "+$" + std::to_string(c.mValue), Color(255, 150, 255));
				gK.mCrystals.erase(gK.mCrystals.begin() + i);
				PlayLocal(SOUND_DIAMOND);
				return true;
			}
		}
		return false;
	}

	static LairAlien* AlienAt(int x, int y)
	{
		LairAlien* aBest = nullptr;
		for (LairAlien& a : gK.mAliens)
		{
			if (a.mState != AS_LIVING)
				continue;
			int r = Size(a) / 2 - 6;
			if (std::fabs(a.mX - x) < r && std::fabs(a.mY - y) < r)
				aBest = &a;
		}
		return aBest;
	}

	///////////////////////////////////////////////////////////////////////////
	// Drawing helpers
	///////////////////////////////////////////////////////////////////////////
	static Image* AlienImage(int theKind)
	{
		switch (theKind)
		{
		case KA_SYLV:
		case KA_BIGSYLV:	return IMAGE_SYLV;
		case KA_GUS:		return IMAGE_GUS;
		case KA_BALROG:		return IMAGE_BALROG;
		case KA_DESTRUCTOR:	return IMAGE_DESTRUCTOR;
		case KA_SQUID:		return IMAGE_PSYCHOSQUID;
		default:			return nullptr;
		}
	}

	static void DrawAlienSprite(Graphics* g, int theKind, int theStage, int cx, int cy, int theSize, int theFrame, bool theMirror, const Color* theTint)
	{
		Image* anImg = AlienImage(theKind);
		int aCel = 160;
		if (theKind == KA_SYLV && theStage == 0 && IMAGE_MINISYLV != nullptr)
		{
			anImg = IMAGE_MINISYLV;		// a real baby Sylvester
			aCel = 80;
		}
		if (anImg == nullptr)
			return;
		Color aTint = theTint != nullptr ? *theTint : (theKind == KA_BIGSYLV ? Color(255, 170, 120) : Color(255, 255, 255));
		bool aColorize = theTint != nullptr || theKind == KA_BIGSYLV;
		if (aColorize)
		{
			g->SetColorizeImages(true);
			g->SetColor(aTint);
		}
		Rect aSrc((theFrame % 10) * aCel, 0, aCel, aCel);
		g->DrawImageMirror(anImg, Rect(cx - theSize / 2, cy - theSize / 2, theSize, theSize), aSrc, theMirror);
		g->SetColorizeImages(false);
	}

	static void Disc(Graphics* g, int x, int y, int r, const Color& c)
	{
		Point p[16];
		for (int k = 0; k < 16; k++)
			p[k] = Point(x + (int)(r * std::cos(k * 6.2832 / 16)), y + (int)(r * std::sin(k * 6.2832 / 16)));
		g->SetColor(c);
		g->PolyFill(p, 16, true);
	}

	static void Ring(Graphics* g, int x, int y, int r, const Color& c)
	{
		g->SetColor(c);
		for (int t = 0; t < 2; t++)
		{
			int rr = r + t;
			for (int k = 0; k < 28; k++)
			{
				double a0 = k * 6.2832 / 28, a1 = (k + 1) * 6.2832 / 28;
				g->DrawLine(x + (int)(rr * std::cos(a0)), y + (int)(rr * std::sin(a0)), x + (int)(rr * std::cos(a1)), y + (int)(rr * std::sin(a1)));
			}
		}
	}

	static void Reticle(Graphics* g, int x, int y, int r, const Color& c)
	{
		Ring(g, x, y, r, c);
		g->DrawLine(x - r - 6, y, x - r + 6, y);
		g->DrawLine(x + r - 6, y, x + r + 6, y);
		g->DrawLine(x, y - r - 6, x, y - r + 6);
		g->DrawLine(x, y + r - 6, x, y + r + 6);
	}

	static void Centered(Graphics* g, Sexy::Font* f, const std::string& s, int cx, int y, const Color& c)
	{
		if (f == nullptr)
			return;
		g->SetFont(f);
		g->SetColor(c);
		g->DrawString(s, cx - f->StringWidth(s) / 2, y);
	}

	struct UiRect { int x, y, w, h; bool Has(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; } };
	static UiRect ShopRect(int i) { return { 6 + i * 66, 5, 62, 52 }; }
	static UiRect BlackoutRect() { return { 6 + KA_COUNT * 66, 5, 62, 52 }; }
	static UiRect MoneyRect() { return { 474, 5, 160, 52 }; }
	static UiRect ShieldRect() { return { 150, kBottomY + 8, 84, 30 }; }
	static UiRect FrenzyRect() { return { 238, kBottomY + 8, 84, 30 }; }
	static UiRect SendRect() { return { 150, kBottomY + 42, 172, 32 }; }
	static UiRect MapRect() { return { kMapX, kMapY, kMapW, kMapH }; }

	static void Button(Graphics* g, const UiRect& r, bool theEnabled, bool theOver, const Color& theEdge)
	{
		g->SetColor(theOver && theEnabled ? Color(80, 30, 100, 235) : Color(35, 12, 50, 235));
		g->FillRect(r.x, r.y, r.w, r.h);
		g->SetColor(theEnabled ? theEdge : Color(80, 60, 90));
		g->DrawRect(r.x, r.y, r.w - 1, r.h - 1);
	}

	///////////////////////////////////////////////////////////////////////////
	// The lair screen
	///////////////////////////////////////////////////////////////////////////
	class LairTank : public Widget
	{
	public:
		int		mOverX = -1, mOverY = -1;
		bool	mMouseDown = false;
		double	mNebulaX = 0;
		std::vector<int> mStars;	// x, y, phase triples
		std::vector<int> mSpires;	// x, width, height, color triples: the lair's crystal formations
		Image*	mNebula = nullptr;

		LairTank()
		{
			mMouseVisible = true;
			mHasAlpha = true;
			mHasTransparencies = true;
			mClip = false;
			for (int i = 0; i < 70; i++)
			{
				mStars.push_back(Rand(640));
				mStars.push_back(kTankTop + Rand(kTankBottom - kTankTop));
				mStars.push_back(Rand(1000));
			}
			mNebula = gSexyAppBase->GetImage("images/nebula1");
			for (int x = -10; x < 560; x += 22 + Rand(30))
			{
				mSpires.push_back(x);
				mSpires.push_back(10 + Rand(18));
				mSpires.push_back(18 + Rand(52));
				mSpires.push_back(Rand(3));
			}
		}

		virtual ~LairTank()
		{
			delete mNebula;
		}

		virtual bool WantsFocus() override { return true; }

		virtual void Update() override
		{
			Widget::Update();
			mNebulaX += 0.15;
			MarkDirty();
		}

		virtual void MouseMove(int x, int y) override
		{
			mOverX = x;
			mOverY = y;
			if (gK.mTabView && mMouseDown)
				SetHunt(true, x, y);
		}

		virtual void MouseDrag(int x, int y) override { MouseMove(x, y); }

		virtual void MouseLeave() override
		{
			Widget::MouseLeave();
			mOverX = mOverY = -1;
			mMouseDown = false;
			if (gK.mHunting)
				SetHunt(false, gK.mCurX, gK.mCurY);
		}

		virtual void MouseDown(int x, int y, int theClickCount) override
		{
			if (theClickCount < 0)
			{
				// Right click: forget the landing spot, drop the selection.
				gK.mMarkerX = gK.mMarkerY = -1;
				for (LairAlien& a : gK.mAliens)
					a.mSelected = false;
				return;
			}
			if (gK.mTabView)
			{
				// Watching the fish tank: pick the landing spot; hold to hunt there.
				gK.mMarkerX = std::clamp(x, 40, 600);
				gK.mMarkerY = std::clamp(y, 90, 380);
				mMouseDown = true;
				SetHunt(true, x, y);
				return;
			}
			if (y < kTopH)
			{
				for (int i = 0; i < KA_COUNT; i++)
					if (ShopRect(i).Has(x, y))
						Buy(i);
				if (BlackoutRect().Has(x, y))
					UseBlackout();
				return;
			}
			if (y >= kBottomY)
			{
				if (ShieldRect().Has(x, y))
					BuyBuff(0);
				else if (FrenzyRect().Has(x, y))
					BuyBuff(1);
				else if (SendRect().Has(x, y))
					SendSelected();
				else if (MapRect().Has(x, y))
				{
					// The mini-map: a landing spot in the fish tank.
					gK.mMarkerX = std::clamp(31 + (x - kMapX) * 555 / kMapW, 40, 600);
					gK.mMarkerY = std::clamp(61 + (y - kMapY) * 338 / kMapH, 90, 380);
				}
				return;
			}
			if (CollectCrystalAt(x, y))
				return;
			if (LairAlien* a = AlienAt(x, y))
			{
				if (Grown(*a))
					a->mSelected = !a->mSelected;
				else
					Note("Too young to send: feed it goo until it grows.");
				return;
			}
			DropGoo(x, y);
		}

		virtual void MouseUp(int x, int y, int theClickCount) override
		{
			mMouseDown = false;
			if (gK.mHunting)
				SetHunt(false, x, y);
		}

		virtual void KeyDown(KeyCode theKey) override
		{
			if (theKey == KEYCODE_ESCAPE)
				TryForfeit();
			else if (theKey == KEYCODE_SPACE)
				SendSelected();
			else if (theKey == 'A')
			{
				// Select every grown alien, or clear the selection if they all are.
				bool anAll = CountLiving(true) > 0 && CountSelected() == CountLiving(true);
				for (LairAlien& a : gK.mAliens)
					a.mSelected = !anAll && a.mState == AS_LIVING && Grown(a);
			}
		}

		void DrawLair(Graphics* g)
		{
			uint32_t aNow = Now();
			// The aliens' home: a drifting nebula, twinkling stars and violet light.
			g->SetColor(Color(8, 0, 18));
			g->FillRect(0, kTankTop, 640, kTankBottom - kTankTop);
			if (mNebula != nullptr)
			{
				int w = mNebula->mWidth, off = (int)mNebulaX % w;
				g->DrawImage(mNebula, -off, kTankTop - 40);
				g->DrawImage(mNebula, w - off, kTankTop - 40);
				g->SetColor(Color(90, 20, 130, 70));		// a violet wash: the aliens' sky
				g->FillRect(0, kTankTop, 640, kTankBottom - kTankTop);
			}
			for (size_t i = 0; i + 2 < mStars.size(); i += 3)
			{
				int a = 110 + (int)(110 * std::sin((aNow + mStars[i + 2] * 7) / 400.0));
				g->SetColor(Color(220, 200, 255, std::clamp(a, 0, 255)));
				g->FillRect(mStars[i], mStars[i + 1], 2, 2);
			}
			// The ground: dark rock with glowing crystal spires.
			static const Color kSpire[3] = { Color(80, 230, 220), Color(230, 90, 255), Color(140, 255, 120) };
			for (size_t i = 0; i + 3 < mSpires.size(); i += 4)
			{
				int x = mSpires[i], w = mSpires[i + 1], h = mSpires[i + 2];
				Color c = kSpire[mSpires[i + 3]];
				int aPulse = 150 + (int)(60 * std::sin(aNow / 700.0 + i));
				Point p[3] = { Point(x, kTankBottom - 12), Point(x + w, kTankBottom - 12), Point(x + w / 2, kTankBottom - 12 - h) };
				g->SetColor(Color(c.mRed, c.mGreen, c.mBlue, aPulse));
				g->PolyFill(p, 3, true);
				Point q[3] = { Point(x + w / 2, kTankBottom - 12), Point(x + w - 3, kTankBottom - 12), Point(x + w / 2, kTankBottom - 12 - h) };
				g->SetColor(Color(255, 255, 255, aPulse / 3));
				g->PolyFill(q, 3, true);
			}
			g->SetColor(Color(25, 8, 35));
			g->FillRect(0, kTankBottom - 14, 640, 14);
			g->SetColor(Color(150, 70, 200, 120));
			g->DrawLine(0, kTankBottom - 14, 640, kTankBottom - 14);

			// The portal to the fish tank.
			int aFrame = (int)(aNow / 60) % 17;
			if (IMAGE_WARPGLOW != nullptr)
				g->DrawImageCel(IMAGE_WARPGLOW, kPortalX - 50, kPortalY - 110, aFrame);
			if (IMAGE_WARPHOLE != nullptr)
				g->DrawImageCel(IMAGE_WARPHOLE, kPortalX - 30, kPortalY - 110, aFrame);

			// Goo, crystals, aliens.
			Sexy::Font* small = FONT_JUNGLEFEVER10OUTLINE;
			for (const Goo& gg : gK.mGoo)
			{
				int a = gg.mLandedAt != 0 ? std::max(0, 255 - (int)(aNow - gg.mLandedAt) * 255 / 1500) : 255;
				int x = (int)gg.mX, y = (int)gg.mY + (int)(2 * std::sin(aNow / 150.0 + x));
				Disc(g, x, y, 9, Color(80, 255, 60, a / 4));
				Disc(g, x, y, 6, Color(110, 240, 60, a));
				Disc(g, x - 2, y - 2, 2, Color(230, 255, 220, a));
			}
			for (const Crystal& c : gK.mCrystals)
			{
				bool aBlink = c.mLandedAt != 0 && aNow - c.mLandedAt > 8000 && (aNow / 150) % 2 == 0;
				if (aBlink)
					continue;
				g->SetColorizeImages(true);
				g->SetColor(Color(255, 110, 255));
				if (IMAGE_MONEY != nullptr)
					g->DrawImageCel(IMAGE_MONEY, (int)c.mX - 36, (int)c.mY - 36, (int)(aNow / 80) % 10, 3);
				g->SetColorizeImages(false);
			}
			for (const LairAlien& a : gK.mAliens)
			{
				int aSize = Size(a);
				int aFrame2 = (int)(aNow / 90 + a.mId * 3) % 10;
				if (a.mState == AS_SENDING)
				{
					double dx = kPortalX - a.mX, dy = kPortalY - a.mY;
					aSize = std::max(12, (int)(aSize * std::min(1.0, std::sqrt(dx * dx + dy * dy) / 120.0)));
				}
				if (a.mState == AS_LIVING)
					Disc(g, (int)a.mX, (int)a.mY, aSize / 2 - 4, Color(200, 160, 255, 55));	// they glow faintly: easy to see on the dark sky
				if (a.mSelected)
					Ring(g, (int)a.mX, (int)a.mY, aSize / 2 + 4, Color(255, 230, 60, 230));
				if (a.mShield)
					Ring(g, (int)a.mX, (int)a.mY, aSize / 2 + 9, Color(90, 180, 255, 210));
				if (a.mFrenzy)
					Ring(g, (int)a.mX, (int)a.mY, aSize / 2 + 13 + (int)(2 * std::sin(aNow / 60.0)), Color(255, 70, 30, 210));
				Color aTint(255, 255, 255);
				const Color* aTintP = nullptr;
				uint32_t aHunger = aNow - a.mLastMeal;
				if (a.mState == AS_DYING)
				{
					aTint = Color(120, 120, 120, std::max(0, 255 - (int)(aNow - a.mStateAt) * 255 / 1000));
					aTintP = &aTint;
				}
				else if (a.mState == AS_LIVING && aHunger >= (uint32_t)kHungryMs)
				{
					int p = (int)(60 * std::sin(aNow / 120.0));
					aTint = Color(255, 150 + p, 150 + p);
					aTintP = &aTint;
				}
				DrawAlienSprite(g, a.mKind, a.mStage, (int)a.mX, (int)a.mY, aSize, aFrame2, a.mFacingRight, aTintP);
				if (a.mState == AS_LIVING && aHunger >= (uint32_t)kHungryMs)
					Centered(g, small, "hungry!", (int)a.mX, (int)a.mY - aSize / 2 - 2, Color(255, 120, 120));
				if (a.mState == AS_LIVING && a.mStage < 2)
				{
					// Growth pips: meals eaten toward the next stage.
					int aNeed = a.mStage == 0 ? kMealsToJuvenile : kMealsToAdult;
					int x0 = (int)a.mX - aNeed * 5, y0 = (int)a.mY + aSize / 2 + 2;
					for (int m = 0; m < aNeed; m++)
					{
						g->SetColor(Color(0, 0, 0, 160));
						g->FillRect(x0 + m * 10, y0, 8, 6);
						g->SetColor(m < a.mMeals ? Color(120, 255, 110) : Color(80, 60, 100));
						g->FillRect(x0 + m * 10 + 1, y0 + 1, 6, 4);
					}
				}
			}

			// Floating messages: crystals, growth, bounties.
			for (size_t i = 0; i < gK.mFloats.size();)
			{
				const KeeperState::FloatText& f = gK.mFloats[i];
				int anAge = (int)(aNow - f.mBorn);
				if (anAge > 1400)
				{
					gK.mFloats.erase(gK.mFloats.begin() + i);
					continue;
				}
				Color c = f.mColor;
				c.mAlpha = std::max(0, 255 - anAge * 255 / 1400);
				Centered(g, FONT_JUNGLEFEVER12OUTLINE, f.mText, (int)f.mX, (int)f.mY - anAge / 30, c);
				i++;
			}

			// Status line under the top bar.
			Sexy::Font* mid = FONT_JUNGLEFEVER12OUTLINE;
			std::string aStatus;
			int aRun = KeeperRunMs();
			if (RaceRunning() && gK.mFishPaused)
				aStatus = S().PlayerName(Them()) + " paused the game";
			else if (RaceRunning())
				aStatus = InGrace() ? "The portal opens in " + ClockText(GraceMs() - aRun + 999) : S().PlayerName(Them()) + "'s tank holds out for " + ClockText(RoundMs() - aRun + 999);
			for (int i = 0; i < KA_COUNT; i++)
				if (ShopRect(i).Has(mOverX, mOverY))
				{
					// What the hovered shop button buys.
					const KeeperAlienInfo& k = kAliens[i];
					char aBuf[160];
					if (KindAvailable(i))
						snprintf(aBuf, sizeof(aBuf), "Baby %s $%d: grows up on goo, then drops $%d crystals. Sending costs $%d.", k.mName, k.mBabyCost, k.mCrystal, PortalFee(i));
					else
						snprintf(aBuf, sizeof(aBuf), "%s: in the lair from Tank %d.", k.mName, k.mFromTank);
					aStatus = aBuf;
				}
			if (ShieldRect().Has(mOverX, mOverY))
				aStatus = "Shield $60: lasers bounce off for 5 s after landing. One power-up per alien.";
			if (FrenzyRect().Has(mOverX, mOverY))
				aStatus = "Frenzy $40: 80% faster for 8 s after landing. One power-up per alien.";
			if (BlackoutRect().Has(mOverX, mOverY))
				aStatus = "Blackout $150: " + S().PlayerName(Them()) + "'s screen goes dark but a flashlight for 6 s.";
			if (!aStatus.empty() && mid != nullptr)
			{
				int w = mid->StringWidth(aStatus);
				g->SetColor(Color(0, 0, 0, 140));
				g->FillRect(320 - w / 2 - 10, kTankTop + 4, w + 20, 20);
				Centered(g, mid, aStatus, 320, kTankTop + 19, InGrace() ? Color(140, 255, 160) : Color(255, 200, 255));
			}
		}

		void DrawBars(Graphics* g)
		{
			uint32_t aNow = Now();
			Sexy::Font* small = FONT_JUNGLEFEVER10OUTLINE;
			Sexy::Font* mid = FONT_JUNGLEFEVER12OUTLINE;
			bool aRunning = RaceRunning();

			// Top bar: the baby shop, Blackout and money.
			g->SetColor(Color(28, 6, 40));
			g->FillRect(0, 0, 640, kTopH);
			g->SetColor(Color(170, 70, 220));
			g->DrawLine(0, kTopH - 1, 640, kTopH - 1);
			for (int i = 0; i < KA_COUNT; i++)
			{
				const KeeperAlienInfo& k = kAliens[i];
				UiRect r = ShopRect(i);
				bool anAvail = KindAvailable(i);
				bool anOk = anAvail && aRunning && gK.mMoney >= k.mBabyCost && (int)gK.mAliens.size() < kMaxLairAliens;
				Button(g, r, anOk, r.Has(mOverX, mOverY), Color(190, 100, 240));
				if (!anAvail)
				{
					Centered(g, small, k.mShort, r.x + r.w / 2, r.y + 20, Color(110, 90, 120));
					Centered(g, small, "Tank " + std::to_string(k.mFromTank), r.x + r.w / 2, r.y + 38, Color(110, 90, 120));
					continue;
				}
				DrawAlienSprite(g, i, 1, r.x + r.w / 2, r.y + 20, 34, 0, false, nullptr);
				Centered(g, small, Money(k.mBabyCost), r.x + r.w / 2, r.y + 48, anOk ? Color(140, 255, 160) : Color(140, 120, 150));
			}
			UiRect b = BlackoutRect();
			bool aCooling = !Elapsed(gK.mBlackoutReadyAt);
			bool aBlackOk = aRunning && !InGrace() && !aCooling && gK.mMoney >= kBlackoutCost;
			Button(g, b, aBlackOk, b.Has(mOverX, mOverY), Color(255, 110, 110));
			if (aCooling)
			{
				g->SetColor(Color(0, 0, 0, 140));
				g->FillRect(b.x + 1, b.y + 1, (b.w - 2) * std::clamp((int)(gK.mBlackoutReadyAt - aNow), 0, kBlackoutCooldownMs) / kBlackoutCooldownMs, b.h - 2);
			}
			Centered(g, small, "Black", b.x + b.w / 2, b.y + 17, aBlackOk ? Color(255, 200, 200) : Color(150, 120, 130));
			Centered(g, small, "out", b.x + b.w / 2, b.y + 31, aBlackOk ? Color(255, 200, 200) : Color(150, 120, 130));
			Centered(g, small, Money(kBlackoutCost), b.x + b.w / 2, b.y + 48, aBlackOk ? Color(140, 255, 160) : Color(140, 120, 150));
			UiRect m = MoneyRect();
			g->SetColor(Color(15, 2, 25));
			g->FillRect(m.x, m.y, m.w, m.h);
			g->SetColor(Color(255, 110, 255));
			g->DrawRect(m.x, m.y, m.w - 1, m.h - 1);
			Centered(g, FONT_JUNGLEFEVER15OUTLINE ? FONT_JUNGLEFEVER15OUTLINE : mid, "$" + Money(gK.mMoney), m.x + m.w / 2, m.y + 26, Color(255, 180, 255));
			Centered(g, small, std::to_string(CountLiving(false)) + "/" + std::to_string(kMaxLairAliens) + " aliens in the lair", m.x + m.w / 2, m.y + 45, Color(200, 170, 230));

			// Bottom bar: selection, power-ups, send, mini-map.
			g->SetColor(Color(28, 6, 40));
			g->FillRect(0, kBottomY, 640, 480 - kBottomY);
			g->SetColor(Color(170, 70, 220));
			g->DrawLine(0, kBottomY, 640, kBottomY);
			int aSel = CountSelected();
			Centered(g, mid, aSel > 0 ? std::to_string(aSel) + " selected" : "Click grown", 74, kBottomY + 26, aSel > 0 ? Color(255, 230, 60) : Color(200, 170, 230));
			Centered(g, small, aSel > 0 ? "right click: clear" : "aliens to select", 74, kBottomY + 44, Color(170, 140, 200));
			Centered(g, small, gK.mMarkerX >= 0 ? "landing: marked" : "landing: anywhere", 74, kBottomY + 62, gK.mMarkerX >= 0 ? Color(255, 120, 120) : Color(170, 140, 200));
			UiRect s = ShieldRect(), f = FrenzyRect(), snd = SendRect();
			bool anySel = aSel > 0;
			Button(g, s, anySel, s.Has(mOverX, mOverY), Color(90, 180, 255));
			Centered(g, small, "Shield " + Money(kShieldCost), s.x + s.w / 2, s.y + 20, anySel ? Color(170, 220, 255) : Color(120, 110, 140));
			Button(g, f, anySel, f.Has(mOverX, mOverY), Color(255, 90, 60));
			Centered(g, small, "Frenzy " + Money(kFrenzyCost), f.x + f.w / 2, f.y + 20, anySel ? Color(255, 190, 170) : Color(120, 110, 140));
			bool aSendOk = anySel && aRunning && !InGrace() && gK.mMoney >= SendFee();
			Button(g, snd, aSendOk, snd.Has(mOverX, mOverY), Color(255, 230, 60));
			Centered(g, mid, anySel ? "SEND ($" + Money(SendFee()) + ")" : "SEND", snd.x + snd.w / 2, snd.y + 22, aSendOk ? Color(255, 240, 120) : Color(140, 120, 150));

			// What the fish keeper has, and a map of their tank (click to pick the landing spot).
			Centered(g, small, S().PlayerName(Them()).substr(0, 10) + ": " + std::to_string(RaceRivalFish()) + " fish", 405, kBottomY + 20, Color(200, 230, 255));
			Centered(g, small, "$" + Money(RaceRivalMoney()) + "  egg " + std::to_string(RaceRivalEggs()) + "/3", 405, kBottomY + 36, Color(200, 230, 255));
			Centered(g, small, "Yours there: " + std::to_string(RaceRivalAliens()), 405, kBottomY + 52, Color(255, 150, 150));
			Centered(g, small, "Hold Tab: live", 405, kBottomY + 68, Color(170, 140, 200));
			RaceDrawRivalDots(g, kMapX, kMapY, kMapW, kMapH);
			if (gK.mMarkerX >= 0)
			{
				int mx = kMapX + (gK.mMarkerX - 31) * kMapW / 555, my = kMapY + (gK.mMarkerY - 61) * kMapH / 338;
				Reticle(g, mx, my, 5, Color(255, 60, 60));
			}
		}

		void DrawTabView(Graphics* g)
		{
			// Only an overlay: the live picture of the fish tank is underneath.
			uint32_t aNow = Now();
			Sexy::Font* mid = FONT_JUNGLEFEVER12OUTLINE;
			std::string aMsg = "LIVE: " + S().PlayerName(Them()) + "'s tank. Click: landing spot. Hold: your aliens hunt there.";
			if (mid != nullptr)
			{
				int w = mid->StringWidth(aMsg);
				g->SetColor(Color(40, 0, 50, 200));
				g->FillRect(320 - w / 2 - 10, 426, w + 20, 22);
				Centered(g, mid, aMsg, 320, 442, Color(255, 180, 255));
			}
			if (gK.mMarkerX >= 0)
				Reticle(g, gK.mMarkerX, gK.mMarkerY, 18 + (int)(3 * std::sin(aNow / 110.0)), Color(255, 60, 200, 220));
			if (gK.mHunting)
				Reticle(g, gK.mCurX, gK.mCurY, 24, Color(255, 60, 60, 230));
		}

		virtual void Draw(Graphics* g) override
		{
			if (gK.mTabView)
			{
				DrawTabView(g);
				return;
			}
			DrawLair(g);
			DrawBars(g);
			if (!gK.mNote.empty() && !Elapsed(gK.mNoteAt + 2600))
			{
				Sexy::Font* small = FONT_JUNGLEFEVER12OUTLINE;
				if (small != nullptr)
				{
					int w = small->StringWidth(gK.mNote);
					g->SetColor(Color(0, 0, 0, 170));
					g->FillRect(320 - w / 2 - 10, kBottomY - 26, w + 20, 22);
					Centered(g, small, gK.mNote, 320, kBottomY - 10, Color(255, 220, 255));
				}
			}
		}
	};

	static void RemoveLairTank()
	{
		if (gK.mWidget == nullptr)
			return;
		if (App()->mWidgetManager != nullptr)
			App()->mWidgetManager->RemoveWidget(gK.mWidget);
		App()->SafeDeleteWidget(gK.mWidget);
		gK.mWidget = nullptr;
	}

	///////////////////////////////////////////////////////////////////////////
	// The tank (fish keeper)
	///////////////////////////////////////////////////////////////////////////
	static void LaunchLands(const PendingLaunch& l)
	{
		gK.mLastPortalAt = Now();
		Board* b = FishTank();
		ByteWriter w;
		w.U8(KE_LANDED);
		w.I32(l.mKind);
		Send(MSG_KEEPER_EVENT, w);
		if (b == nullptr)
			return;
		int x = std::clamp(l.mX - 80, 20, 470);
		int y = std::clamp(l.mY - 80, 85, 290);
		size_t aBefore = b->mAlienList->size();
		b->SpawnAlien(kAliens[l.mKind].mGameType, x, y, true);
		if (b->mAlienList->size() > aBefore)
		{
			Alien* anAlien = b->mAlienList->back();
			if (l.mFlags & LF_JUVENILE)
			{
				anAlien->mHealth *= kJuvenileHealth;
				anAlien->mMaxHealth *= kJuvenileHealth;
			}
			AlienBuff& aBuff = gK.mBuffs[anAlien];
			aBuff.mLandedAt = Now();
			aBuff.mShieldUntil = Now() + ((l.mFlags & LF_SHIELD) ? kShieldMs : kArrivalShieldMs);
			if (l.mFlags & LF_FRENZY)
				aBuff.mFrenzyUntil = Now() + kFrenzyMs;
		}
		S().Log("Keeper: %s landed at %d,%d (flags %d)", kAliens[l.mKind].mName, l.mX, l.mY, l.mFlags);
	}

	static void ReceiveLaunch(int theKind, int theX, int theY, int theFlags)
	{
		Board* b = FishTank();
		if (b == nullptr || theKind < 0 || theKind >= KA_COUNT || !RaceRunning())
			return;
		if (theX < 0)
		{
			theX = 60 + Rand(520);
			theY = 110 + Rand(240);
		}
		// The portal lets one alien through every few seconds, so a wave lands as a stream
		// the laser can answer one by one.
		uint32_t anAt = Now() + kWarpMs;
		uint32_t aPortalFree = gK.mLastPortalAt + kPortalGapMs;
		for (const PendingLaunch& l : gK.mLaunches)
			aPortalFree = std::max(aPortalFree, l.mAt + kPortalGapMs);
		if (gK.mLastPortalAt != 0 || !gK.mLaunches.empty())
			anAt = std::max(anAt, aPortalFree);
		gK.mLaunches.push_back({ theKind, theX, theY, theFlags, anAt });
		if (gK.mWaveCount == 0)
			b->PlaySample(SOUND_AWOOGA_ID, 3, 1.0);
		// A wave arrives as several launches: one message for all of them.
		gK.mWaveCount++;
		gK.mWaveKind = theKind;
		gK.mWaveToastAt = Now() + 2000;	// the lair sends a wave one alien at a time
	}

	static void ReceivePower(int thePower)
	{
		Board* b = FishTank();
		if (b == nullptr || thePower != 0)
			return;
		if (Elapsed(gK.mBlackoutUntil))
			gK.mBlackoutStart = Now();
		gK.mBlackoutUntil = Now() + kBlackoutMs;
		b->PlaySample(SOUND_SONAR_ID, 3, 1.0);
		S().AddToast(S().PlayerName(Them()) + " used Blackout!", Them(), 250);
	}

	static int Bounty(GameObject* theFish)
	{
		switch (theFish->mType)
		{
		case TYPE_GUPPY:
		{
			int aSize = ((Fish*)theFish)->mSize;
			return aSize == SIZE_SMALL ? 15 : (aSize == SIZE_MEDIUM ? 20 : (aSize == SIZE_LARGE ? 30 : 45));
		}
		case TYPE_BREEDER:	return 30;
		case TYPE_OSCAR:	return 80;
		case TYPE_ULTRA:	return 200;
		default:			return 40;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Lifecycle
	///////////////////////////////////////////////////////////////////////////
	void KeeperBegin(bool theFishKeeper)
	{
		RemoveLairTank();
		gK = KeeperState();
		gK.mActive = true;
		gK.mFish = theFishKeeper;
		gK.mMoney = kStartMoney;
		gK.mLastTick = Now();
		if (!theFishKeeper)
		{
			gK.mWidget = new LairTank();
			gK.mWidget->Resize(0, 0, 640, 480);
			App()->mWidgetManager->AddWidget(gK.mWidget);
			App()->mWidgetManager->SetFocus(gK.mWidget);
			App()->mMusicInterface->StopAllMusic();
			App()->PlayMusic(1, 0);		// the aliens' own theme
		}
		S().Log("Keeper: round begins, I %s", theFishKeeper ? "keep the fish" : "run the lair");
	}

	void KeeperEnd()
	{
		if (!gK.mFish && gK.mActive)
		{
			App()->mMusicInterface->StopAllMusic();
			if (S().GetRole() == ROLE_HOST)
				App()->PlayMusic(2, 0);		// back to the menu's music
		}
		RemoveLairTank();
		S().SetKeeperViewShown(false);
		gK.mActive = false;
		gK.mLaunches.clear();
		gK.mBuffs.clear();
	}

	void KeeperUpdate()
	{
		if (!gK.mActive)
			return;
		uint32_t aNow = Now();
		if (!gK.mFish)
		{
			double aDt = std::clamp((int)(aNow - gK.mLastTick), 0, 100) / 1000.0;
			gK.mLastTick = aNow;
			if (RaceRunning())
				UpdateLair(aDt);

			// Tab: the live view of the fish tank.
			bool aTab = App()->mWidgetManager->mKeyDown[KEYCODE_TAB] && RaceRunning();
			if (aTab != gK.mTabView)
			{
				gK.mTabView = aTab;
				S().SetKeeperViewShown(aTab);
				if (!aTab && gK.mHunting)
					SetHunt(false, gK.mCurX, gK.mCurY);
			}
			if (gK.mWidget != nullptr && App()->mDialogList.empty())
			{
				App()->mWidgetManager->BringToFront(gK.mWidget);
				if (App()->mWidgetManager->mFocusWidget != gK.mWidget)
					App()->mWidgetManager->SetFocus(gK.mWidget);
			}
			SendCursor(false);
			if (Elapsed(gK.mLastLairSent + 250))
			{
				ByteWriter w;
				w.I32(gK.mMoney);
				w.U8((uint8_t)CountLiving(false));
				w.U8((uint8_t)CountLiving(true));
				Send(MSG_KEEPER_LAIR, w);
				gK.mLastLairSent = aNow;
			}
			// Out of aliens everywhere and too poor for another baby: the lair loses.
			if (RaceRunning() && Elapsed(gK.mLastDefeatCheck + 500))
			{
				gK.mLastDefeatCheck = aNow;
				int aCrystals = 0;
				for (const Crystal& c : gK.mCrystals)
					aCrystals += c.mValue;
				// Three checks in a row (1.5 s): an alien that just landed shows up in the
				// fish keeper's next snapshot, not instantly.
				bool aBroke = gK.mAliens.empty() && gK.mInFlight == 0 && RaceRivalAliens() == 0 && gK.mMoney + aCrystals < CheapestBaby();
				gK.mBrokeChecks = aBroke ? gK.mBrokeChecks + 1 : 0;
				if (gK.mBrokeChecks >= 3)
				{
					S().Log("Keeper: the lair is out of aliens and money");
					RaceKeeperLairDefeated();
				}
			}
			return;
		}

		Board* b = RaceTankBoard();
		if (b == nullptr)
			return;
		b->mAlienTimer = 3000;		// no natural invasions: every alien is the keeper's
		uint32_t aStep = gK.mLastFishTick == 0 ? 0 : std::min<uint32_t>(aNow - gK.mLastFishTick, 500);
		gK.mLastFishTick = aNow;
		if (!RaceRunning())
			return;
		bool aPaused = b->mPause || !App()->mDialogList.empty();
		if (aPaused)
		{
			// A paused tank stops the round clock and holds back aliens on their way.
			gK.mPausedMs += aStep;
			for (PendingLaunch& l : gK.mLaunches)
				l.mAt += aStep;
		}
		if (Elapsed(gK.mLastClockSent + 500))
		{
			ByteWriter w;
			w.U8(KE_CLOCK);
			w.I32(aPaused ? -(KeeperRunMs() + 1) : KeeperRunMs());
			Send(MSG_KEEPER_EVENT, w);
			gK.mLastClockSent = aNow;
		}
		if (gK.mWaveCount > 0 && Elapsed(gK.mWaveToastAt))
		{
			S().AddToast(S().PlayerName(Them()) + (gK.mWaveCount == 1 ? " is sending " + std::string(kAliens[gK.mWaveKind].mName) + "!"
				: " is sending a wave of " + std::to_string(gK.mWaveCount) + " aliens!"), Them(), 250);
			gK.mWaveCount = 0;
		}
		if (aPaused)
			return;
		for (size_t i = 0; i < gK.mLaunches.size();)
		{
			if (Elapsed(gK.mLaunches[i].mAt))
			{
				PendingLaunch l = gK.mLaunches[i];
				gK.mLaunches.erase(gK.mLaunches.begin() + i);
				LaunchLands(l);
			}
			else
				i++;
		}
		if (KeeperRunMs() >= RoundMs())
			RaceKeeperSurvived();
	}

	void KeeperHandleMessage(uint8_t theType, ByteReader& r)
	{
		if (!gK.mActive)
			return;
		switch (theType)
		{
		case MSG_KEEPER_LAUNCH:
		{
			int aKind = r.U8();
			int x = r.I16(), y = r.I16();
			int aFlags = r.U8();
			if (!r.mError && gK.mFish)
				ReceiveLaunch(aKind, x, y, aFlags);
			break;
		}
		case MSG_KEEPER_POWER:
		{
			int aPower = r.U8();
			if (!r.mError && gK.mFish)
				ReceivePower(aPower);
			break;
		}
		case MSG_KEEPER_CURSOR:
		{
			int x = r.I16(), y = r.I16();
			bool aHunting = r.U8() != 0;
			if (!r.mError && gK.mFish)
			{
				gK.mRivalX = x;
				gK.mRivalY = y;
				gK.mRivalHunting = aHunting;
			}
			break;
		}
		case MSG_KEEPER_LAIR:
		{
			int aMoney = r.I32();
			int anAliens = r.U8(), aGrown = r.U8();
			if (!r.mError && gK.mFish)
			{
				gK.mLairKnown = true;
				gK.mLairMoney = aMoney;
				gK.mLairAliens = anAliens;
				gK.mLairGrown = aGrown;
			}
			break;
		}
		case MSG_KEEPER_EVENT:
		{
			int anEvent = r.U8();
			int aValue = r.I32();
			if (r.mError || gK.mFish)
				break;
			if (anEvent == KE_BOUNTY)
			{
				gK.mMoney += aValue;
				gK.mEaten++;
				Float(kPortalX - 30, kPortalY - 60, "Bounty +$" + std::to_string(aValue), Color(255, 220, 90));
				Note("Your alien ate a fish! +$" + std::to_string(aValue));
				PlayLocal(SOUND_CHOMP);
			}
			else if (anEvent == KE_ALIEN_DOWN)
				S().AddToast(S().PlayerName(Them()) + " shot down one of your aliens", Them(), 250);
			else if (anEvent == KE_LANDED)
				gK.mInFlight = std::max(0, gK.mInFlight - 1);
			else if (anEvent == KE_CLOCK)
			{
				gK.mFishPaused = aValue < 0;
				gK.mFishRunMs = aValue < 0 ? -aValue - 1 : aValue;
				gK.mFishRunAt = Now();
			}
			break;
		}
		default:
			break;
		}
	}

	std::string KeeperResultLine()
	{
		char aBuf[96];
		if (gK.mFish)
			snprintf(aBuf, sizeof(aBuf), "You shot down %d alien%s.", gK.mShotDown, gK.mShotDown == 1 ? "" : "s");
		else
			snprintf(aBuf, sizeof(aBuf), "You sent %d alien%s; they ate %d fish.", gK.mSent, gK.mSent == 1 ? "" : "s", gK.mEaten);
		return aBuf;
	}

	///////////////////////////////////////////////////////////////////////////
	// Drawing on the fish keeper's screen (the shared part is streamed to the lair)
	///////////////////////////////////////////////////////////////////////////
	void KeeperDrawShared(Graphics* g)
	{
		if (!gK.mActive || !gK.mFish)
			return;
		Board* b = RaceTankBoard();
		if (b == nullptr)
			return;
		Session& s = S();
		Sexy::Font* small = FONT_JUNGLEFEVER10OUTLINE;
		Sexy::Font* mid = FONT_JUNGLEFEVER12OUTLINE;
		uint32_t aNow = Now();

		if (RaceRunning())
		{
			int aRun = KeeperRunMs();
			std::string aClock = aRun < GraceMs()
				? "Aliens can come through in " + ClockText(GraceMs() - aRun + 999)
				: "Hold out: " + ClockText(RoundMs() - aRun + 999);
			if (mid != nullptr)
			{
				int w = mid->StringWidth(aClock);
				g->SetColor(Color(0, 0, 0, 150));
				g->FillRect(320 - w / 2 - 10, 64, w + 20, 20);
				Centered(g, mid, aClock, 320, 79, aRun < GraceMs() ? Color(140, 255, 160) : Color(255, 220, 120));
			}

			// Aliens on their way: a crosshair where each will land.
			for (const PendingLaunch& l : gK.mLaunches)
			{
				int aLeft = std::max(0, (int)(l.mAt - aNow));
				int r = 20 + std::min(aLeft, kWarpMs) * 30 / kWarpMs;
				int a = 150 + (int)(100 * std::sin(aNow / 90.0));
				Reticle(g, l.mX, l.mY, r, Color(255, 40, 40, a));
				if (Sexy::IMAGE_CROSSHAIR != nullptr)
				{
					Image* c = Sexy::IMAGE_CROSSHAIR;
					int aCels = std::max(1, c->mNumCols), cw = c->mWidth / aCels;
					g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);	// as the game draws it
					g->DrawImageCel(c, l.mX - cw / 2, l.mY - c->mHeight / 2, (int)(aNow / 80) % aCels);
					g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				}
				Centered(g, small, std::string(kAliens[l.mKind].mName) + " INCOMING!", l.mX, l.mY - r - 8, Color(255, 90, 90, a));
			}

			// The keeper's hunt reticle: where the pack is heading.
			if (gK.mRivalHunting)
				Reticle(g, gK.mRivalX, gK.mRivalY, 22 + (int)(3 * std::sin(aNow / 100.0)), Color(255, 50, 50, 210));

			// Shield and frenzy on the aliens that have them.
			for (Alien* anAlien : *b->mAlienList)
			{
				auto it = gK.mBuffs.find(anAlien);
				if (anAlien == nullptr || it == gK.mBuffs.end())
					continue;
				int cx = anAlien->mX + anAlien->mWidth / 2, cy = anAlien->mY + anAlien->mHeight / 2;
				if (!Elapsed(it->second.mShieldUntil))
					Ring(g, cx, cy, 58, Color(90, 180, 255, 200));
				if (!Elapsed(it->second.mFrenzyUntil))
					Ring(g, cx, cy, 50 + (int)(4 * std::sin(aNow / 60.0)), Color(255, 60, 20, 200));
			}
		}

		// The lair's strip along the bottom.
		std::string aLair = s.PlayerName(Them()) + "'s lair: ";
		if (gK.mLairKnown)
			aLair += std::to_string(gK.mLairAliens) + " alien" + (gK.mLairAliens == 1 ? "" : "s") + " (" + std::to_string(gK.mLairGrown) + " grown)";
		else
			aLair += "...";
		if (small != nullptr)
		{
			int w = small->StringWidth(aLair);
			g->SetColor(Color(12, 4, 20, 190));
			g->FillRect(636 - w - 12, 456, w + 12, 20);
			g->SetColor(s.PlayerColor(Them()));
			g->DrawRect(636 - w - 12, 456, w + 11, 19);
			g->SetFont(small);
			g->SetColor(Color(220, 190, 255));
			g->DrawString(aLair, 636 - w - 6, 470);
		}
	}

	void KeeperDrawLocal(Graphics* g)
	{
		if (!gK.mActive || !gK.mFish || Elapsed(gK.mBlackoutUntil) || RaceTankBoard() == nullptr)
			return;
		// Blackout: darkness except a flashlight around the fish keeper's mouse. The
		// keeper doesn't get this part of the picture, so they still see everything.
		uint32_t aNow = Now();
		int aIn = (int)(aNow - gK.mBlackoutStart), aOut = (int)(gK.mBlackoutUntil - aNow);
		int a = std::clamp(std::min(aIn, aOut) * 245 / 400, 0, 245);
		WidgetManager* wm = App()->mWidgetManager;
		int mx = wm->mMouseIn ? wm->mLastMouseX : 320, my = wm->mMouseIn ? wm->mLastMouseY : 240;
		const int R = 70, top = 60, bottom = 480;
		g->SetColor(Color(0, 0, 8, a));
		for (int y = top; y < bottom; y += 2)
		{
			int dy = y - my;
			if (std::abs(dy) >= R)
			{
				g->FillRect(0, y, 640, 2);
				continue;
			}
			int half = (int)std::sqrt((double)(R * R - dy * dy));
			g->FillRect(0, y, std::max(0, mx - half), 2);
			g->FillRect(mx + half, y, std::max(0, 640 - mx - half), 2);
		}
		Centered(g, FONT_JUNGLEFEVER12OUTLINE, "BLACKOUT! " + std::to_string(aOut / 1000 + 1), 320, 100, Color(180, 180, 255, std::max(60, a)));
	}

	///////////////////////////////////////////////////////////////////////////
	// Game hooks
	///////////////////////////////////////////////////////////////////////////
	void KeeperHuntPoint(int& theX, int& theY)
	{
		if (!gK.mRivalHunting || FishTank() == nullptr || !RaceRunning())
			return;
		theX = gK.mRivalX;
		theY = gK.mRivalY;
	}

	double KeeperAlienSpeed(GameObject* theAlien)
	{
		if (FishTank() == nullptr)
			return 1.0;
		auto it = gK.mBuffs.find(theAlien);
		return it != gK.mBuffs.end() && !Elapsed(it->second.mFrenzyUntil) ? kFrenzySpeed : 1.0;
	}

	bool KeeperAlienShielded(GameObject* theAlien)
	{
		if (FishTank() == nullptr)
			return false;
		auto it = gK.mBuffs.find(theAlien);
		return it != gK.mBuffs.end() && !Elapsed(it->second.mShieldUntil);
	}

	void KeeperAlienAte(GameObject* theFish)
	{
		if (FishTank() == nullptr || !RaceRunning() || theFish == nullptr)
			return;
		ByteWriter w;
		w.U8(KE_BOUNTY);
		w.I32(Bounty(theFish));
		Send(MSG_KEEPER_EVENT, w);
	}

	void KeeperAlienRemoved(GameObject* theAlien)
	{
		auto it = gK.mBuffs.find(theAlien);
		if (it != gK.mBuffs.end())
		{
			S().Log("Keeper: alien down %u ms after landing", Now() - it->second.mLandedAt);
			gK.mBuffs.erase(it);
		}
		if (FishTank() == nullptr || !RaceRunning())
			return;
		gK.mShotDown++;
		ByteWriter w;
		w.U8(KE_ALIEN_DOWN);
		w.I32(0);
		Send(MSG_KEEPER_EVENT, w);
	}

	///////////////////////////////////////////////////////////////////////////
	// Test harness
	///////////////////////////////////////////////////////////////////////////
	bool KeeperTestBuy(int theKind) { return Buy(theKind); }

	void KeeperTestFeed()
	{
		for (const LairAlien& a : gK.mAliens)
			if (a.mState == AS_LIVING)
				DropGoo((int)a.mX, (int)a.mY - 40);
	}

	void KeeperTestGrow()
	{
		for (LairAlien& a : gK.mAliens)
			if (a.mState == AS_LIVING)
			{
				a.mStage = 2;
				a.mMeals = 0;
				a.mLastMeal = Now();
				a.mNextCrystal = Now() + 1000;
			}
	}

	void KeeperTestCollect()
	{
		for (const Crystal& c : gK.mCrystals)
			gK.mMoney += c.mValue;
		gK.mCrystals.clear();
	}

	void KeeperTestSelectGrown(int theCount)
	{
		int n = 0;
		for (LairAlien& a : gK.mAliens)
		{
			a.mSelected = a.mState == AS_LIVING && Grown(a) && (theCount <= 0 || n < theCount);
			if (a.mSelected)
				n++;
		}
	}

	bool KeeperTestSend(int theCount, int theX, int theY)
	{
		KeeperTestSelectGrown(theCount);
		gK.mMarkerX = theX;
		gK.mMarkerY = theY;
		return SendSelected();
	}

	bool KeeperTestBuff(int theBuff) { return BuyBuff(theBuff); }
	bool KeeperTestBlackout() { return UseBlackout(); }
	void KeeperTestHunt(bool theOn, int theX, int theY) { SetHunt(theOn, theX, theY); SendCursor(true); }
	void KeeperTestSetMoney(int theMoney) { gK.mMoney = theMoney; }

	// An average lair player: picks up crystals, feeds hungry aliens, buys the best baby it
	// can afford while keeping a small food reserve, and sends waves of theWave grown aliens
	// (shielding the strongest when rich, blacking out the fish keeper for big waves).
	void KeeperTestBot(int theWave)
	{
		if (gK.mFish || !RaceRunning())
			return;
		KeeperTestCollect();
		uint32_t aNow = Now();
		for (const LairAlien& a : gK.mAliens)
			if (a.mState == AS_LIVING && aNow - a.mLastMeal >= (uint32_t)kPeckishMs)
			{
				bool aFood = false;
				for (const Goo& gg : gK.mGoo)
					if (std::fabs(gg.mX - a.mX) < 90)
						aFood = true;
				if (!aFood)
					DropGoo((int)a.mX, (int)a.mY - 30);
			}
		if (CountLiving(false) < 6)
		{
			int aBest = -1;
			for (int i = 0; i < KA_COUNT; i++)
				if (KindAvailable(i) && gK.mMoney >= kAliens[i].mBabyCost + 35)	// keep 7 goo's worth
					aBest = i;
			if (aBest >= 0)
				Buy(aBest);
		}
		int anAdults = 0;
		for (const LairAlien& a : gK.mAliens)
			if (a.mState == AS_LIVING && a.mStage == 2)
				anAdults++;
		// Two adults stay home to earn crystals; the rest go through in waves.
		if (!InGrace() && anAdults >= std::max(1, theWave) + 2)
		{
			int n = 0, aStrongest = -1;
			for (size_t i = 0; i < gK.mAliens.size(); i++)
			{
				LairAlien& a = gK.mAliens[i];
				a.mSelected = a.mState == AS_LIVING && a.mStage == 2 && n < theWave;
				if (a.mSelected)
				{
					n++;
					if (aStrongest < 0 || kAliens[a.mKind].mBabyCost > kAliens[gK.mAliens[aStrongest].mKind].mBabyCost)
						aStrongest = (int)i;
				}
			}
			if (aStrongest >= 0 && gK.mMoney >= SendFee() + kShieldCost + 100)
				gK.mAliens[aStrongest].mShield = (gK.mMoney -= kShieldCost, true);
			if (n >= 3 && gK.mMoney >= SendFee() + kBlackoutCost + 100)
				UseBlackout();
			SendSelected();
		}
	}

	std::string KeeperDebugState()
	{
		char aBuf[400];
		int aStages[3] = { 0, 0, 0 };
		for (const LairAlien& a : gK.mAliens)
			if (a.mState == AS_LIVING)
				aStages[a.mStage]++;
		Board* b = RaceTankBoard();
		snprintf(aBuf, sizeof(aBuf), "keeper active=%d fish=%d money=%d lair=%d(baby %d juv %d adult %d) goo=%d crystals=%d selected=%d sent=%d eaten=%d inflight=%d rivalAliens=%d rivalFish=%d | tank money=%d aliens=%d fish=%d breeders=%d pending=%d buffs=%d shotdown=%d lairseen=%d/%d/%d hunting=%d blackout=%d run=%u",
			gK.mActive, gK.mFish, gK.mMoney, (int)gK.mAliens.size(), aStages[0], aStages[1], aStages[2], (int)gK.mGoo.size(), (int)gK.mCrystals.size(), CountSelected(),
			gK.mSent, gK.mEaten, gK.mInFlight, RaceRivalAliens(), RaceRivalFish(),
			b ? b->mMoney : -1, b ? (int)b->mAlienList->size() : -1, b ? (int)b->mFishList->size() : -1, b ? (int)b->mBreederList->size() : -1, (int)gK.mLaunches.size(), (int)gK.mBuffs.size(), gK.mShotDown,
			gK.mLairMoney, gK.mLairAliens, gK.mLairGrown, gK.mRivalHunting, !Elapsed(gK.mBlackoutUntil), RaceRunningMs());
		return aBuf;
	}
}
