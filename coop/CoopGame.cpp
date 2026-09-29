#include <SexyAppFramework/Font.h>
#include "CoopGame.h"
#include "CoopSession.h"

#include "WinFishApp.h"
#include "Board.h"
#include "Alien.h"
#include "Coin.h"
#include "Fish.h"
#include "Oscar.h"
#include "Ultra.h"
#include "Gekko.h"
#include "Penta.h"
#include "Grubber.h"
#include "Breeder.h"
#include "OtherTypePet.h"
#include "FishTypePet.h"
#include "MyLabelWidget.h"
#include "MessageWidget.h"
#include "Res.h"
#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/CoopHooks.h>

#include <cmath>
#include <cstdio>
#include <algorithm>

using namespace Sexy;

namespace Coop
{
	double AlienHealthMult()
	{
		return S().ActiveTuning().mAlienHealth;
	}

	int ScaleAlienDelay(int theTicks)
	{
		// Keep the timer above the 276-tick warning window the board relies on.
		int aScaled = (int)std::lround(theTicks * S().ActiveTuning().mAlienDelay);
		return std::max(aScaled, 600);
	}

	int HungerExtraPeriod()
	{
		float aRate = S().ActiveTuning().mHungerRate;
		if (aRate <= 1.001f)
			return 0;
		return std::max(1, (int)std::lround(1.0 / (aRate - 1.0)));
	}

	void ApplyLevelPrices(Board* theBoard)
	{
		float aMult = S().ActiveTuning().mEggPrice;
		if (aMult == 1.0f || theBoard->mApp->mGameMode == GAMEMODE_VIRTUAL_TANK || theBoard->mApp->mGameMode == GAMEMODE_SANDBOX)
			return;
		int& aPrice = theBoard->mSlotPrices[SLOT_EGG];
		aPrice = (int)(std::lround(aPrice * aMult / 25.0) * 25);
	}

	void MaybeSpawnBonusAlien(Board* theBoard)
	{
		Session& s = S();
		if (!s.CoopActive())
			return;
		const Tuning& t = s.ActiveTuning();
		int anExpect = theBoard->mAlienExpect;
		// Bosses, the finale and scripted double waves stay as designed.
		if (anExpect <= ALIEN_NONE || anExpect == ALIEN_BILATERUS || anExpect >= 9 || theBoard->mTank == 5 || theBoard->mCyraxPtr != nullptr)
			return;
		if (theBoard->mApp->mRelaxMode || theBoard->IsTankAndLevelNB(1, 2))
			return;
		if ((int)(theBoard->mApp->mSeed->Next() % 100) >= t.mBonusAlienPct)
			return;
		int aBonus = (anExpect == ALIEN_STRONG_SYLV) ? ALIEN_STRONG_SYLV : ALIEN_WEAK_SYLV;
		theBoard->mCrosshair2X = theBoard->mApp->mSeed->Next() % 450 + 20;
		theBoard->mCrosshair2Y = theBoard->mApp->mSeed->Next() % 195 + 105;
		theBoard->SpawnAlien(aBonus, theBoard->mCrosshair2X, theBoard->mCrosshair2Y, false);
		s.AddToast("DOUBLE TROUBLE! A second alien crashed the party!");
		s.Log("Game: bonus alien %d alongside %d", aBonus, anExpect);
	}

	///////////////////////////////////////////////////////////////////////////
	// Coin Rivals state
	///////////////////////////////////////////////////////////////////////////
	static const int	kCoinLockMs = 1500;		// head start on coins from your own fish

	struct RivalsState
	{
		Board*	mLevelBoard = nullptr;	// board of the level in progress (any mode)
		bool	mLevelLive = false;
		int		mStartMoney = 200;
		int		mStartUpdate = 0;

		bool	mRunning = false;
		int		mActor = 0;
		int		mScopeDepth = 0;
		int		mWallet[2] = { 0, 0 };
		int		mEggs[2] = { 1, 1 };		// the board's counter: 1 = no pieces, 4 = done
		int		mBrokeUntil[2] = { 0, 0 };
	};
	static RivalsState gR;

	static WinFishApp* App() { return (WinFishApp*)gSexyAppBase; }
	// Coins age once per game update, every mFrameTime ms (28), not 100 times a second.
	static int CoinLockTicks() { return kCoinLockMs / std::max(1, App()->mFrameTime); }

	static bool Live()
	{
		return gR.mRunning && gR.mLevelBoard != nullptr && App() != nullptr && App()->mBoard == gR.mLevelBoard;
	}

	bool RivalsActive()
	{
		return Live() && S().HasGuest();
	}

	static bool Eligible(Board* theBoard)
	{
		if (theBoard == nullptr || S().GetMode() != MODE_RIVALS || !S().HasGuest())
			return false;
		int aMode = theBoard->mApp->mGameMode;
		if (aMode != GAMEMODE_ADVENTURE && aMode != GAMEMODE_TIME_TRIAL)	// Challenge and Virtual Tank: co-op only
			return false;
		return !theBoard->mIsBonusRound && theBoard->mTank != 5;
	}

	static bool EggRace(Board* theBoard)
	{
		return theBoard->mApp->mGameMode != GAMEMODE_TIME_TRIAL;
	}

	static void SetActorRaw(int thePlayer)
	{
		thePlayer &= 1;
		if (!Live() || thePlayer == gR.mActor)
			return;
		Board* b = gR.mLevelBoard;
		gR.mWallet[gR.mActor] = b->mMoney;
		gR.mEggs[gR.mActor] = b->m0x43c;
		b->mMoney = gR.mWallet[thePlayer];
		b->m0x43c = gR.mEggs[thePlayer];
		gR.mActor = thePlayer;
	}

	int RivalsActor()
	{
		return Live() ? gR.mActor : 0;
	}

	int RivalsWallet(int thePlayer)
	{
		thePlayer &= 1;
		if (!Live())
			return 0;
		return thePlayer == gR.mActor ? gR.mLevelBoard->mMoney : gR.mWallet[thePlayer];
	}

	int RivalsEggPieces(int thePlayer)
	{
		thePlayer &= 1;
		if (!Live())
			return 0;
		int aCount = thePlayer == gR.mActor ? gR.mLevelBoard->m0x43c : gR.mEggs[thePlayer];
		return std::clamp(aCount - 1, 0, 3);
	}

	RivalsScope::RivalsScope(int thePlayer)
	{
		mOn = RivalsActive();
		if (!mOn)
			return;
		mPrev = gR.mActor;
		SetActorRaw(thePlayer);
		gR.mScopeDepth++;
	}

	RivalsScope::~RivalsScope()
	{
		if (!mOn)
			return;
		if (gR.mScopeDepth > 0)
			gR.mScopeDepth--;
		if (Live())
			SetActorRaw(mPrev);
	}

	static void ShowMoneyLabel(Board* theBoard, bool theShow)
	{
		if (theBoard != nullptr && theBoard->mMoneyLabel != nullptr && theBoard->mMoneyLabel->mVisible != theShow)
			theBoard->mMoneyLabel->SetVisible(theShow);
		if (theBoard != nullptr)
			theBoard->UpdateMoneyLabelText();
	}

	static void StartRivals(Board* theBoard, bool theMidLevel)
	{
		gR.mRunning = true;
		gR.mActor = 0;
		gR.mScopeDepth = 0;
		gR.mBrokeUntil[0] = gR.mBrokeUntil[1] = 0;
		gR.mWallet[0] = theBoard->mMoney;
		gR.mEggs[0] = theBoard->m0x43c;
		gR.mWallet[1] = theMidLevel ? gR.mStartMoney : theBoard->mMoney;
		gR.mEggs[1] = theMidLevel ? 1 : theBoard->m0x43c;
		// Eggs keep their normal price: a Coin Rivals win completes the Adventure level
		// for real, so it's never an easier way through than playing alone.
		ShowMoneyLabel(theBoard, false);
		Session& s = S();
		s.AddToast("COIN RIVALS! Separate wallets, every coin for yourself.", -1, 600);
		s.AddToast(EggRace(theBoard) ? "First to buy all 3 egg pieces wins!" : "Richest player when time runs out wins!", -1, 600);
		s.Log("Rivals: started (%s), wallets %d / %d, egg price %d", theMidLevel ? "mid-level" : "level start", gR.mWallet[0], gR.mWallet[1], theBoard->mSlotPrices[SLOT_EGG]);
	}

	// theKeepBoardState: leave the board holding whoever is active now (the
	// winner mid-purchase); otherwise hand the board back to the host's wallet.
	static void StopRivals(bool theKeepBoardState)
	{
		if (!gR.mRunning)
			return;
		Board* b = Live() ? gR.mLevelBoard : nullptr;
		if (b != nullptr && !theKeepBoardState)
			SetActorRaw(0);
		gR.mRunning = false;
		gR.mActor = 0;
		gR.mScopeDepth = 0;
		ShowMoneyLabel(b, true);
		S().Log("Rivals: stopped");
	}

	int RivalsSpawnOwner()
	{
		if (!RivalsActive())
			return -1;
		// The starting fish of a level belong to nobody.
		if (gR.mLevelBoard->mGameUpdateCnt <= gR.mStartUpdate + 1)
			return -1;
		Widget* w = gUpdatingWidget;
		if (w != nullptr)
		{
			if (GameObject* anObj = dynamic_cast<GameObject*>(w))
				return anObj->mCoopOwner;		// coins, babies... inherit their parent's owner
		}
		// Player input (or a player's held button) buys, feeds and shoots for that player.
		if (w == nullptr || gR.mScopeDepth > 0)
			return gR.mActor;
		return -1;	// the board itself: aliens, bonus drops
	}

	static void Credit(Board* theBoard, int thePlayer, int theValue)
	{
		if (thePlayer < 0)
		{
			int aHalf = theValue / 2;
			{
				RivalsScope a(0);
				theBoard->Unk07(aHalf);
			}
			RivalsScope b(1);
			theBoard->Unk07(theValue - aHalf);
			return;
		}
		RivalsScope a(thePlayer);
		theBoard->Unk07(theValue);
	}

	bool RivalsReceive(Board* theBoard, GameObject* theFrom, int theValue)
	{
		if (!RivalsActive() || theBoard != gR.mLevelBoard || theBoard->mIsBonusRound)
			return false;
		int aPlayer = theFrom->mCoopCollector >= 0 ? theFrom->mCoopCollector : theFrom->mCoopOwner;
		Credit(theBoard, aPlayer, theValue);
		return true;
	}

	bool RivalsCountsInFlight(GameObject* theObj)
	{
		return !RivalsActive() || theObj->mCoopCollector == gR.mActor;
	}

	void RivalsFlushInFlight(Board* theBoard)
	{
		for (int p = 0; p < 2; p++)
		{
			RivalsScope aScope(p);
			theBoard->mMoney = std::clamp(theBoard->mMoney + theBoard->Unk10(), 0, 9999999);
		}
		theBoard->UpdateMoneyLabelText();
	}

	bool RivalsCoinLocked(GameObject* theCoin, int thePlayer)
	{
		return RivalsActive() && theCoin->mCoopOwner >= 0 && theCoin->mCoopOwner != thePlayer && theCoin->mUpdateCnt < CoinLockTicks();
	}

	void RivalsOnAlienShot(GameObject* theAlien, int thePlayer)
	{
		// The treasure an alien drops belongs to whoever shot it last.
		if (RivalsActive())
			theAlien->mCoopOwner = thePlayer & 1;
	}

	void RivalsBuyFailed()
	{
		if (RivalsActive())
			gR.mBrokeUntil[gR.mActor] = gR.mLevelBoard->mGameUpdateCnt + 70;
	}

	void RivalsGuestJoined()
	{
		WinFishApp* anApp = App();
		if (anApp == nullptr || gR.mRunning || !gR.mLevelLive || anApp->mBoard != gR.mLevelBoard)
			return;
		if (Eligible(gR.mLevelBoard))
			StartRivals(gR.mLevelBoard, true);
	}

	void RivalsGuestLeft()
	{
		if (!gR.mRunning)
			return;
		StopRivals(false);
		S().AddToast("Coin Rivals is over - you keep your own wallet.", 0, 500);
	}

	void OnLevelStarted(Board* theBoard)
	{
		S().ResetLevelStats();
		StopRivals(false);
		gR.mLevelBoard = theBoard;
		gR.mLevelLive = true;
		gR.mStartMoney = theBoard->mMoney;
		gR.mStartUpdate = theBoard->mGameUpdateCnt;
		if (Eligible(theBoard))
			StartRivals(theBoard, false);
	}

	void OnBoardDestroyed(Board* theBoard)
	{
		if (gR.mLevelBoard != theBoard)
			return;
		gR.mRunning = false;
		gR.mActor = 0;
		gR.mScopeDepth = 0;
		gR.mLevelLive = false;
		gR.mLevelBoard = nullptr;
	}

	static std::string Money(int v)
	{
		// The game fonts have no '$' glyph; spell it out.
		char aBuf[32];
		if (v >= 1000000)
			snprintf(aBuf, sizeof(aBuf), "%d,%03d,%03d", v / 1000000, v / 1000 % 1000, v % 1000);
		else if (v >= 1000)
			snprintf(aBuf, sizeof(aBuf), "%d,%03d", v / 1000, v % 1000);
		else
			snprintf(aBuf, sizeof(aBuf), "%d", v);
		return aBuf;
	}

	static void RivalsReport(const std::string& theHeadline, int theWinner)
	{
		Session& s = S();
		s.AddToast(theHeadline, theWinner, 1000);
		for (int p = 0; p < 2; p++)
		{
			PlayerStats& st = s.Stats(p);
			char aBuf[160];
			snprintf(aBuf, sizeof(aBuf), "%s: %d egg piece%s, %s in the bank, %d coin%s grabbed",
				s.PlayerName(p).c_str(), RivalsEggPieces(p), RivalsEggPieces(p) == 1 ? "" : "s", Money(RivalsWallet(p)).c_str(), st.mCoins, st.mCoins == 1 ? "" : "s");
			s.AddToast(aBuf, p, 1000);
		}
		s.Log("Rivals: %s | P1 eggs %d wallet %d coins %d | P2 eggs %d wallet %d coins %d", theHeadline.c_str(),
			RivalsEggPieces(0), RivalsWallet(0), s.Stats(0).mCoins, RivalsEggPieces(1), RivalsWallet(1), s.Stats(1).mCoins);
	}

	void OnLevelWon(Board* theBoard)
	{
		Session& s = S();
		gR.mLevelLive = false;
		if (RivalsActive())
		{
			int aWinner = gR.mActor;
			RivalsReport(s.PlayerName(aWinner) + " WINS COIN RIVALS!", aWinner);
			StopRivals(true);	// the board keeps the winner's egg so the level ends normally
			return;
		}
		if (!s.CoopActive())
			return;
		PlayerStats& a = s.Stats(0);
		PlayerStats& b = s.Stats(1);
		int aTotal = std::max(1, a.mMoney + b.mMoney);
		for (int p = 0; p < 2; p++)
		{
			PlayerStats& st = s.Stats(p);
			char aBuf[160];
			snprintf(aBuf, sizeof(aBuf), "%s: %s in coins (%d%%), %d alien hits, %d feedings",
				s.PlayerName(p).c_str(), Money(st.mMoney).c_str(), st.mMoney * 100 / aTotal, st.mAlienHits, st.mFoodDropped);
			s.AddToast(aBuf, p, 900);
		}
		int aScoreA = a.mMoney + a.mAlienHits * 20 + a.mFoodDropped * 5;
		int aScoreB = b.mMoney + b.mAlienHits * 20 + b.mFoodDropped * 5;
		if (aScoreA != aScoreB)
			s.AddToast("Tank MVP: " + s.PlayerName(aScoreA > aScoreB ? 0 : 1) + "!", aScoreA > aScoreB ? 0 : 1, 900);
		else
			s.AddToast("Perfect teamwork - it's a tie!", -1, 900);
		s.Log("Game: level won. P1 $%d hits %d food %d | P2 $%d hits %d food %d", a.mMoney, a.mAlienHits, a.mFoodDropped, b.mMoney, b.mAlienHits, b.mFoodDropped);
	}

	void OnTimeUp(Board* theBoard)
	{
		gR.mLevelLive = false;
		if (!RivalsActive())
			return;
		int a = RivalsWallet(0), b = RivalsWallet(1);
		Session& s = S();
		if (a == b)
			RivalsReport("Time's up - a dead heat!", -1);
		else
			RivalsReport("Time's up - " + s.PlayerName(a > b ? 0 : 1) + " WINS COIN RIVALS!", a > b ? 0 : 1);
		StopRivals(false);	// the host's own wallet goes on the high score table
	}

	void OnTankLost(Board* theBoard)
	{
		gR.mLevelLive = false;
		if (!RivalsActive())
			return;
		int a = RivalsWallet(0), b = RivalsWallet(1);
		std::string aRicher = a == b ? "" : " (" + S().PlayerName(a > b ? 0 : 1) + " was richer)";
		RivalsReport("The tank is empty - nobody wins!" + aRicher, -1);
		StopRivals(false);
	}

	///////////////////////////////////////////////////////////////////////////
	// Rivals overlay: wallets, egg progress, fish tags and coin locks.
	///////////////////////////////////////////////////////////////////////////
	template <class T>
	static void DrawTags(Graphics* g, std::vector<T*>* theList)
	{
		if (theList == nullptr)
			return;
		for (T* anObj : *theList)
		{
			if (anObj == nullptr || anObj->mCoopOwner < 0 || !anObj->mVisible || anObj->mInvisible)
				continue;
			Color c = S().PlayerColor(anObj->mCoopOwner);
			int cx = anObj->mX + anObj->mWidth / 2;
			int cy = anObj->mY + anObj->mHeight / 2 + 20;
			g->SetColor(Color(0, 0, 0, 170));
			g->FillRect(cx - 4, cy - 4, 9, 9);
			g->SetColor(c);
			g->FillRect(cx - 3, cy - 3, 7, 7);
		}
	}

	static void DrawCoinLock(Graphics* g, GameObject* theCoin)
	{
		Color c = S().PlayerColor(theCoin->mCoopOwner);
		int aLeft = CoinLockTicks() - theCoin->mUpdateCnt;
		c.mAlpha = std::clamp(80 + aLeft * 175 / CoinLockTicks(), 80, 255);
		g->SetColor(c);
		int cx = theCoin->mX + theCoin->mWidth / 2, cy = theCoin->mY + theCoin->mHeight / 2;
		int r = 17, l = 7;
		for (int t = 0; t < 2; t++)
		{
			int rr = r + t;
			// four corner brackets
			g->DrawLine(cx - rr, cy - rr, cx - rr + l, cy - rr);
			g->DrawLine(cx - rr, cy - rr, cx - rr, cy - rr + l);
			g->DrawLine(cx + rr, cy - rr, cx + rr - l, cy - rr);
			g->DrawLine(cx + rr, cy - rr, cx + rr, cy - rr + l);
			g->DrawLine(cx - rr, cy + rr, cx - rr + l, cy + rr);
			g->DrawLine(cx - rr, cy + rr, cx - rr, cy + rr - l);
			g->DrawLine(cx + rr, cy + rr, cx + rr - l, cy + rr);
			g->DrawLine(cx + rr, cy + rr, cx + rr, cy + rr - l);
		}
	}

	void DrawRivalsOverlay(Graphics* g)
	{
		if (!RivalsActive())
			return;
		WinFishApp* anApp = App();
		Board* b = gR.mLevelBoard;
		if (b->mMoneyLabel != nullptr && b->mMoneyLabel->mVisible)
			b->mMoneyLabel->SetVisible(false);
		bool aDialogUp = !anApp->mDialogList.empty();

		if (!aDialogUp)
		{
			DrawTags(g, b->mFishList);
			DrawTags(g, b->mOscarList);
			DrawTags(g, b->mUltraList);
			DrawTags(g, b->mGekkoList);
			DrawTags(g, b->mPentaList);
			DrawTags(g, b->mGrubberList);
			DrawTags(g, b->mBreederList);
			DrawTags(g, b->mOtherTypePetList);
			DrawTags(g, b->mFishTypePetList);
			for (Coin* aCoin : *b->mCoinList)
			{
				if (aCoin != nullptr && aCoin->mVisible && !aCoin->m0x198 && aCoin->mCoopOwner >= 0 && aCoin->mUpdateCnt < CoinLockTicks())
					DrawCoinLock(g, aCoin);
			}
		}

		// Wallet panel, top right of the tank.
		Font* aFont = FONT_JUNGLEFEVER10OUTLINE;
		if (aFont == nullptr)
			return;
		Session& s = S();
		const int aW = 196, aRowH = 18, aX = 632 - aW, aY = 64;
		g->SetColor(Color(0, 0, 0, 150));
		g->FillRect(aX, aY, aW, aRowH * 2 + 8);
		g->SetFont(aFont);
		int aLeader = RivalsEggPieces(0) != RivalsEggPieces(1) ? (RivalsEggPieces(0) > RivalsEggPieces(1) ? 0 : 1)
			: (RivalsWallet(0) != RivalsWallet(1) ? (RivalsWallet(0) > RivalsWallet(1) ? 0 : 1) : -1);
		for (int p = 0; p < 2; p++)
		{
			Color c = s.PlayerColor(p);
			int y = aY + 4 + p * aRowH;
			int aBase = y + aFont->GetAscent() + 1;
			std::string aName = s.PlayerName(p);
			if (aName.size() > 10)
				aName = aName.substr(0, 10);
			g->SetColor(c);
			if (p == aLeader)
				g->FillRect(aX + 3, y + 2, 3, aRowH - 3);	// leader bar
			g->DrawString(aName, aX + 10, aBase);
			bool aBroke = b->mGameUpdateCnt < gR.mBrokeUntil[p] && (b->mGameUpdateCnt / 8) % 2 == 0;
			std::string aMoney = Money(RivalsWallet(p));
			g->SetColor(aBroke ? Color(255, 70, 50) : Color(255, 255, 255));
			g->DrawString(aMoney, aX + aW - 50 - aFont->StringWidth(aMoney), aBase);
			if (EggRace(b))
			{
				int aPieces = RivalsEggPieces(p);
				for (int i = 0; i < 3; i++)
				{
					int px = aX + aW - 42 + i * 13, py = y + 3;
					g->SetColor(Color(0, 0, 0, 200));
					g->FillRect(px, py, 10, 11);
					g->SetColor(i < aPieces ? c : Color(90, 90, 100));
					g->FillRect(px + 1, py + 1, 8, 9);
				}
			}
		}
	}

	void OnCoinCollected(int theValue)
	{
		Session& s = S();
		PlayerStats& st = s.Stats(s.CurrentPlayer());
		st.mCoins++;
		st.mMoney += theValue;
	}
}
