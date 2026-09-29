#include <SexyAppFramework/Font.h>
#include "CoopRace.h"
#include "CoopSession.h"
#include "CoopUI.h"
#include "CoopKeeper.h"

#include "WinFishApp.h"
#include "Board.h"
#include "ProfileMgr.h"
#include "Alien.h"
#include "Coin.h"
#include "Fish.h"
#include "Oscar.h"
#include "Ultra.h"
#include "Gekko.h"
#include "Penta.h"
#include "Grubber.h"
#include "Breeder.h"
#include "Food.h"
#include "OtherTypePet.h"
#include "FishTypePet.h"
#include "Bilaterus.h"
#include "MenuButtonWidget.h"
#include "MessageWidget.h"
#include "Res.h"

#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/Widget.h>
#include <SexyAppFramework/WidgetManager.h>
#include <SexyAppFramework/KeyCodes.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace Sexy;

namespace Coop
{
	///////////////////////////////////////////////////////////////////////////
	// Tuning
	///////////////////////////////////////////////////////////////////////////
	struct AttackInfo
	{
		const char*	mName;		// button label
		const char*	mVerb;		// "sent you ..."
		float		mPriceOfEgg;	// price as a share of the level's egg piece
		int			mCooldownMs;
	};
	static const AttackInfo kAttacks[ATK_COUNT] =
	{
		{ "Alien",   "an ALIEN",          0.50f, 25000 },
		{ "Hunger",  "a HUNGER SPELL",    0.30f, 20000 },
		{ "Murk",    "MURKY WATER",       0.20f, 15000 },
		{ "Thief",   "a COIN THIEF",      0.30f, 20000 },
		{ "Hike",    "a PRICE HIKE",      0.25f, 20000 },
		{ "Poison",  "a POISON RAID",     0.40f, 30000 },
		{ "Block",   "a COIN BLOCKER",    0.35f, 25000 },
	};
	static const int	kGlobalCooldownMs = 2000;
	static const int	kCountdownMs = 3000;
	static const int	kResultMs = 7000;
	static const int	kSetupTimeoutMs = 120000;
	static const int	kMurkMs = 10000;
	static const int	kThiefLifeMs = 15000;
	static const int	kBlockMs = 10000;
	// Poison raid: potions fall one after another across the tank, too many to click away.
	static const int	kRaidPotions = 16, kRaidEveryMs = 110, kRaidLeftX = 40, kRaidRightX = 530;
	static const double	kThiefSpeed = 65.0, kThiefLeaveSpeed = 90.0;	// pixels per second
	static const int	kHikeMs = 20000;
	static const float	kCatchUpDiscount = 0.6f;
	static const int	kMaxRivalAliens = 2;
	static const int	kAlienWarmupMs = 45000;	// no aliens in the first 45 s: let tanks get going

	// Tank area in game coordinates.
	static const int	kTankX = 31, kTankY = 61, kTankW = 555, kTankH = 338;	// where fish swim
	static const int	kBarY = 446;	// top of the attack bar
	static const int	kBarX = 4;		// race tanks hide the game's "Tank 1-1" label to make room
	static const int	kStripX = kBarX + ATK_COUNT * 76;	// rival info, right of the attack bar (buttons 72 + gap 4)

	enum RacePhase { PH_IDLE, PH_SETUP, PH_READY, PH_COUNTDOWN, PH_RUNNING, PH_FINISHED, PH_RESULT };
	enum FinishReason { FIN_EGG = 1, FIN_LOST, FIN_FORFEIT, FIN_SURVIVED };
	enum RaceEvent { EV_DEFENDED = 1, EV_THIEF_SQUASHED };
	enum RivalFlags { RF_MURK = 1, RF_HIKE = 2, RF_THIEF = 4, RF_PAUSED = 8, RF_SETUP = 16, RF_BLOCK = 32 };
	enum DotKind { DOT_GUPPY, DOT_BREEDER, DOT_CARNIVORE, DOT_ULTRA, DOT_OTHER, DOT_PET, DOT_ALIEN };

	class RaceBar;
	class ThiefWidget;

	struct RivalInfo
	{
		bool	mKnown = false;
		int		mMoney = 0;
		int		mEggs = 0;
		int		mFish = 0;
		int		mAliens = 0;
		int		mFlags = RF_SETUP;
		struct Dot { uint8_t x, y, kind; };
		std::vector<Dot> mDots;
	};

	struct SentAlien
	{
		GameObject*	mAlien;
		int			mPrice;
	};

	struct RaceData
	{
		bool		mSettingsLoaded = false;
		int			mSetTank = 1, mSetLevel = 5;	// Versus plays a tank's final level (tests may pick another)
		bool		mSetCatchUp = true;
		bool		mSetKeeper = false;				// Versus mode: Tank Race or Alien Keeper

		RacePhase	mPhase = PH_IDLE;
		uint32_t	mRaceId = 0;
		int			mTank = 1, mLevel = 1;
		bool		mCatchUp = true;
		bool		mPendingBoard = false;
		Board*		mBoard = nullptr;			// the race tank (kept until it is destroyed)
		bool		mLocalReady = false, mPeerReady = false;
		uint32_t	mPhaseSince = 0;
		uint32_t	mCountdownAt = 0;
		uint32_t	mStartMs = 0;
		uint32_t	mLastStateSent = 0;
		int			mEggPrice = 150;

		uint32_t	mCooldownUntil[ATK_COUNT] = {};
		uint32_t	mGlobalCdUntil = 0;
		uint32_t	mMurkUntil = 0, mMurkStart = 0;
		uint32_t	mHikeUntil = 0;
		uint32_t	mBlockUntil = 0;
		std::vector<SentAlien> mSentAliens;
		RaceBar*	mBar = nullptr;
		ThiefWidget* mThief = nullptr;
		int			mThiefPrice = 0;
		int			mRaidLeft = 0, mRaidIndex = 0;	// potions still to drop
		bool		mRaidFromRight = false;
		uint32_t	mRaidNextAt = 0;

		RivalInfo	mRival;

		int			mWinner = -1, mReason = 0, mFinisher = 0;
		uint32_t	mRaceMs = 0, mResultAt = 0;
		int			mWins[2] = { 0, 0 };
		bool		mKeeper = false;			// Alien Keeper round
		int			mFishKeeper = 0;			// who keeps the fish this round
		int			mKeeperRounds = 0;			// host: rounds played, to swap roles
		int			mAttacksSent = 0, mDefended = 0;
	};
	static RaceData gRace;

	static WinFishApp* App() { return (WinFishApp*)gSexyAppBase; }
	static uint32_t Now() { return NetMillis(); }
	static int Me() { return S().LocalPlayer(); }
	static int Them() { return 1 - S().LocalPlayer(); }
	static bool Elapsed(uint32_t theDeadline) { return (int32_t)(Now() - theDeadline) >= 0; }

	static Board* RaceBoard()
	{
		WinFishApp* anApp = App();
		if (gRace.mBoard != nullptr && anApp != nullptr && anApp->mBoard == gRace.mBoard)
			return gRace.mBoard;
		return nullptr;
	}

	///////////////////////////////////////////////////////////////////////////
	// Settings
	///////////////////////////////////////////////////////////////////////////
	static void LoadSettings()
	{
		if (gRace.mSettingsLoaded || App() == nullptr)
			return;
		gRace.mSettingsLoaded = true;
		int v = 0;
		if (App()->RegistryReadInteger("RaceTank", &v) && v >= 1 && v <= 4)
			gRace.mSetTank = v;
		if (App()->RegistryReadInteger("RaceCatchUp", &v))
			gRace.mSetCatchUp = v != 0;
		if (App()->RegistryReadInteger("VersusKeeper", &v))
			gRace.mSetKeeper = v != 0;
	}

	int RaceTank() { LoadSettings(); return gRace.mSetTank; }
	int RaceLevel() { LoadSettings(); return gRace.mSetLevel; }
	bool RaceCatchUp() { LoadSettings(); return gRace.mSetCatchUp; }
	int RaceWins(int thePlayer) { return gRace.mWins[thePlayer & 1]; }

	void SetRaceTank(int theTank)
	{
		LoadSettings();
		gRace.mSetTank = std::clamp(theTank, 1, 4);
		App()->RegistryWriteInteger("RaceTank", gRace.mSetTank);
	}

	void SetRaceLevel(int theLevel)
	{
		LoadSettings();
		gRace.mSetLevel = std::clamp(theLevel, 1, 5);	// not saved: the menu always plays level 5
	}

	bool VersusIsKeeper() { LoadSettings(); return gRace.mSetKeeper; }

	void SetVersusKeeper(bool theKeeper)
	{
		LoadSettings();
		gRace.mSetKeeper = theKeeper;
		App()->RegistryWriteInteger("VersusKeeper", theKeeper ? 1 : 0);
	}

	///////////////////////////////////////////////////////////////////////////
	// Stage pets: in a Versus round each player may pick any pet they'd have at the
	// tank's final Adventure level (pets 0 .. 5*tank-2), whatever their profile holds.
	// The profile itself is never changed: the game asks these instead.
	///////////////////////////////////////////////////////////////////////////
	static bool StagePetsActive()
	{
		if (gRace.mPhase == PH_IDLE || (gRace.mKeeper && !RaceIAmFishKeeper()))
			return false;
		static int sProfilePets = -1;	// tests keep their profiles' pets unless they ask
		if (sProfilePets < 0)
			sProfilePets = getenv("INSANIQ_PROFILE_PETS") != nullptr ? 1 : 0;
		return sProfilePets == 0;
	}

	static int StagePetLast() { return 5 * gRace.mTank - 2; }

	int RaceStagePetOverride(int thePet)
	{
		if (!StagePetsActive())
			return -1;
		return thePet >= 0 && thePet <= StagePetLast() ? 1 : 0;
	}

	int RaceStagePetCount(int theProfileCount)
	{
		return StagePetsActive() ? StagePetLast() + 1 : theProfileCount;
	}

	int RaceStagePetSlots(int theProfileSlots)
	{
		return StagePetsActive() ? 3 : theProfileSlots;
	}

	void SetRaceCatchUp(bool theOn)
	{
		LoadSettings();
		gRace.mSetCatchUp = theOn;
		App()->RegistryWriteInteger("RaceCatchUp", theOn ? 1 : 0);
	}

	///////////////////////////////////////////////////////////////////////////
	// Helpers
	///////////////////////////////////////////////////////////////////////////
	static std::string Money(int v)
	{
		char aBuf[32];
		if (v < 0)
			snprintf(aBuf, sizeof(aBuf), "%d", v);
		else if (v >= 1000000)
			snprintf(aBuf, sizeof(aBuf), "%d,%03d,%03d", v / 1000000, v / 1000 % 1000, v % 1000);
		else if (v >= 1000)
			snprintf(aBuf, sizeof(aBuf), "%d,%03d", v / 1000, v % 1000);
		else
			snprintf(aBuf, sizeof(aBuf), "%d", v);
		return aBuf;
	}

	static std::string Clock(uint32_t theMs)
	{
		char aBuf[16];
		snprintf(aBuf, sizeof(aBuf), "%u:%02u", theMs / 60000, theMs / 1000 % 60);
		return aBuf;
	}

	static int MyEggs()
	{
		Board* b = RaceBoard();
		return b ? std::clamp(b->m0x43c - 1, 0, 3) : 0;
	}

	static int AttackPrice(int theAttack)
	{
		int aBase = (int)std::ceil(gRace.mEggPrice * kAttacks[theAttack].mPriceOfEgg / 25.0) * 25;
		aBase = std::max(25, aBase);
		if (gRace.mCatchUp && gRace.mRival.mKnown && MyEggs() < gRace.mRival.mEggs)
			aBase = std::max(25, (int)std::lround(aBase * kCatchUpDiscount / 25.0) * 25);
		return aBase;
	}

	// The alien and the poison raid wait out the first 45 s, so neither can end a race
	// before both tanks get going (Hunger + Poison on a new tank is a near-instant win).
	static bool WarmsUp(int theAttack) { return theAttack == ATK_ALIEN || theAttack == ATK_POISON; }

	static int AlienLockLeftMs()
	{
		if (gRace.mStartMs == 0)
			return kAlienWarmupMs;
		return std::max(0, kAlienWarmupMs - (int)(Now() - gRace.mStartMs));
	}

	static void Send(uint8_t theType, const ByteWriter& w)
	{
		S().SendMsg(theType, w.mData);
	}

	static void PlayBoardSound(int theSound)
	{
		if (Board* b = RaceBoard())
			b->PlaySample(theSound, 3, 1.0);
	}

	///////////////////////////////////////////////////////////////////////////
	// Attack bar: five buttons along the bottom of the tank (keys 1-5).
	///////////////////////////////////////////////////////////////////////////
	static bool TryAttack(int theAttack);

	class RaceBar : public Widget
	{
	public:
		static const int kButtonW = 72, kGap = 4;
		int		mOver = -1;

		RaceBar()
		{
			mMouseVisible = true;
			mHasTransparencies = true;
			mHasAlpha = true;
		}

		virtual void Update() override
		{
			Widget::Update();
			MarkDirty();
		}

		virtual void MouseMove(int x, int y) override
		{
			int anIdx = x / (kButtonW + kGap);
			mOver = (anIdx >= 0 && anIdx < ATK_COUNT && x % (kButtonW + kGap) < kButtonW) ? anIdx : -1;
		}

		virtual void MouseLeave() override
		{
			Widget::MouseLeave();
			mOver = -1;
		}

		virtual void MouseDown(int x, int y, int theClickCount) override
		{
			if (theClickCount < 0)
				return;
			int anIdx = x / (kButtonW + kGap);
			if (anIdx >= 0 && anIdx < ATK_COUNT && x % (kButtonW + kGap) < kButtonW)
				TryAttack(anIdx);
		}

		virtual void Draw(Graphics* g) override
		{
			Font* aFont = FONT_JUNGLEFEVER10OUTLINE;
			if (aFont == nullptr)
				return;
			g->SetFont(aFont);
			Board* b = RaceBoard();
			int aMoney = b ? b->mMoney : 0;
			bool aRunning = gRace.mPhase == PH_RUNNING;
			for (int i = 0; i < ATK_COUNT; i++)
			{
				int x = i * (kButtonW + kGap);
				int aPrice = RaceAdjustCost(b, AttackPrice(i));	// what Buy() will actually charge
				bool aCooling = !Elapsed(gRace.mCooldownUntil[i]);
				bool aFull = i == ATK_ALIEN && gRace.mRival.mAliens >= kMaxRivalAliens;
				int aLocked = WarmsUp(i) ? AlienLockLeftMs() : 0;
				bool aOk = aRunning && !aCooling && !aFull && aLocked == 0 && aMoney >= aPrice;
				g->SetColor(Color(0, 0, 0, 170));
				g->FillRect(x, 0, kButtonW, mHeight);
				if (aCooling)
				{
					int aLeft = (int)(gRace.mCooldownUntil[i] - Now());
					int w = kButtonW * std::clamp(aLeft, 0, kAttacks[i].mCooldownMs) / kAttacks[i].mCooldownMs;
					g->SetColor(Color(90, 90, 110, 150));
					g->FillRect(x, 0, w, mHeight);
				}
				Color aEdge = aOk ? (i == mOver ? Color(255, 255, 160) : Color(255, 200, 40)) : Color(110, 110, 120);
				g->SetColor(aEdge);
				g->DrawRect(x, 0, kButtonW - 1, mHeight - 1);
				char aTitle[32];
				snprintf(aTitle, sizeof(aTitle), "%d %s", i + 1, kAttacks[i].mName);
				g->SetColor(aOk ? Color(255, 255, 255) : Color(170, 170, 180));
				g->DrawString(aTitle, x + (kButtonW - aFont->StringWidth(aTitle)) / 2, aFont->GetAscent() + 2);
				std::string aSub = aFull ? "tank full" : (aLocked > 0 ? "in " + std::to_string(aLocked / 1000 + 1) + "s" : Money(aPrice));
				g->SetColor(aOk ? Color(120, 255, 120) : Color(150, 150, 160));
				g->DrawString(aSub, x + (kButtonW - aFont->StringWidth(aSub)) / 2, aFont->GetAscent() * 2 + 4);
			}
		}
	};

	///////////////////////////////////////////////////////////////////////////
	// Coin thief: a mini alien that swims around eating coins. Click it 3 times.
	///////////////////////////////////////////////////////////////////////////
	static void SendEvent(int theEvent, int theValue);

	class ThiefWidget : public Widget
	{
	public:
		double	mXD = 0, mYD = 0;
		int		mAge = 0;		// updates, for animation
		int		mAliveMs = 0;	// time in the tank, not counting pauses
		int		mHits = 0;
		int		mEaten = 0;
		int		mFlash = 0;
		bool	mFacingRight = true;
		bool	mLeaving = false;

		ThiefWidget()
		{
			mMouseVisible = true;
			mHasTransparencies = true;
			mHasAlpha = true;
			bool aFromLeft = (App()->mSeed->Next() & 1) != 0;
			mXD = aFromLeft ? -70.0 : 630.0;
			mYD = 110.0 + App()->mSeed->Next() % 220;
			Resize((int)mXD, (int)mYD, 80, 80);
		}

		void Kill()
		{
			if (gRace.mThief == this)
				gRace.mThief = nullptr;
			if (mWidgetManager != nullptr)
				mWidgetManager->RemoveWidget(this);
			App()->SafeDeleteWidget(this);
		}

		virtual void Update() override
		{
			Widget::Update();
			Board* b = RaceBoard();
			if (b == nullptr || gRace.mPhase != PH_RUNNING)
				return;
			if (b->mPause)
				return;
			mAge++;
			// The game updates every mFrameTime ms (28), not 100 times a second.
			mAliveMs += App()->mFrameTime;
			if (mFlash > 0)
				mFlash--;
			if (mAliveMs > kThiefLifeMs || mEaten >= 6)
				mLeaving = true;

			double tx = mXD, ty = mYD;
			Coin* aTarget = nullptr;
			if (!mLeaving)
			{
				double aBest = 1e18;
				for (Coin* c : *b->mCoinList)
				{
					if (c == nullptr || c->m0x198 || c->mCoinType == COIN_NOTE)
						continue;
					double dx = (c->mX + c->mWidth / 2) - (mXD + 40), dy = (c->mY + c->mHeight / 2) - (mYD + 40);
					double d = dx * dx + dy * dy;
					if (d < aBest)
					{
						aBest = d;
						aTarget = c;
					}
				}
				if (aTarget != nullptr)
				{
					tx = aTarget->mX + aTarget->mWidth / 2 - 40;
					ty = aTarget->mY + aTarget->mHeight / 2 - 40;
				}
				else
				{
					tx = 290 + 180 * std::sin(mAge / 90.0);
					ty = 200 + 60 * std::sin(mAge / 55.0);
				}
			}
			else
			{
				tx = mFacingRight ? 700 : -120;
				ty = mYD;
			}
			double dx = tx - mXD, dy = ty - mYD;
			double aDist = std::sqrt(dx * dx + dy * dy);
			double aSpeed = (mLeaving ? kThiefLeaveSpeed : kThiefSpeed) * App()->mFrameTime / 1000.0;
			if (aDist > 0.5)
			{
				mXD += dx / aDist * std::min(aSpeed, aDist);
				mYD += dy / aDist * std::min(aSpeed, aDist);
				if (std::fabs(dx) > 1)
					mFacingRight = dx > 0;
			}
			Move((int)mXD, (int)mYD);
			MarkDirty();

			if (aTarget != nullptr && aDist < 14)
			{
				aTarget->Remove();		// gone, nobody gets paid
				b->PlayChompSound(false);
				mEaten++;
			}
			if (mLeaving && (mXD < -110 || mXD > 690))
				Kill();
		}

		virtual void MouseDown(int x, int y, int theClickCount) override
		{
			if (theClickCount < 0 || gRace.mPhase != PH_RUNNING || mLeaving)
				return;
			mHits++;
			mFlash = 10;
			PlayBoardSound(SOUND_PUNCH_ID);
			if (mHits >= 3)
			{
				int aRefund = gRace.mThiefPrice / 2;
				if (Board* b = RaceBoard())
				{
					b->Unk07(aRefund);
					b->PlaySample(SOUND_DIAMOND_ID, 3, 1.0);
				}
				S().AddToast("You squashed the coin thief! +" + Money(aRefund), Me(), 300);
				SendEvent(EV_THIEF_SQUASHED, aRefund);
				Kill();
			}
		}

		virtual void Draw(Graphics* g) override
		{
			Image* anImg = IMAGE_MINISYLV;
			if (anImg == nullptr)
				return;
			int aFrame = (mAge / 4) % 10;
			Rect aSrc(aFrame * 80, 0, 80, 80);
			if (aSrc.mX + 80 > anImg->mWidth)
				aSrc.mX = 0;
			g->SetColorizeImages(true);
			g->SetColor(mFlash > 0 ? Color(255, 255, 255) : Color(255, 170, 60));
			g->DrawImageMirror(anImg, 0, 0, aSrc, mFacingRight);
			g->SetColorizeImages(false);
			// Hit pips
			for (int i = 0; i < 3; i++)
			{
				g->SetColor(i < mHits ? Color(255, 60, 60) : Color(0, 0, 0, 150));
				g->FillRect(28 + i * 9, 4, 7, 5);
			}
		}
	};

	static void RemoveWidgets()
	{
		WidgetManager* wm = App() ? App()->mWidgetManager : nullptr;
		if (gRace.mBar != nullptr)
		{
			if (wm)
				wm->RemoveWidget(gRace.mBar);
			App()->SafeDeleteWidget(gRace.mBar);
			gRace.mBar = nullptr;
		}
		if (gRace.mThief != nullptr)
			gRace.mThief->Kill();
	}

	///////////////////////////////////////////////////////////////////////////
	// Hike: shop buttons show the inflated price while it lasts.
	///////////////////////////////////////////////////////////////////////////
	static void ShowShopPrices(Board* b, bool theHiked)
	{
		if (b == nullptr)
			return;
		for (int i = 0; i < SLOT_END; i++)
		{
			MenuButtonWidget* aBtn = b->GetMenuButtonById(i);
			if (aBtn != nullptr)
				aBtn->SetSlotPrice(theHiked ? RaceAdjustCost(b, b->mSlotPrices[i]) : b->mSlotPrices[i]);
		}
	}

	int RaceAdjustCost(Board* theBoard, int theCost)
	{
		if (theBoard == nullptr || theBoard != RaceBoard() || Elapsed(gRace.mHikeUntil) || theCost == theBoard->m0x4ac)
			return theCost;
		return (int)std::lround(theCost * 1.3 / 5.0) * 5;
	}

	///////////////////////////////////////////////////////////////////////////
	// Phases
	///////////////////////////////////////////////////////////////////////////
	static void SetPhase(RacePhase thePhase)
	{
		gRace.mPhase = thePhase;
		gRace.mPhaseSince = Now();
	}

	static void ResetRound()
	{
		gRace.mLocalReady = gRace.mPeerReady = false;
		gRace.mCountdownAt = gRace.mStartMs = gRace.mLastStateSent = 0;
		for (uint32_t& c : gRace.mCooldownUntil)
			c = 0;
		gRace.mGlobalCdUntil = 0;
		gRace.mMurkUntil = gRace.mMurkStart = gRace.mHikeUntil = gRace.mBlockUntil = 0;
		gRace.mSentAliens.clear();
		gRace.mRival = RivalInfo();
		gRace.mWinner = -1;
		gRace.mReason = gRace.mFinisher = 0;
		gRace.mRaceMs = gRace.mResultAt = 0;
		gRace.mAttacksSent = gRace.mDefended = 0;
		gRace.mThiefPrice = 0;
		gRace.mRaidLeft = gRace.mRaidIndex = 0;
	}

	static void SendReady()
	{
		gRace.mLocalReady = true;
		ByteWriter w;
		w.U32(gRace.mRaceId);
		Send(MSG_RACE_READY, w);
	}

	static void HostGo();

	// Alien Keeper, the lair side: no tank of our own; watch the fish keeper's.
	static void BeginLair()
	{
		WinFishApp* anApp = App();
		Session& s = S();
		ResetRound();
		SetPhase(PH_SETUP);
		s.Log("Race %u: Alien Keeper, I run the lair (tank %d-%d)", gRace.mRaceId, gRace.mTank, gRace.mLevel);
		if (s.GetRole() == ROLE_HOST)
			s.SuspendStreaming(true);
		else
			s.SuspendGuestView();
		anApp->KillDialog(kDialogCoop);
		anApp->KillDialog(kDialogVersus);
		anApp->KillDialog(kDialogGuestMenu);
		anApp->CleanDialogs();
		KeeperBegin(false);
		s.StartKeeperView();
		SetPhase(PH_READY);
		SendReady();
		if (s.GetRole() == ROLE_HOST && gRace.mPeerReady)
			HostGo();
	}

	static void BeginLocal()
	{
		if (gRace.mKeeper && !RaceIAmFishKeeper())
		{
			BeginLair();
			return;
		}
		WinFishApp* anApp = App();
		Session& s = S();
		ResetRound();
		SetPhase(PH_SETUP);
		if (gRace.mKeeper)
			KeeperBegin(true);
		s.Log("Race %u: setting up tank %d-%d (catch-up %s)", gRace.mRaceId, gRace.mTank, gRace.mLevel, gRace.mCatchUp ? "on" : "off");
		if (s.GetRole() == ROLE_HOST)
			s.SuspendStreaming(true);
		else
			s.SuspendGuestView();
		anApp->KillDialog(kDialogCoop);
		anApp->KillDialog(kDialogVersus);
		anApp->KillDialog(kDialogGuestMenu);
		anApp->CleanDialogs();
		if (anApp->mBoard != nullptr)
			anApp->RemoveBoard();			// an ordinary game underneath: saved as usual
		anApp->RemoveGameSelector();
		anApp->RemoveHelpScreen();
		anApp->RemoveHighScoreScreen();
		anApp->RemovePetsScreen();
		anApp->RemoveHatchScreen();
		anApp->RemoveSimSetupScreen();
		anApp->RemoveTankScreen();
		anApp->RemoveStoryScreen();
		anApp->RemoveStoreScreen();
		anApp->RemoveSimFishScreen();
		anApp->RemoveBonusScreen();
		anApp->RemoveInterludeScreen();
		anApp->mGameMode = GAMEMODE_ADVENTURE;
		gRace.mPendingBoard = true;
		if (anApp->mCurrentProfile != nullptr && RaceStagePetCount(anApp->mCurrentProfile->mNumOfUnlockedPets) > 3)
		{
			for (int i = 0; i < 24; i++)
				anApp->mCurrentProfile->m0x5a[i] = false;
			anApp->SwitchToPetsScreen();	// pick pets, then the game starts (RaceOnGameStarted)
		}
		else
			anApp->StartGame();
	}

	static void BeginCountdown()
	{
		SetPhase(PH_COUNTDOWN);
		gRace.mCountdownAt = Now();
		if (gRace.mBar == nullptr && !gRace.mKeeper)
		{
			gRace.mBar = new RaceBar();
			gRace.mBar->Resize(kBarX, kBarY, ATK_COUNT * (RaceBar::kButtonW + RaceBar::kGap) - RaceBar::kGap, 30);
			App()->mWidgetManager->AddWidget(gRace.mBar);
		}
		S().Log("Race %u: countdown", gRace.mRaceId);
	}

	static void HostGo()
	{
		ByteWriter w;
		w.U32(gRace.mRaceId);
		Send(MSG_RACE_GO, w);
		BeginCountdown();
	}

	static void EndRace()
	{
		WinFishApp* anApp = App();
		Session& s = S();
		bool hadBoard = RaceBoard() != nullptr;
		SetPhase(PH_IDLE);
		gRace.mPendingBoard = false;
		RemoveWidgets();
		gRace.mSentAliens.clear();
		if (gRace.mKeeper)
		{
			KeeperEnd();
			s.StopKeeperStream();
			s.StopKeeperView();
		}
		if (hadBoard)
			anApp->LeaveGameBoard();		// race tanks are never saved (RaceOwnsBoard)
		else
		{
			anApp->RemovePetsScreen();
			if (anApp->mGameSelector == nullptr)
				anApp->SwitchToGameSelector();
		}
		if (s.GetRole() == ROLE_HOST)
			s.SuspendStreaming(false);
		else
			s.ResumeGuestView();
		s.Log("Race %u: over", gRace.mRaceId);
	}

	static void Cancel(const std::string& theWhy, bool theTellPeer)
	{
		if (gRace.mPhase == PH_IDLE)
			return;
		if (theTellPeer)
		{
			ByteWriter w;
			w.Str(theWhy);
			Send(MSG_RACE_CANCEL, w);
		}
		S().Log("Race %u: cancelled (%s)", gRace.mRaceId, theWhy.c_str());
		EndRace();
		if (!theWhy.empty())
			S().AddToast(theWhy, -1, 500);
	}

	static void ShowResult(int theWinner, int theReason, int theFinisher, uint32_t theMs)
	{
		gRace.mWinner = theWinner;
		gRace.mReason = theReason;
		gRace.mFinisher = theFinisher;
		gRace.mRaceMs = theMs;
		gRace.mResultAt = Now();
		SetPhase(PH_RESULT);
		if (Board* b = RaceBoard())
		{
			b->mPause = true;
			b->PlaySample(theWinner == Me() ? SOUND_APPLAUSE_ID : SOUND_EVILLAFF_ID, 3, 1.0);
		}
		RemoveWidgets();
		S().Log("Race %u: result winner=%d reason=%d finisher=%d time=%u score %d-%d", gRace.mRaceId, theWinner, theReason, theFinisher, theMs, gRace.mWins[0], gRace.mWins[1]);
	}

	// Host: the race is decided by whichever finish arrives first.
	static void HostDecide(int theFinisher, int theReason)
	{
		if (gRace.mPhase == PH_RESULT || gRace.mPhase == PH_IDLE)
			return;
		int aWinner = (theReason == FIN_EGG || theReason == FIN_SURVIVED) ? theFinisher : 1 - theFinisher;
		gRace.mWins[aWinner]++;
		uint32_t aMs = gRace.mStartMs != 0 ? Now() - gRace.mStartMs : 0;
		ByteWriter w;
		w.U8((uint8_t)aWinner);
		w.U8((uint8_t)theReason);
		w.U8((uint8_t)theFinisher);
		w.U32(aMs);
		w.U8((uint8_t)std::min(gRace.mWins[0], 255));
		w.U8((uint8_t)std::min(gRace.mWins[1], 255));
		Send(MSG_RACE_RESULT, w);
		ShowResult(aWinner, theReason, theFinisher, aMs);
	}

	static void LocalFinished(int theReason)
	{
		RacePhase p = gRace.mPhase;
		bool ok = p == PH_RUNNING || (theReason == FIN_FORFEIT && (p == PH_READY || p == PH_COUNTDOWN || p == PH_SETUP));
		if (!ok)
			return;
		SetPhase(PH_FINISHED);
		if (Board* b = RaceBoard())
			b->mPause = true;
		S().Log("Race %u: I finished (reason %d)", gRace.mRaceId, theReason);
		if (S().GetRole() == ROLE_HOST)
			HostDecide(0, theReason);
		else
		{
			ByteWriter w;
			w.U8((uint8_t)theReason);
			Send(MSG_RACE_FINISH, w);
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Status snapshots
	///////////////////////////////////////////////////////////////////////////
	template <class T>
	static void AddDots(std::vector<RivalInfo::Dot>& theDots, std::vector<T*>* theList, int theKind, int& theCount, bool theCountIt)
	{
		if (theList == nullptr)
			return;
		for (T* o : *theList)
		{
			if (o == nullptr)
				continue;
			if (theCountIt)
				theCount++;
			if (theDots.size() >= 48)
				continue;
			int cx = o->mX + o->mWidth / 2, cy = o->mY + o->mHeight / 2;
			RivalInfo::Dot d;
			d.x = (uint8_t)std::clamp((cx - kTankX) * 255 / kTankW, 0, 255);
			d.y = (uint8_t)std::clamp((cy - kTankY) * 255 / kTankH, 0, 255);
			d.kind = (uint8_t)theKind;
			theDots.push_back(d);
		}
	}

	static void SendState()
	{
		Board* b = RaceBoard();
		ByteWriter w;
		std::vector<RivalInfo::Dot> aDots;
		int aFish = 0, anAliens = 0, aDummy = 0;
		int aFlags = 0;
		if (b != nullptr)
		{
			AddDots(aDots, b->mAlienList, DOT_ALIEN, anAliens, true);
			AddDots(aDots, b->mFishList, DOT_GUPPY, aFish, true);
			AddDots(aDots, b->mBreederList, DOT_BREEDER, aFish, true);
			AddDots(aDots, b->mOscarList, DOT_CARNIVORE, aFish, true);
			AddDots(aDots, b->mUltraList, DOT_ULTRA, aFish, true);
			AddDots(aDots, b->mGekkoList, DOT_OTHER, aFish, true);
			AddDots(aDots, b->mPentaList, DOT_OTHER, aFish, true);
			AddDots(aDots, b->mGrubberList, DOT_OTHER, aFish, true);
			AddDots(aDots, b->mOtherTypePetList, DOT_PET, aDummy, false);
			AddDots(aDots, b->mFishTypePetList, DOT_PET, aDummy, false);
			anAliens += (int)b->mBilaterusList->size();
			if (!Elapsed(gRace.mMurkUntil))
				aFlags |= RF_MURK;
			if (!Elapsed(gRace.mHikeUntil))
				aFlags |= RF_HIKE;
			if (!Elapsed(gRace.mBlockUntil))
				aFlags |= RF_BLOCK;
			if (gRace.mThief != nullptr)
				aFlags |= RF_THIEF;
			if (b->mPause && gRace.mPhase == PH_RUNNING)
				aFlags |= RF_PAUSED;
		}
		else
			aFlags |= RF_SETUP;
		w.I32(b ? b->mMoney : 0);
		w.U8((uint8_t)MyEggs());
		w.U8((uint8_t)std::min(aFish, 255));
		w.U8((uint8_t)std::min(anAliens, 255));
		w.U8((uint8_t)aFlags);
		w.U8((uint8_t)aDots.size());
		for (const RivalInfo::Dot& d : aDots)
		{
			w.U8(d.x);
			w.U8(d.y);
			w.U8(d.kind);
		}
		Send(MSG_RACE_STATE, w);
		gRace.mLastStateSent = Now();
	}

	static void ReadState(ByteReader& r)
	{
		RivalInfo i;
		i.mMoney = r.I32();
		i.mEggs = r.U8();
		i.mFish = r.U8();
		i.mAliens = r.U8();
		i.mFlags = r.U8();
		int n = r.U8();
		for (int k = 0; k < n && !r.mError; k++)
		{
			RivalInfo::Dot d;
			d.x = r.U8();
			d.y = r.U8();
			d.kind = r.U8();
			i.mDots.push_back(d);
		}
		if (r.mError)
			return;
		i.mKnown = true;
		gRace.mRival = i;
	}

	///////////////////////////////////////////////////////////////////////////
	// Attacks
	///////////////////////////////////////////////////////////////////////////
	static bool TryAttack(int theAttack)
	{
		Board* b = RaceBoard();
		if (b == nullptr || gRace.mPhase != PH_RUNNING || theAttack < 0 || theAttack >= ATK_COUNT)
			return false;
		if (!Elapsed(gRace.mCooldownUntil[theAttack]) || !Elapsed(gRace.mGlobalCdUntil))
			return false;
		if (WarmsUp(theAttack) && AlienLockLeftMs() > 0)
			return false;
		if (theAttack == ATK_ALIEN && gRace.mRival.mAliens >= kMaxRivalAliens)
		{
			S().AddToast(S().PlayerName(Them()) + "'s tank is already full of aliens!", -1, 250);
			return false;
		}
		int aPrice = AttackPrice(theAttack);
		if (!b->Buy(aPrice, true))
			return false;
		gRace.mCooldownUntil[theAttack] = Now() + kAttacks[theAttack].mCooldownMs;
		gRace.mGlobalCdUntil = Now() + kGlobalCooldownMs;
		gRace.mAttacksSent++;
		ByteWriter w;
		w.U8((uint8_t)theAttack);
		w.I32(aPrice);
		Send(MSG_RACE_ATTACK, w);
		b->PlaySample(SOUND_EVILLAFF_ID, 3, 1.0);
		S().AddToast(std::string("You sent ") + S().PlayerName(Them()) + " " + kAttacks[theAttack].mVerb + "!", Me(), 250);
		S().Log("Race %u: sent attack %d for %d", gRace.mRaceId, theAttack, aPrice);
		return true;
	}

	static void SendEvent(int theEvent, int theValue)
	{
		ByteWriter w;
		w.U8((uint8_t)theEvent);
		w.I32(theValue);
		Send(MSG_RACE_EVENT, w);
	}

	static void ReceiveAttack(int theAttack, int thePrice)
	{
		Board* b = RaceBoard();
		if (b == nullptr || gRace.mPhase != PH_RUNNING || theAttack < 0 || theAttack >= ATK_COUNT)
			return;
		Session& s = S();
		s.AddToast(s.PlayerName(Them()) + " sent you " + kAttacks[theAttack].mVerb + "!", Them(), 350);
		s.Log("Race %u: received attack %d (%d)", gRace.mRaceId, theAttack, thePrice);
		switch (theAttack)
		{
		case ATK_ALIEN:
		{
			int aType = b->mTank <= 1 ? ALIEN_WEAK_SYLV : (b->mTank == 2 ? ALIEN_STRONG_SYLV : ALIEN_BALROG);
			b->mCrosshair2X = App()->mSeed->Next() % 450 + 20;
			b->mCrosshair2Y = App()->mSeed->Next() % 195 + 105;
			size_t aBefore = b->mAlienList->size();
			b->SpawnAlien(aType, b->mCrosshair2X, b->mCrosshair2Y, false);
			if (b->mAlienList->size() > aBefore)
				gRace.mSentAliens.push_back({ b->mAlienList->back(), thePrice });
			break;
		}
		case ATK_HUNGER:
		{
			for (Fish* f : *b->mFishList)
				if (f != nullptr && f->mHunger > 400)
					f->mHunger = 400;
			for (Breeder* f : *b->mBreederList)
				if (f != nullptr && f->mHunger > 400)
					f->mHunger = 400;
			b->PlaySample(SOUND_SLURP_ID, 3, 1.0);
			break;
		}
		case ATK_MURK:
			if (Elapsed(gRace.mMurkUntil))
				gRace.mMurkStart = Now();
			gRace.mMurkUntil = Now() + kMurkMs;
			b->PlaySample(SOUND_SPLASHBIG_ID, 3, 1.0);
			break;
		case ATK_THIEF:
			gRace.mThiefPrice = thePrice;
			if (gRace.mThief == nullptr)
			{
				gRace.mThief = new ThiefWidget();
				App()->mWidgetManager->AddWidget(gRace.mThief);
			}
			else
				gRace.mThief->mAliveMs = 0;
			b->PlaySample(SOUND_GUFFAW_ID, 3, 1.0);
			break;
		case ATK_HIKE:
			gRace.mHikeUntil = Now() + kHikeMs;
			ShowShopPrices(b, true);
			b->PlaySample(SOUND_BUZZER_ID, 3, 1.0);
			break;
		case ATK_BLOCK:
			gRace.mBlockUntil = Now() + kBlockMs;
			b->PlaySample(SOUND_INTERFER_ID, 3, 1.0);
			break;
		case ATK_POISON:
			gRace.mRaidLeft = kRaidPotions;
			gRace.mRaidIndex = 0;
			gRace.mRaidFromRight = (App()->mSeed->Next() & 1) != 0;
			gRace.mRaidNextAt = Now() + 600;	// a beat after the warning
			b->PlaySample(SOUND_AWOOGA_ID, 3, 1.0);
			break;
		}
	}

	static void DropRaidPotion(Board* b)
	{
		int i = gRace.mRaidFromRight ? kRaidPotions - 1 - gRace.mRaidIndex : gRace.mRaidIndex;
		int x = kRaidLeftX + i * (kRaidRightX - kRaidLeftX) / (kRaidPotions - 1);
		Food* aPotion = new Food(x, 70, 0, false, 0);
		aPotion->mFoodType = 3;		// drawn and falls like a star potion
		aPotion->mCoopPoison = true;
		aPotion->mCantEatTimer = 0;
		b->AddGameObject(aPotion);
		App()->mWidgetManager->AddWidget(aPotion);
		b->SortGameObjects();
		b->PlaySample(SOUND_DROPFOOD_ID, 3, 1.0);
		gRace.mRaidIndex++;
		gRace.mRaidLeft--;
		gRace.mRaidNextAt = Now() + kRaidEveryMs;
	}

	bool RaceCoinsBlocked()
	{
		Board* b = RaceBoard();
		return b != nullptr && App()->mBoard == b && gRace.mPhase == PH_RUNNING && !Elapsed(gRace.mBlockUntil);
	}

	void RacePoisonClicked(GameObject* thePotion)
	{
		Board* b = RaceBoard();
		if (b == nullptr || thePotion == nullptr)
			return;
		b->SpawnShot(thePotion->mX + 10, thePotion->mY + 5, 3);
		b->PlaySample(SOUND_PUNCH_ID, 3, 1.0);
		static_cast<Food*>(thePotion)->Remove();
	}

	void RaceOnAlienRemoved(GameObject* theAlien)
	{
		if (gRace.mKeeper)
		{
			KeeperAlienRemoved(theAlien);
			return;
		}
		for (size_t i = 0; i < gRace.mSentAliens.size(); i++)
		{
			if (gRace.mSentAliens[i].mAlien != theAlien)
				continue;
			int aRefund = gRace.mSentAliens[i].mPrice / 2;
			gRace.mSentAliens.erase(gRace.mSentAliens.begin() + i);
			Board* b = RaceBoard();
			if (b == nullptr || gRace.mPhase != PH_RUNNING)
				return;
			b->Unk07(aRefund);
			gRace.mDefended++;
			S().AddToast("You beat " + S().PlayerName(Them()) + "'s alien! +" + Money(aRefund), Me(), 300);
			SendEvent(EV_DEFENDED, aRefund);
			return;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Public lifecycle
	///////////////////////////////////////////////////////////////////////////
	bool RaceBusy()
	{
		return gRace.mPhase != PH_IDLE;
	}

	bool RaceIsKeeper() { return gRace.mKeeper && gRace.mPhase != PH_IDLE; }
	int RaceFishKeeper() { return gRace.mFishKeeper; }
	bool RaceIAmFishKeeper() { return gRace.mFishKeeper == Me(); }
	Board* RaceTankBoard() { return RaceBoard(); }
	bool RaceRunning() { return gRace.mPhase == PH_RUNNING; }
	uint32_t RaceRunningMs() { return gRace.mPhase == PH_RUNNING && gRace.mStartMs != 0 ? Now() - gRace.mStartMs : 0; }
	int RaceRivalAliens() { return gRace.mRival.mAliens; }
	int RaceRivalFish() { return gRace.mRival.mFish; }
	int RaceRivalMoney() { return gRace.mRival.mMoney; }
	int RaceRivalEggs() { return gRace.mRival.mEggs; }
	int RaceRoundTank() { return gRace.mTank; }

	void RaceKeeperLairDefeated()
	{
		if (gRace.mKeeper && !RaceIAmFishKeeper())
			LocalFinished(FIN_LOST);
	}

	void RaceKeeperSurvived()
	{
		if (gRace.mKeeper && RaceIAmFishKeeper())
			LocalFinished(FIN_SURVIVED);
	}

	int RaceKeeperNextFishKeeper() { return gRace.mKeeperRounds % 2 == 0 ? 0 : 1; }

	void RaceKeeperForfeit()
	{
		if (gRace.mKeeper && !RaceIAmFishKeeper())
			LocalFinished(FIN_FORFEIT);
	}

	bool RaceHidesToasts()
	{
		return gRace.mPhase == PH_RESULT || gRace.mPhase == PH_COUNTDOWN;
	}

	bool RaceCanStart(std::string& theWhy)
	{
		Session& s = S();
		if (!s.HasGuest())
		{
			theWhy = "Waiting for player 2 to join.";
			return false;
		}
		if (gRace.mPhase != PH_IDLE)
		{
			theWhy = "A race is already on.";
			return false;
		}
		if (App()->mBoard != nullptr)
		{
			theWhy = "Leave your level first (Menu > Main Menu).";
			return false;
		}
		if (App()->mCurrentProfile == nullptr)
		{
			theWhy = "Pick a player profile first.";
			return false;
		}
		return true;
	}

	void RaceHostStart()
	{
		std::string aWhy;
		if (S().GetRole() != ROLE_HOST || !RaceCanStart(aWhy))
			return;
		LoadSettings();
		gRace.mRaceId++;
		gRace.mTank = gRace.mSetTank;
		gRace.mLevel = gRace.mSetLevel;
		gRace.mCatchUp = gRace.mSetCatchUp;
		gRace.mKeeper = gRace.mSetKeeper;
		if (gRace.mKeeper)
		{
			// Roles swap every round: the host keeps the fish first.
			gRace.mFishKeeper = (gRace.mKeeperRounds % 2 == 0) ? 0 : 1;
			gRace.mKeeperRounds++;
		}
		ByteWriter w;
		w.U32(gRace.mRaceId);
		w.U8((uint8_t)gRace.mTank);
		w.U8((uint8_t)gRace.mLevel);
		w.U8(gRace.mCatchUp ? 1 : 0);
		w.U8(gRace.mKeeper ? 1 : 0);
		w.U8((uint8_t)gRace.mFishKeeper);
		Send(MSG_RACE_SETUP, w);
		BeginLocal();
	}

	void RaceHostCancel()
	{
		if (S().GetRole() == ROLE_HOST)
			Cancel("The host called off the race.", true);
	}

	void RacePeerLost(const std::string& theWhy)
	{
		if (gRace.mPhase == PH_IDLE)
			return;
		S().Log("Race %u: peer lost (%s)", gRace.mRaceId, theWhy.c_str());
		EndRace();	// the session already tells the player the other one left
	}

	void RaceHandleMessage(uint8_t theType, ByteReader& r)
	{
		Session& s = S();
		bool isHost = s.GetRole() == ROLE_HOST;
		switch (theType)
		{
		case MSG_RACE_SETUP:
		{
			uint32_t anId = r.U32();
			int aTank = r.U8(), aLevel = r.U8();
			bool aCatch = r.U8() != 0;
			bool aKeeper = r.U8() != 0;
			int aFishKeeper = r.U8() & 1;
			if (r.mError || isHost)
				return;
			if (gRace.mPhase != PH_IDLE)
				EndRace();
			if (App()->mCurrentProfile == nullptr)
			{
				ByteWriter w;
				w.Str(s.GetLocalName() + " has no player profile yet.");
				Send(MSG_RACE_CANCEL, w);
				return;
			}
			gRace.mRaceId = anId;
			gRace.mTank = std::clamp(aTank, 1, 4);
			gRace.mLevel = std::clamp(aLevel, 1, 5);
			gRace.mCatchUp = aCatch;
			gRace.mKeeper = aKeeper;
			gRace.mFishKeeper = aFishKeeper;
			BeginLocal();
			break;
		}
		case MSG_RACE_READY:
		{
			uint32_t anId = r.U32();
			if (r.mError || anId != gRace.mRaceId || gRace.mPhase == PH_IDLE)
				return;
			gRace.mPeerReady = true;
			if (isHost && gRace.mLocalReady && gRace.mPhase == PH_READY)
				HostGo();
			break;
		}
		case MSG_RACE_GO:
		{
			uint32_t anId = r.U32();
			if (r.mError || isHost || anId != gRace.mRaceId)
				return;
			if (gRace.mPhase == PH_READY)
				BeginCountdown();
			break;
		}
		case MSG_RACE_STATE:
			if (gRace.mPhase != PH_IDLE)
				ReadState(r);
			break;
		case MSG_RACE_ATTACK:
		{
			int anAttack = r.U8();
			int aPrice = r.I32();
			if (!r.mError)
				ReceiveAttack(anAttack, aPrice);
			break;
		}
		case MSG_RACE_FINISH:
		{
			int aReason = r.U8();
			if (!r.mError && isHost)
				HostDecide(1, aReason);
			break;
		}
		case MSG_RACE_RESULT:
		{
			int aWinner = r.U8(), aReason = r.U8(), aFinisher = r.U8();
			uint32_t aMs = r.U32();
			int w0 = r.U8(), w1 = r.U8();
			if (r.mError || isHost || gRace.mPhase == PH_IDLE)
				return;
			gRace.mWins[0] = w0;
			gRace.mWins[1] = w1;
			ShowResult(aWinner & 1, aReason, aFinisher & 1, aMs);
			break;
		}
		case MSG_RACE_EVENT:
		{
			int anEvent = r.U8();
			int aValue = r.I32();
			if (r.mError)
				return;
			if (anEvent == EV_DEFENDED)
				s.AddToast(s.PlayerName(Them()) + " beat your alien (+" + Money(aValue) + " for them)", Them(), 300);
			else if (anEvent == EV_THIEF_SQUASHED)
				s.AddToast(s.PlayerName(Them()) + " squashed your coin thief", Them(), 300);
			break;
		}
		case MSG_RACE_CANCEL:
		{
			std::string aWhy = r.Str();
			Cancel(aWhy.empty() ? "The race was called off." : aWhy, false);
			break;
		}
		case MSG_KEEPER_LAUNCH:
		case MSG_KEEPER_POWER:
		case MSG_KEEPER_CURSOR:
		case MSG_KEEPER_LAIR:
		case MSG_KEEPER_EVENT:
			if (gRace.mKeeper && gRace.mPhase != PH_IDLE)
				KeeperHandleMessage(theType, r);
			break;
		default:
			break;
		}
	}

	void RaceUpdate()
	{
		if (gRace.mPhase == PH_IDLE)
			return;
		Session& s = S();
		Board* b = RaceBoard();
		bool isHost = s.GetRole() == ROLE_HOST;

		if (gRace.mPhase == PH_SETUP && isHost && Elapsed(gRace.mPhaseSince + kSetupTimeoutMs))
		{
			Cancel("The race couldn't get started.", true);
			return;
		}

		// Keep our widgets above the tank's creatures, but under any dialog.
		if (App()->mDialogList.empty())
		{
			if (gRace.mThief != nullptr)
				App()->mWidgetManager->BringToFront(gRace.mThief);
			if (gRace.mBar != nullptr)
				App()->mWidgetManager->BringToFront(gRace.mBar);
		}

		if (gRace.mRaidLeft > 0 && gRace.mPhase == PH_RUNNING && b != nullptr && !b->mPause && Elapsed(gRace.mRaidNextAt))
			DropRaidPotion(b);

		if (gRace.mPhase == PH_COUNTDOWN && Elapsed(gRace.mCountdownAt + kCountdownMs))
		{
			SetPhase(PH_RUNNING);
			gRace.mStartMs = Now();
			if (b != nullptr && App()->mDialogList.empty())
				b->PauseGame(false);
			if (b != nullptr && !gRace.mKeeper)
				b->PlaySample(SOUND_AWOOGA_ID, 3, 1.0);
			s.Log("Race %u: go!", gRace.mRaceId);
		}

		if (gRace.mPhase == PH_RUNNING && b != nullptr && gRace.mHikeUntil != 0 && Elapsed(gRace.mHikeUntil))
		{
			gRace.mHikeUntil = 0;
			ShowShopPrices(b, false);
		}

		if (gRace.mKeeper)
			KeeperUpdate();

		bool aSendsState = !gRace.mKeeper || RaceIAmFishKeeper();
		if (aSendsState && gRace.mPhase != PH_RESULT && (int32_t)(Now() - gRace.mLastStateSent) >= 200)
			SendState();

		if (gRace.mPhase == PH_RESULT && Elapsed(gRace.mResultAt + kResultMs))
			EndRace();
	}

	///////////////////////////////////////////////////////////////////////////
	// Game hooks
	///////////////////////////////////////////////////////////////////////////
	bool RaceOwnsBoard(Board* theBoard)
	{
		return theBoard != nullptr && theBoard == gRace.mBoard;
	}

	void RaceOverrideLevel(int& theTank, int& theLevel)
	{
		if (!gRace.mPendingBoard)
			return;
		theTank = gRace.mTank;
		theLevel = gRace.mLevel;
	}

	void RaceOnGameStarted()
	{
		if (!gRace.mPendingBoard)
			return;
		gRace.mPendingBoard = false;
		Board* b = App()->mBoard;
		if (b == nullptr || gRace.mPhase != PH_SETUP)
			return;
		gRace.mBoard = b;
		b->mPause = true;					// frozen until the countdown ends
		if (b->mMessageWidget != nullptr)
			b->mMessageWidget->SetVisible(false);	// the attack bar lives where the tips scroll
		// Versus plays each tank's final level, whose egg is priced for the end of a long
		// Adventure run (Tank 4: 99,999 a piece). A round wants a reachable goal.
		static const int kVersusEggPrice[4] = { 1500, 3000, 5000, 10000 };
		if (gRace.mLevel == 5 && gRace.mTank >= 1 && gRace.mTank <= 4)
			b->UpdateSlotPrice(SLOT_EGG, kVersusEggPrice[gRace.mTank - 1]);
		gRace.mEggPrice = std::max(50, b->mSlotPrices[SLOT_EGG]);
		// Alien Keeper: a fish tank's economy starts slowly and the lair's doesn't, so the
		// fish keeper gets a head start (two more guppies; a breeder on Tank 4).
		if (gRace.mKeeper && RaceIAmFishKeeper())
		{
			if (b->mTank == 4)
				b->SpawnBreeder(App()->mSeed->Next() % 520 + 20, App()->mSeed->Next() % 265 + 105);
			else
				for (int i = 0; i < 2; i++)
					b->SpawnGuppy(App()->mSeed->Next() % 520 + 20, App()->mSeed->Next() % 265 + 105);
		}
		SetPhase(PH_READY);
		gRace.mLocalReady = true;
		ByteWriter w;
		w.U32(gRace.mRaceId);
		Send(MSG_RACE_READY, w);
		SendState();
		S().Log("Race %u: my tank %d-%d is ready (egg %d)", gRace.mRaceId, b->mTank, b->mLevel, gRace.mEggPrice);
		if (S().GetRole() == ROLE_HOST && gRace.mPeerReady)
			HostGo();
	}

	bool RaceOnEggComplete(Board* theBoard)
	{
		if (!RaceOwnsBoard(theBoard))
			return false;
		SendState();
		LocalFinished(FIN_EGG);
		return true;
	}

	bool RaceOnTankLost(Board* theBoard)
	{
		if (!RaceOwnsBoard(theBoard))
			return false;
		if (gRace.mPhase == PH_RUNNING)
			LocalFinished(FIN_LOST);
		theBoard->mPause = true;
		return true;
	}

	void RaceOnBoardDestroyed(Board* theBoard)
	{
		if (theBoard != gRace.mBoard)
			return;
		gRace.mBoard = nullptr;
		gRace.mSentAliens.clear();
		RemoveWidgets();
		// Left the race tank early (Main Menu, restart...): that's a forfeit.
		LocalFinished(FIN_FORFEIT);
	}

	bool RaceKeyDown(int theKey)
	{
		if (gRace.mPhase != PH_RUNNING || RaceBoard() == nullptr || gRace.mKeeper)
			return false;
		if (theKey >= '1' && theKey < '1' + ATK_COUNT)
		{
			TryAttack(theKey - '1');
			return true;
		}
		return false;
	}

	///////////////////////////////////////////////////////////////////////////
	// Overlay
	///////////////////////////////////////////////////////////////////////////
	static void CenterText(Graphics* g, Font* f, const std::string& s, int y, const Color& c)
	{
		if (f == nullptr)
			return;
		g->SetFont(f);
		g->SetColor(c);
		g->DrawString(s, 320 - f->StringWidth(s) / 2, y);
	}

	static void Banner(Graphics* g, const std::string& s, int y, const Color& c)
	{
		Font* f = FONT_JUNGLEFEVER15OUTLINE ? FONT_JUNGLEFEVER15OUTLINE : FONT_JUNGLEFEVER12OUTLINE;
		if (f == nullptr)
			return;
		int w = f->StringWidth(s);
		g->SetColor(Color(0, 0, 0, 160));
		g->FillRect(320 - w / 2 - 16, y - f->GetAscent() - 8, w + 32, f->GetHeight() + 16);
		CenterText(g, f, s, y, c);
	}

	static Color DotColor(int theKind)
	{
		switch (theKind)
		{
		case DOT_GUPPY: return Color(120, 255, 120);
		case DOT_BREEDER: return Color(255, 150, 220);
		case DOT_CARNIVORE: return Color(255, 150, 40);
		case DOT_ULTRA: return Color(255, 70, 70);
		case DOT_PET: return Color(255, 255, 120);
		case DOT_ALIEN: return Color(230, 60, 255);
		default: return Color(170, 200, 255);
		}
	}

	void RaceDrawRivalDots(Graphics* g, int x, int y, int w, int h)
	{
		const RivalInfo& r = gRace.mRival;
		bool anAlert = r.mAliens > 0 && (Now() / 300) % 2 == 0;
		g->SetColor(Color(10, 40, 80, 230));
		g->FillRect(x, y, w, h);
		g->SetColor(anAlert ? Color(255, 60, 60) : Color(90, 140, 190));
		g->DrawRect(x, y, w - 1, h - 1);
		for (const RivalInfo::Dot& d : r.mDots)
		{
			int px = x + 2 + d.x * (w - 5) / 255, py = y + 2 + d.y * (h - 5) / 255;
			g->SetColor(DotColor(d.kind));
			int sz = d.kind == DOT_ALIEN ? 5 : (d.kind == DOT_ULTRA || d.kind == DOT_CARNIVORE ? 4 : 3);
			g->FillRect(px - sz / 2, py - sz / 2, sz, sz);
		}
	}

	static Color Faded(Color c, int thePercent)
	{
		c.mAlpha = c.mAlpha * thePercent / 100;
		return c;
	}

	// The rival's name, money, egg pieces and fish count, in the empty corner right of
	// the attack bar so nothing covers the tank.
	static void DrawRivalStrip(Graphics* g)
	{
		Session& s = S();
		Font* aFont = FONT_JUNGLEFEVER10OUTLINE;
		if (aFont == nullptr)
			return;
		const RivalInfo& r = gRace.mRival;
		const int aX = kStripX, aY = kBarY, aW = 640 - 3 - kStripX, aH = 30;
		bool anAlert = r.mAliens > 0 && (Now() / 300) % 2 == 0;
		g->SetColor(Color(0, 0, 0, 165));
		g->FillRect(aX, aY, aW, aH);
		g->SetColor(anAlert ? Color(255, 60, 60) : Color(90, 140, 190));
		g->DrawRect(aX, aY, aW - 1, aH - 1);
		g->SetFont(aFont);
		std::string aMoney = r.mKnown ? Money(r.mMoney) : "...";
		int aMoneyW = aFont->StringWidth(aMoney);
		std::string aName = s.PlayerName(Them());
		while (aName.size() > 1 && aFont->StringWidth(aName) > aW - 12 - aMoneyW)
			aName.pop_back();
		g->SetColor(s.PlayerColor(Them()));
		g->DrawString(aName, aX + 4, aY + 2 + aFont->GetAscent());
		g->SetColor(Color(255, 255, 255));
		g->DrawString(aMoney, aX + aW - 4 - aMoneyW, aY + 2 + aFont->GetAscent());
		int aRow2 = aY + 17;
		Color c = s.PlayerColor(Them());
		for (int i = 0; i < 3; i++)
		{
			g->SetColor(Color(0, 0, 0, 200));
			g->FillRect(aX + 4 + i * 12, aRow2, 10, 10);
			g->SetColor(i < r.mEggs ? c : Color(90, 90, 100));
			g->FillRect(aX + 5 + i * 12, aRow2 + 1, 8, 8);
		}
		char aBuf[32];
		snprintf(aBuf, sizeof(aBuf), "%d fish", r.mFish);
		g->SetColor(Color(200, 230, 255));
		g->DrawString(aBuf, aX + aW - 4 - aFont->StringWidth(aBuf), aRow2 + aFont->GetAscent() - 2);
	}

	// Live mini-map of the rival's tank, top right: very faint so it doesn't hide your
	// own tank, and clear while Tab is held.
	static void DrawRivalMap(Graphics* g)
	{
		Font* aFont = FONT_JUNGLEFEVER10OUTLINE;
		if (aFont == nullptr)
			return;
		const RivalInfo& r = gRace.mRival;
		bool aHeld = App()->mWidgetManager->mKeyDown[KEYCODE_TAB];
		int aPct = aHeld ? 100 : 22;
		const int mx = 489, my = 66, mw = 138, mh = 86;
		bool anAlert = r.mAliens > 0 && (Now() / 300) % 2 == 0;
		g->SetColor(Faded(Color(10, 40, 80, 220), aPct));
		g->FillRect(mx, my, mw, mh);
		g->SetColor(Faded(anAlert ? Color(255, 60, 60) : Color(90, 140, 190), aPct));
		g->DrawRect(mx, my, mw - 1, mh - 1);
		for (const RivalInfo::Dot& d : r.mDots)
		{
			int px = mx + 2 + d.x * (mw - 5) / 255, py = my + 2 + d.y * (mh - 5) / 255;
			g->SetColor(Faded(DotColor(d.kind), aHeld ? 100 : 40));
			int sz = d.kind == DOT_ALIEN ? 5 : (d.kind == DOT_ULTRA || d.kind == DOT_CARNIVORE ? 4 : 3);
			g->FillRect(px - sz / 2, py - sz / 2, sz, sz);
		}
		std::string aStatus;
		if (!r.mKnown || (r.mFlags & RF_SETUP))
			aStatus = "getting ready";
		else if (r.mAliens > 0)
			aStatus = "ALIEN ATTACK!";
		else if (r.mFlags & RF_THIEF)
			aStatus = "thief!";
		else if (r.mFlags & RF_MURK)
			aStatus = "murky";
		else if (r.mFlags & RF_HIKE)
			aStatus = "price hike";
		else if (r.mFlags & RF_BLOCK)
			aStatus = "coins blocked";
		else if (r.mFlags & RF_PAUSED)
			aStatus = "paused";
		g->SetFont(aFont);
		if (aHeld)
		{
			g->SetColor(S().PlayerColor(Them()));
			g->DrawString(S().PlayerName(Them()) + "'s tank", mx + 3, my + 2 + aFont->GetAscent());
		}
		if (!aStatus.empty())
		{
			g->SetColor(Faded(r.mAliens > 0 ? Color(255, 90, 90) : Color(255, 230, 120), aHeld ? 100 : 45));
			g->DrawString(aStatus, mx + 3, my + mh - 4);
		}
	}

	static void DrawRivalPanel(Graphics* g)
	{
		DrawRivalStrip(g);
		DrawRivalMap(g);
	}

	void DrawRaceOverlay(Graphics* g)
	{
		if (gRace.mPhase == PH_IDLE)
			return;
		Session& s = S();
		Board* b = RaceBoard();
		bool aDialogUp = !App()->mDialogList.empty();
		uint32_t aNow = Now();

		if (gRace.mPhase == PH_SETUP)
		{
			// Usually the pets screen; keep it short.
			Font* f = FONT_JUNGLEFEVER12OUTLINE;
			if (f != nullptr)
			{
				std::string aMsg = gRace.mKeeper
					? std::string("Alien Keeper: ") + (RaceIAmFishKeeper() ? "you keep the fish, " + s.PlayerName(Them()) + " runs the lair" : "you run the lair, " + s.PlayerName(Them()) + " keeps the fish")
					: "Tank Race vs " + s.PlayerName(Them()) + ": Tank " + std::to_string(gRace.mTank) + "-" + std::to_string(gRace.mLevel);
				int w = f->StringWidth(aMsg);
				// On the pets screen the top holds its title: use the bottom edge instead.
				int y = App()->mPetsScreen != nullptr ? 480 - f->GetHeight() - 8 : 4;
				g->SetColor(Color(0, 0, 0, 180));
				g->FillRect(320 - w / 2 - 10, y, w + 20, f->GetHeight() + 8);
				CenterText(g, f, aMsg, y + 4 + f->GetAscent(), Color(255, 220, 90));
			}
			return;
		}
		if (gRace.mKeeper)
			KeeperDrawShared(g);
		else
		{
		if (b == nullptr && gRace.mPhase != PH_RESULT && gRace.mPhase != PH_FINISHED)
			return;

		// Murky water (under everything else we draw).
		if (b != nullptr && !Elapsed(gRace.mMurkUntil) && gRace.mPhase == PH_RUNNING)
		{
			int aIn = (int)(aNow - gRace.mMurkStart), aOut = (int)(gRace.mMurkUntil - aNow);
			int a = std::min(225, std::min(aIn, aOut) * 225 / 600);
			// The whole visible water, from under the top bar down to the attack bar.
			const int aMurkY = kTankY, aMurkH = kBarY - kTankY;
			g->SetColor(Color(34, 48, 22, std::max(0, a)));
			g->FillRect(0, aMurkY, 640, aMurkH);
			g->SetColor(Color(60, 80, 35, std::max(0, a) / 2));
			for (int i = 0; i < 7; i++)
			{
				int yy = aMurkY + 30 + i * 52 + (int)(10 * std::sin(aNow / 700.0 + i));
				g->FillRect(0, yy, 640, 14);
			}
			Font* f = FONT_JUNGLEFEVER10OUTLINE;
			if (f != nullptr)
				CenterText(g, f, "MURKY WATER! " + std::to_string(aOut / 1000 + 1), kTankY + kTankH - 10, Color(200, 230, 150));
		}

		if (!aDialogUp || gRace.mPhase == PH_RESULT)
			DrawRivalPanel(g);

		if (b != nullptr && !Elapsed(gRace.mHikeUntil))
		{
			g->SetColor(Color(255, 40, 40, 55 + (int)(30 * std::sin(aNow / 150.0))));
			g->FillRect(0, 0, 522, 60);
			Font* f = FONT_JUNGLEFEVER12OUTLINE;
			if (f != nullptr)
				CenterText(g, f, "PRICE HIKE! +30% for " + std::to_string((gRace.mHikeUntil - aNow) / 1000 + 1) + "s", 76, Color(255, 120, 100));
		}

		// Coin blocker: a red "no" sign over every coin, and a countdown.
		if (b != nullptr && RaceCoinsBlocked())
		{
			g->SetColor(Color(230, 40, 40, 210));
			for (Coin* aCoin : *b->mCoinList)
			{
				if (aCoin == nullptr || aCoin->m0x198)
					continue;
				int cx = aCoin->mX + aCoin->mWidth / 2, cy = aCoin->mY + aCoin->mHeight / 2, r = 15;
				for (int t = 0; t < 2; t++)
				{
					int rr = r + t;
					for (int k = 0; k < 24; k++)
					{
						double a0 = k * 6.2832 / 24, a1 = (k + 1) * 6.2832 / 24;
						g->DrawLine(cx + (int)(rr * std::cos(a0)), cy + (int)(rr * std::sin(a0)), cx + (int)(rr * std::cos(a1)), cy + (int)(rr * std::sin(a1)));
					}
					g->DrawLine(cx - 10 + t, cy - 10, cx + 10 + t, cy + 10);
				}
			}
			Font* f = FONT_JUNGLEFEVER10OUTLINE;
			if (f != nullptr)
				CenterText(g, f, "COINS BLOCKED! " + std::to_string((gRace.mBlockUntil - aNow) / 1000 + 1), kTankY + kTankH - 28, Color(255, 120, 100));
		}
		}

		switch (gRace.mPhase)
		{
		case PH_READY:
			Banner(g, gRace.mPeerReady ? "Get ready..." : "Waiting for " + s.PlayerName(Them()) + "...", 250, Color(255, 240, 120));
			break;
		case PH_COUNTDOWN:
		{
			int aLeft = kCountdownMs - (int)(aNow - gRace.mCountdownAt);
			std::string aNum = std::to_string(std::max(1, aLeft / 1000 + 1));
			Font* f = FONT_JUNGLEFEVER17OUTLINE ? FONT_JUNGLEFEVER17OUTLINE : FONT_JUNGLEFEVER15OUTLINE;
			Banner(g, gRace.mKeeper ? (RaceIAmFishKeeper() ? "KEEP YOUR FISH ALIVE!" : "INVADE " + s.PlayerName(Them()) + "'S TANK!") : "RACE vs " + s.PlayerName(Them()), 200, s.PlayerColor(Them()));
			if (f != nullptr)
			{
				g->SetFont(f);
				g->SetColor(Color(0, 0, 0, 170));
				g->FillRect(290, 222, 60, 50);
				CenterText(g, f, aNum, 222 + 25 + f->GetAscent() / 2, Color(255, 240, 80));
			}
			break;
		}
		case PH_RUNNING:
			if (aNow - gRace.mStartMs < 900)
				Banner(g, "GO!", 250, Color(120, 255, 120));
			break;
		case PH_FINISHED:
			Banner(g, "Checking the finish line...", 250, Color(255, 240, 120));
			break;
		case PH_RESULT:
		{
			bool aWon = gRace.mWinner == Me();
			const int bx = 150, by = 150, bw = 340, bh = 170;
			g->SetColor(Color(0, 0, 0, 200));
			g->FillRect(bx, by, bw, bh);
			g->SetColor(s.PlayerColor(gRace.mWinner));
			g->DrawRect(bx, by, bw - 1, bh - 1);
			g->DrawRect(bx + 1, by + 1, bw - 3, bh - 3);
			Font* big = FONT_JUNGLEFEVER17OUTLINE ? FONT_JUNGLEFEVER17OUTLINE : FONT_JUNGLEFEVER15OUTLINE;
			Font* mid = FONT_JUNGLEFEVER12OUTLINE;
			Font* small = FONT_JUNGLEFEVER10OUTLINE;
			std::string aTitle = aWon ? "YOU WIN!" : s.PlayerName(gRace.mWinner) + " WINS!";
			CenterText(g, big, aTitle, by + 40, s.PlayerColor(gRace.mWinner));
			std::string aFinisher = s.PlayerName(gRace.mFinisher);
			std::string aWhy;
			if (gRace.mReason == FIN_EGG)
				aWhy = aFinisher + " finished the egg in " + Clock(gRace.mRaceMs);
			else if (gRace.mReason == FIN_SURVIVED)
				aWhy = aFinisher + "'s tank held out to the end!";
			else if (gRace.mReason == FIN_LOST)
				aWhy = !gRace.mKeeper ? "All of " + aFinisher + "'s fish died"
					: (gRace.mFinisher == gRace.mFishKeeper ? s.PlayerName(1 - gRace.mFishKeeper) + "'s aliens wiped out the tank!"
						: aFinisher + "'s lair ran out of aliens!");
			else
				aWhy = aFinisher + (gRace.mKeeper ? " gave up the round" : " left the race");
			CenterText(g, mid, aWhy, by + 72, Color(255, 255, 255));
			char aScore[96];
			snprintf(aScore, sizeof(aScore), "%s %d  -  %d %s", s.PlayerName(0).c_str(), gRace.mWins[0], gRace.mWins[1], s.PlayerName(1).c_str());
			CenterText(g, mid, aScore, by + 102, Color(255, 220, 90));
			char aMine[96];
			snprintf(aMine, sizeof(aMine), "You sent %d attack%s and beat %d alien%s", gRace.mAttacksSent, gRace.mAttacksSent == 1 ? "" : "s", gRace.mDefended, gRace.mDefended == 1 ? "" : "s");
			CenterText(g, small, gRace.mKeeper ? KeeperResultLine() : std::string(aMine), by + 126, Color(200, 220, 255));
			int aBack = (kResultMs - (int)(aNow - gRace.mResultAt)) / 1000 + 1;
			CenterText(g, small, (gRace.mKeeper ? "Next round the roles swap. Menu in " : "Back to the menu in ") + std::to_string(std::max(1, aBack)) + "...", by + 152, Color(170, 170, 180));
			break;
		}
		default:
			break;
		}
	}

	void DrawRaceLocalOverlay(Graphics* g)
	{
		if (gRace.mKeeper && gRace.mPhase != PH_IDLE)
			KeeperDrawLocal(g);
	}

	///////////////////////////////////////////////////////////////////////////
	// Test harness
	///////////////////////////////////////////////////////////////////////////
	bool RaceTestAttack(int theAttack)
	{
		return TryAttack(theAttack);
	}

	void RaceTestReceive(int theAttack, int thePrice)
	{
		ReceiveAttack(theAttack, thePrice);
	}

	std::string RaceDebugState()
	{
		char aBuf[512];
		Board* b = RaceBoard();
		std::string aPrices;
		for (int i = 0; i < ATK_COUNT; i++)
			aPrices += (i ? "/" : "") + std::to_string(AttackPrice(i));
		snprintf(aBuf, sizeof(aBuf), "phase=%d race=%u tank=%d-%d money=%d eggs=%d rival(known=%d money=%d eggs=%d fish=%d aliens=%d flags=%d dots=%d) wins=%d-%d thief=%d/%d murk=%d hike=%d block=%d raid=%d egg=%d prices=%s",
			(int)gRace.mPhase, gRace.mRaceId, b ? b->mTank : 0, b ? b->mLevel : 0, b ? b->mMoney : 0, MyEggs(),
			gRace.mRival.mKnown ? 1 : 0, gRace.mRival.mMoney, gRace.mRival.mEggs, gRace.mRival.mFish, gRace.mRival.mAliens, gRace.mRival.mFlags, (int)gRace.mRival.mDots.size(),
			gRace.mWins[0], gRace.mWins[1], gRace.mThief != nullptr ? 1 : 0, gRace.mThief != nullptr ? gRace.mThief->mEaten : -1, Elapsed(gRace.mMurkUntil) ? 0 : 1, Elapsed(gRace.mHikeUntil) ? 0 : 1, Elapsed(gRace.mBlockUntil) ? 0 : 1, gRace.mRaidLeft, gRace.mEggPrice,
			aPrices.c_str());
		return aBuf;
	}
}
