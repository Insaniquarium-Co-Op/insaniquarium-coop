// Insaniquarium Co-op - scripted input + screenshots for automated testing.
//
// INSANIQ_TESTSCRIPT=<file> runs a script of timed actions, one per line:
//   <ms since start> move|down|up|rdown|rup|click|rclick <x> <y>
//   <ms> key <keycode> | keydown/keyup <keycode> (hold a key) | char <c> | shot <png path> | host | join <addr> | leave | quit | log <text>
//   <ms> record <path prefix> <every ms> | record stop   (numbered PNG frames for a video)
// Times are wall-clock milliseconds from launch. Input goes through SDL's own
// event queue, exactly like a real mouse.

#include "CoopTest.h"
#include <SexyAppFramework/Font.h>
#include "CoopSession.h"
#include "CoopUpdate.h"
#include "WinFishApp.h"
#include "Board.h"
#include "Alien.h"
#include "Coin.h"
#include "Fish.h"
#include "Breeder.h"
#include "PetsScreen.h"
#include "PetButtonWidget.h"
#include "ProfileMgr.h"
#include "Food.h"
#include "CoopGame.h"
#include "CoopRace.h"
#include "CoopKeeper.h"
#include "CoopHeroes.h"
#include "CoopUI.h"
#include <SDL.h>
#include <SexyAppFramework/SexyAppBase.h>
#include <SexyAppFramework/WidgetManager.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Sexy { bool WriteBackbufferPNG(const std::string& thePath); bool ReadBackbuffer(std::vector<uint32_t>& theOut, int& theW, int& theH); }

namespace Coop
{
	struct TestStep
	{
		uint32_t	mAt = 0;
		std::string	mVerb;
		std::string	mArg;
		int			mX = 0, mY = 0;
	};

	static std::vector<TestStep> gSteps;
	static size_t gNext = 0;
	static bool gLoaded = false;
	static uint32_t gStart = 0;
	static std::vector<std::string> gPendingShots;

	// "video <path.mp4> <fps>": frames go straight into an ffmpeg process (PNGs are far too
	// slow at 30 fps); a frame repeats when the game falls behind, so it plays in real time.
	static FILE*		gVideo = nullptr;
	static int			gVideoW = 0, gVideoH = 0, gVideoDue = 0, gVideoFrames = 0, gVideoRepeats = 0;
	static uint32_t		gVideoEvery = 33, gVideoNextAt = 0;
	static std::vector<uint32_t> gVideoBuf;

	static void StopVideo()
	{
		if (gVideo != nullptr)
		{
			pclose(gVideo);
			fprintf(stderr, "[test] video stopped after %d frames (%d repeats)\n", gVideoFrames, gVideoRepeats);
		}
		gVideo = nullptr;
	}
	static std::string gScriptDir;	// relative screenshot paths are relative to the script
	static std::string gRecordPrefix;	// empty: not recording
	static uint32_t gRecordEvery = 0, gRecordNextAt = 0;
	static int gRecordFrame = 0;
	static std::vector<std::string> gPendingFrames;
	static bool gQuitWhenIdle = false;	// quit as soon as no race or round is running

	static std::string ScriptPath(const std::string& thePath)
	{
		bool isAbsolute = !thePath.empty() && (thePath[0] == '/' || thePath[0] == '\\' || (thePath.size() > 1 && thePath[1] == ':'));
		return isAbsolute ? thePath : gScriptDir + thePath;
	}

	static void Load()
	{
		gLoaded = true;
		const char* aPath = getenv("INSANIQ_TESTSCRIPT");
		if (aPath == nullptr)
			return;
		std::ifstream f(aPath);
		std::string aLine;
		while (std::getline(f, aLine))
		{
			if (aLine.empty() || aLine[0] == '#')
				continue;
			std::istringstream ss(aLine);
			TestStep s;
			ss >> s.mAt >> s.mVerb;
			if (s.mVerb == "move" || s.mVerb == "down" || s.mVerb == "up" || s.mVerb == "click" || s.mVerb == "rdown" || s.mVerb == "rup" || s.mVerb == "rclick" || s.mVerb == "mclick")
				ss >> s.mX >> s.mY;
			else
			{
				std::getline(ss, s.mArg);
				while (!s.mArg.empty() && s.mArg[0] == ' ')
					s.mArg.erase(0, 1);
			}
			gSteps.push_back(s);
		}
		gStart = SDL_GetTicks();
		std::string aScript = aPath;
		size_t aSlash = aScript.find_last_of("/\\");
		gScriptDir = aSlash == std::string::npos ? std::string() : aScript.substr(0, aSlash + 1);
		fprintf(stderr, "[test] loaded %d steps from %s\n", (int)gSteps.size(), aPath);
	}

	static void PushMouse(Uint32 theType, int x, int y, int theButton)
	{
		// Script coordinates are game coordinates (640x480); SDL wants window pixels.
		Sexy::WidgetManager* wm = Sexy::gSexyAppBase->mWidgetManager;
		if (wm->mMouseDestRect.mWidth > 0 && wm->mMouseDestRect.mHeight > 0)
		{
			x = (x - wm->mMouseDestRect.mX) * wm->mMouseSourceRect.mWidth / wm->mMouseDestRect.mWidth + wm->mMouseSourceRect.mX;
			y = (y - wm->mMouseDestRect.mY) * wm->mMouseSourceRect.mHeight / wm->mMouseDestRect.mHeight + wm->mMouseSourceRect.mY;
		}
		SDL_Event e;
		memset(&e, 0, sizeof(e));
		e.type = theType;
		if (theType == SDL_MOUSEMOTION)
		{
			e.motion.x = x;
			e.motion.y = y;
		}
		else
		{
			e.button.x = x;
			e.button.y = y;
			e.button.button = (Uint8)theButton;
			e.button.clicks = 1;
			e.button.state = theType == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
		}
		SDL_PushEvent(&e);
	}

	void TestHarnessUpdate()
	{
		if (!gLoaded)
			Load();
		if (gSteps.empty())
			return;
		uint32_t aNow = SDL_GetTicks() - gStart;
		// Headless test displays have no window manager to hand out focus.
		if (!Sexy::gSexyAppBase->mActive)
		{
			Sexy::gSexyAppBase->mActive = true;
			Sexy::gSexyAppBase->mHasFocus = true;
			Sexy::gSexyAppBase->mWidgetManager->GotFocus();
		}
		while (gNext < gSteps.size() && gSteps[gNext].mAt <= aNow)
		{
			TestStep& s = gSteps[gNext++];
			fprintf(stderr, "[test] t=%u %s %s %d %d\n", aNow, s.mVerb.c_str(), s.mArg.c_str(), s.mX, s.mY);
			if (s.mVerb == "move")
				PushMouse(SDL_MOUSEMOTION, s.mX, s.mY, 0);
			else if (s.mVerb == "down")
				PushMouse(SDL_MOUSEBUTTONDOWN, s.mX, s.mY, SDL_BUTTON_LEFT);
			else if (s.mVerb == "up")
				PushMouse(SDL_MOUSEBUTTONUP, s.mX, s.mY, SDL_BUTTON_LEFT);
			else if (s.mVerb == "rdown")
				PushMouse(SDL_MOUSEBUTTONDOWN, s.mX, s.mY, SDL_BUTTON_RIGHT);
			else if (s.mVerb == "rup")
				PushMouse(SDL_MOUSEBUTTONUP, s.mX, s.mY, SDL_BUTTON_RIGHT);
			else if (s.mVerb == "click" || s.mVerb == "rclick" || s.mVerb == "mclick")
			{
				int b = s.mVerb == "click" ? SDL_BUTTON_LEFT : (s.mVerb == "rclick" ? SDL_BUTTON_RIGHT : SDL_BUTTON_MIDDLE);
				PushMouse(SDL_MOUSEMOTION, s.mX, s.mY, 0);
				PushMouse(SDL_MOUSEBUTTONDOWN, s.mX, s.mY, b);
				PushMouse(SDL_MOUSEBUTTONUP, s.mX, s.mY, b);
			}
			else if (s.mVerb == "keydown" || s.mVerb == "keyup")
			{
				SDL_Event e;
				memset(&e, 0, sizeof(e));
				e.type = s.mVerb == "keydown" ? SDL_KEYDOWN : SDL_KEYUP;
				e.key.keysym.sym = (SDL_Keycode)atoi(s.mArg.c_str());
				SDL_PushEvent(&e);
			}
			else if (s.mVerb == "key")
			{
				SDL_Event e;
				memset(&e, 0, sizeof(e));
				e.type = SDL_KEYDOWN;
				e.key.keysym.sym = (SDL_Keycode)atoi(s.mArg.c_str());
				SDL_PushEvent(&e);
				e.type = SDL_KEYUP;
				SDL_PushEvent(&e);
			}
			else if (s.mVerb == "char")
			{
				// sdl2-compat (Homebrew's SDL2 on macOS) can't push SDL_TEXTINPUT; this is what the framework does with one.
				if (!s.mArg.empty())
					Sexy::gSexyAppBase->mWidgetManager->KeyChar(s.mArg[0]);
			}
			else if (s.mVerb == "shot")
			{
				gPendingShots.push_back(ScriptPath(s.mArg));
				Sexy::gSexyAppBase->mWidgetManager->MarkAllDirty();
			}
			else if (s.mVerb == "record")
			{
				std::string aPrefix;
				int anEvery = 200;
				std::istringstream as(s.mArg);
				as >> aPrefix >> anEvery;
				if (aPrefix == "stop")
					gRecordPrefix.clear();
				else
				{
					gRecordPrefix = ScriptPath(aPrefix);
					gRecordEvery = (uint32_t)std::max(anEvery, 20);
					gRecordNextAt = aNow;
					gRecordFrame = 0;
					std::error_code ec;
					std::filesystem::create_directories(std::filesystem::path(gRecordPrefix).parent_path(), ec);
				}
			}
			else if (s.mVerb == "coopwindow")
				OpenCoopDialog((Sexy::WinFishApp*)Sexy::gSexyAppBase);
			else if (s.mVerb == "host")
			{
				std::string anErr;
				if (!S().StartHosting(kDefaultPort, anErr))
					fprintf(stderr, "[test] host failed: %s\n", anErr.c_str());
			}
			else if (s.mVerb == "join")
			{
				std::string anErr;
				if (!S().StartJoin(s.mArg, kDefaultPort, anErr))
					fprintf(stderr, "[test] join failed: %s\n", anErr.c_str());
			}
			else if (s.mVerb == "leave")
			{
				if (S().IsGuest())
					S().Leave("test");
				else
					S().StopHosting();
			}
			else if (s.mVerb == "money" || s.mVerb == "alien" || s.mVerb == "egg" || s.mVerb == "diff" || s.mVerb == "wave" || s.mVerb == "bonus")
			{
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				Sexy::Board* aBoard = anApp->mBoard;
				int anArg = atoi(s.mArg.c_str());
				if (s.mVerb == "diff")
					S().SetDifficulty(anArg);
				else if (aBoard == nullptr)
					fprintf(stderr, "[test] no board for %s\n", s.mVerb.c_str());
				else if (s.mVerb == "money")
				{
					aBoard->mMoney = anArg;
					aBoard->UpdateMoneyLabelText();
				}
				else if (s.mVerb == "alien")
				{
					aBoard->SpawnAlien(anArg, 300, 200, true);
					if (!aBoard->mAlienList->empty())
					{
						Sexy::Alien* a = aBoard->mAlienList->back();
						fprintf(stderr, "[test] alien type %d health %.1f (max %.1f)\n", anArg, a->mHealth, a->mMaxHealth);
					}
				}
				else if (s.mVerb == "bonus")
				{
					int aSaveLevel = aBoard->mLevel;
					aBoard->mLevel = 3;
					aBoard->mAlienExpect = Sexy::ALIEN_STRONG_SYLV;
					size_t aBefore = aBoard->mAlienList->size();
					for (int i = 0; i < anArg; i++)
						Coop::MaybeSpawnBonusAlien(aBoard);
					fprintf(stderr, "[test] bonus: %d rolls -> %d bonus aliens\n", anArg, (int)(aBoard->mAlienList->size() - aBefore));
					aBoard->mLevel = aSaveLevel;
				}
				else if (s.mVerb == "wave")
				{
					// Fast-forward the invasion countdown to its warning phase.
					aBoard->mAlienExpect = anArg > 0 ? anArg : Sexy::ALIEN_WEAK_SYLV;
					aBoard->mAlienTimer = 280;
				}
				else if (s.mVerb == "egg")
				{
					// Buy an egg piece (as player <arg>, default the host).
					RivalsScope aWallet(anArg);
					int aSaveMoney = aBoard->mMoney;
					aBoard->mMoney = 99999;
					aBoard->MakeAndUnlockMenuButton(Sexy::SLOT_EGG, true);
					aBoard->HandleBuyEgg();
					if (aBoard->mMoney < 99999)		// the wallet stays as it was (for videos)
						aBoard->mMoney = aSaveMoney;
					aBoard->UpdateMoneyLabelText();
				}
			}
			else if (s.mVerb == "clickcoin" || s.mVerb == "p2clickcoin" || s.mVerb == "petcollect")
			{
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				if (anApp->mBoard != nullptr && !anApp->mBoard->mCoinList->empty())
				{
					Sexy::Coin* aCoin = anApp->mBoard->mCoinList->back();
					int cx = aCoin->mX + aCoin->mWidth / 2, cy = aCoin->mY + aCoin->mHeight / 2;
					fprintf(stderr, "[test] %s at %d,%d value %d owner %d age %d\n", s.mVerb.c_str(), cx, cy, aCoin->GetValue(), aCoin->mCoopOwner, aCoin->mUpdateCnt);
					if (s.mVerb == "clickcoin")
					{
						PushMouse(SDL_MOUSEMOTION, cx, cy, 0);
						PushMouse(SDL_MOUSEBUTTONDOWN, cx, cy, SDL_BUTTON_LEFT);
						PushMouse(SDL_MOUSEBUTTONUP, cx, cy, SDL_BUTTON_LEFT);
					}
					else if (s.mVerb == "p2clickcoin")
						S().TestGuestClick(cx, cy);
					else
					{
						aCoin->PetCollected();
						aCoin->Remove();
					}
				}
			}
			else if (s.mVerb == "clickpoison")
			{
				// Clicks a falling poison potion (Tank Race poison raid) like a player would.
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				int aLeft = 0;
				Sexy::Food* aTarget = nullptr;
				if (anApp->mBoard != nullptr)
					for (Sexy::Food* f : *anApp->mBoard->mFoodList)
						if (f != nullptr && f->mCoopPoison)
						{
							aLeft++;
							if (aTarget == nullptr)
								aTarget = f;
						}
				fprintf(stderr, "[test] clickpoison: %d poison potions falling\n", aLeft);
				if (aTarget != nullptr)
				{
					int cx = aTarget->mX + 20, cy = aTarget->mY + 15;
					PushMouse(SDL_MOUSEMOTION, cx, cy, 0);
					PushMouse(SDL_MOUSEBUTTONDOWN, cx, cy, SDL_BUTTON_LEFT);
					PushMouse(SDL_MOUSEBUTTONUP, cx, cy, SDL_BUTTON_LEFT);
				}
			}
			else if (s.mVerb == "pickpets")
			{
				// pickpets <a> <b> <c>: choose these pets on the pets screen and carry on.
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				Sexy::PetsScreen* aScreen = anApp->mPetsScreen;
				if (aScreen == nullptr)
					fprintf(stderr, "[test] pickpets: no pets screen\n");
				else
				{
					std::istringstream as(s.mArg);
					int aPet, aCount = 0, aOffered = 0;
					for (int i = 0; i < 24; i++)
						if (!aScreen->mPetButtons[i]->mDisabled)
							aOffered++;
					while (as >> aPet)
						if (aPet >= 0 && aPet < 24 && !aScreen->mPetButtons[aPet]->mDisabled)
						{
							aScreen->mPetButtons[aPet]->m0x130 = true;
							aCount++;
						}
					aScreen->m0x124 = aCount;
					fprintf(stderr, "[test] pickpets: %d offered, picked %d\n", aOffered, aCount);
					aScreen->ButtonDepress(99);
				}
			}
			else if (s.mVerb == "petcount")
			{
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				Sexy::Board* b = anApp->mBoard;
				fprintf(stderr, "[test] petcount tank=%d profileUnlocked=%d %s\n", b ? (int)(b->mOtherTypePetList->size() + b->mFishTypePetList->size()) : -1,
					anApp->mCurrentProfile ? anApp->mCurrentProfile->mNumOfUnlockedPets : -1, s.mArg.c_str());
			}
			else if (s.mVerb == "unlock")
			{
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				if (anApp->mBoard != nullptr)
					anApp->mBoard->MakeAndUnlockMenuButton(atoi(s.mArg.c_str()), true);
			}
			else if (s.mVerb == "fishowners")
			{
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				if (anApp->mBoard != nullptr)
				{
					std::string aList;
					for (Sexy::Fish* f : *anApp->mBoard->mFishList)
						aList += std::to_string(f->mCoopOwner) + " ";
					fprintf(stderr, "[test] fish owners: %s\n", aList.c_str());
				}
			}
			else if (s.mVerb == "coinage")
			{
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				if (anApp->mBoard != nullptr && !anApp->mBoard->mCoinList->empty())
					anApp->mBoard->mCoinList->back()->mUpdateCnt = atoi(s.mArg.c_str());
			}
			else if (s.mVerb == "p2click")
				S().TestGuestClick(atoi(s.mArg.c_str()), atoi(s.mArg.substr(s.mArg.find(' ') + 1).c_str()));
			else if (s.mVerb == "race")
			{
				// race [tank level catchup]
				int t = 0, l = 0, c = -1;
				sscanf(s.mArg.c_str(), "%d %d %d", &t, &l, &c);
				if (t > 0) SetRaceTank(t);
				if (l > 0) SetRaceLevel(l);
				if (c >= 0) SetRaceCatchUp(c != 0);
				std::string aWhy;
				if (!RaceCanStart(aWhy))
					fprintf(stderr, "[test] race can't start: %s\n", aWhy.c_str());
				RaceHostStart();
			}
			else if (s.mVerb == "raceattack")
				fprintf(stderr, "[test] raceattack %s -> %d\n", s.mArg.c_str(), RaceTestAttack(atoi(s.mArg.c_str())) ? 1 : 0);
			else if (s.mVerb == "racegift")
			{
				int a = 0, p = 100;
				sscanf(s.mArg.c_str(), "%d %d", &a, &p);
				RaceTestReceive(a, p);
			}
			else if (s.mVerb == "fishbot")
			{
				// A fair, average fish keeper for balance runs: shoots aliens, grabs coins,
				// feeds hungry fish, buys guppies (up to 10), laser upgrades and egg pieces.
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				Sexy::Board* b = anApp->mBoard;
				if (b != nullptr && !b->mPause)
				{
					auto Click = [](int x, int y) {
						PushMouse(SDL_MOUSEMOTION, x, y, 0);
						PushMouse(SDL_MOUSEBUTTONDOWN, x, y, SDL_BUTTON_LEFT);
						PushMouse(SDL_MOUSEBUTTONUP, x, y, SDL_BUTTON_LEFT);
					};
					// One alien at a time, as a person aims (the laser hits at most every 280 ms),
					// skipping shielded ones while there's another to shoot.
					Sexy::Alien* aTarget = nullptr;
					for (Sexy::Alien* a : *b->mAlienList)
						if (aTarget == nullptr || (KeeperAlienShielded(aTarget) && !KeeperAlienShielded(a)))
							aTarget = a;
					if (aTarget != nullptr)
						Click((int)aTarget->mXD + 80, (int)aTarget->mYD + 80);
					int aCoins = 0;
					for (Sexy::Coin* c : *b->mCoinList)
						if (c != nullptr && !c->m0x198 && aCoins++ < 2)
							Click(c->mX + c->mWidth / 2, c->mY + c->mHeight / 2);
					// Guppies, and on Tank 4 the breeders that stand in for them.
					bool aFed = (int)b->mFoodList->size() >= std::min(Sexy::gFoodLimit, 3);
					for (Sexy::Fish* f : *b->mFishList)
						if (!aFed && f != nullptr && f->mHunger < 300)
						{
							Click(std::clamp((int)f->mXD + 40, 40, 560), std::clamp((int)f->mYD - 20, 100, 360));
							aFed = true;
						}
					for (Sexy::Breeder* f : *b->mBreederList)
						if (!aFed && f != nullptr && f->mHunger < 300)
						{
							Click(std::clamp((int)f->mXD + 40, 40, 560), std::clamp((int)f->mYD - 20, 100, 360));
							aFed = true;
						}
					int aFish = (int)b->mFishList->size() + (int)b->mBreederList->size();
					int aFishSlot = b->mTank == 4 ? Sexy::SLOT_BREEDER : Sexy::SLOT_GUPPY;	// Tank 4 sells breeders
					// A few fish, the food upgrades, more fish, the tank's big fish (buying one
					// unlocks the laser and the egg), two laser upgrades (it starts at level 2),
					// then the egg.
					auto CanBuy = [b](int theSlot) { return b->mSlotNumber[theSlot] >= 0 && b->mSlotUnlocked[theSlot] && b->mMoney >= b->mSlotPrices[theSlot] + 60; };
					int aBigFish = -1;
					if (!b->mSlotUnlocked[Sexy::SLOT_EGG] || !b->mSlotUnlocked[Sexy::SLOT_WEAPON])
						for (int aSlot : { Sexy::SLOT_OSCAR, Sexy::SLOT_STARCATCHER, Sexy::SLOT_GRUBBER, Sexy::SLOT_GEKKO, Sexy::SLOT_ULTRA })
							if (aBigFish < 0 && b->mSlotNumber[aSlot] >= 0 && b->mSlotUnlocked[aSlot])
								aBigFish = aSlot;
					if (aFish < 4 && CanBuy(aFishSlot))
						b->HandleBuySlotPressed(aFishSlot);
					else if (Sexy::gFoodLimit < 3 && CanBuy(Sexy::SLOT_FOODLIMIT))
						b->HandleBuySlotPressed(Sexy::SLOT_FOODLIMIT);
					else if (Sexy::gFoodType < 1 && CanBuy(Sexy::SLOT_FOODLVL))
						b->HandleBuySlotPressed(Sexy::SLOT_FOODLVL);
					else if (aFish < 10 && CanBuy(aFishSlot))
						b->HandleBuySlotPressed(aFishSlot);
					else if (aBigFish >= 0 && CanBuy(aBigFish))
						b->HandleBuySlotPressed(aBigFish);
					else if (aFish >= 6 && b->m0x3e4 < 4 && CanBuy(Sexy::SLOT_WEAPON))
						b->HandleBuySlotPressed(Sexy::SLOT_WEAPON);
					else if (b->mMoney >= b->mSlotPrices[Sexy::SLOT_EGG] + 100)
						b->HandleBuySlotPressed(Sexy::SLOT_EGG);
				}
			}
			// Pet Heroes: heroespractice | heroespick <hero> | heroesbot 0/1 | heroesspeed <n> |
			// heroesstate [label] | heroesgiveup | heroestutorial | heroeswarp <arena> |
			// heroeslevel <n> | heroesaim <slot> | heroesskill <0-2>
			else if (s.mVerb == "heroespractice")
				HeroesOpenPractice();
			else if (s.mVerb == "heroespick")
				fprintf(stderr, "[test] heroespick %s -> %d\n", s.mArg.c_str(), HeroesTestPick(atoi(s.mArg.c_str())) ? 1 : 0);
			else if (s.mVerb == "heroesbot")
				HeroesTestBot(atoi(s.mArg.c_str()) != 0);
			else if (s.mVerb == "heroesspeed")
				HeroesTestSpeed(atoi(s.mArg.c_str()));
			else if (s.mVerb == "heroesstate")
				fprintf(stderr, "[test] heroesstate %s %s\n", HeroesDebugState().c_str(), s.mArg.c_str());
			else if (s.mVerb == "heroesgiveup")
				HeroesTestGiveUp();
			else if (s.mVerb == "heroesbothero")
				HeroesTestBotHero(atoi(s.mArg.c_str()));
			else if (s.mVerb == "heroeshost")
				HeroesHostStart();
			else if (s.mVerb == "heroeslock")
				fprintf(stderr, "[test] heroeslock %s -> %d\n", s.mArg.c_str(), HeroesTestLock(atoi(s.mArg.c_str())) ? 1 : 0);
			else if (s.mVerb == "heroestutorial")
				HeroesTestTutorial();
			else if (s.mVerb == "heroeswarp")
				HeroesTestWarp(atoi(s.mArg.c_str()));
			else if (s.mVerb == "heroeslevel")
				HeroesTestLevel(atoi(s.mArg.c_str()));
			else if (s.mVerb == "heroesaim")
				HeroesTestAim(atoi(s.mArg.c_str()));
			else if (s.mVerb == "heroesskill")
				HeroesTestBotSkill(atoi(s.mArg.c_str()));
			else if (s.mVerb == "quitwhenidle")
				gQuitWhenIdle = true;
			else if (s.mVerb == "keeperbot")
				KeeperTestBot(atoi(s.mArg.c_str()));
			else if (s.mVerb == "shootaliens")
			{
				// One real laser click on the middle of every alien, like a busy player.
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				if (anApp->mBoard != nullptr)
					for (Sexy::Alien* a : *anApp->mBoard->mAlienList)
					{
						int cx = (int)a->mXD + 80, cy = (int)a->mYD + 80;
						PushMouse(SDL_MOUSEMOTION, cx, cy, 0);
						PushMouse(SDL_MOUSEBUTTONDOWN, cx, cy, SDL_BUTTON_LEFT);
						PushMouse(SDL_MOUSEBUTTONUP, cx, cy, SDL_BUTTON_LEFT);
					}
			}
			else if (s.mVerb == "p2shootaliens")
			{
				// Player 2's laser click on the middle of every alien (their cursor shows it).
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				if (anApp->mBoard != nullptr)
					for (Sexy::Alien* a : *anApp->mBoard->mAlienList)
						S().TestGuestClick((int)a->mXD + 80, (int)a->mYD + 80);
			}
			else if (s.mVerb == "zapaliens")
			{
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				// One last laser hit on every alien; they die on their next update.
				if (anApp->mBoard != nullptr)
					for (Sexy::Alien* a : *anApp->mBoard->mAlienList)
					{
						a->mHealth = 1;
						a->mHitFlashTimer = 0;
						a->Shot((int)a->mXD + 80, (int)a->mYD + 80);
					}
			}
			else if (s.mVerb == "leaveboard")
				((Sexy::WinFishApp*)Sexy::gSexyAppBase)->LeaveGameBoard();
			else if (s.mVerb == "racestate")
				fprintf(stderr, "[test] racestate %s %s\n", RaceDebugState().c_str(), s.mArg.c_str());
			// Alien Keeper (the lair): keeperbuy <kind> | keeperfeed | keepergrow | keepercollect |
			// keepersend <count> [x y] | keeperbuff 0 shield / 1 frenzy | keeperblackout |
			// keeperhunt <x> <y> | keeperhunt off | keepermoney <n> | keeperstate [label]
			else if (s.mVerb == "keeperbuy")
				fprintf(stderr, "[test] keeperbuy %s -> %d\n", s.mArg.c_str(), KeeperTestBuy(atoi(s.mArg.c_str())) ? 1 : 0);
			else if (s.mVerb == "keeperfeed")
				KeeperTestFeed();
			else if (s.mVerb == "keepergrow")
				KeeperTestGrow();
			else if (s.mVerb == "keepercollect")
				KeeperTestCollect();
			else if (s.mVerb == "keepersend")
			{
				int aCount = 0, x = -1, y = -1;
				sscanf(s.mArg.c_str(), "%d %d %d", &aCount, &x, &y);
				fprintf(stderr, "[test] keepersend %s -> %d\n", s.mArg.c_str(), KeeperTestSend(aCount, x, y) ? 1 : 0);
			}
			else if (s.mVerb == "keeperselect")
				KeeperTestSelectGrown(atoi(s.mArg.c_str()));
			else if (s.mVerb == "keeperbuff")
				fprintf(stderr, "[test] keeperbuff %s -> %d\n", s.mArg.c_str(), KeeperTestBuff(atoi(s.mArg.c_str())) ? 1 : 0);
			else if (s.mVerb == "keeperblackout")
				fprintf(stderr, "[test] keeperblackout -> %d\n", KeeperTestBlackout() ? 1 : 0);
			else if (s.mVerb == "keeperhunt")
			{
				int x = 0, y = 0;
				bool anOn = sscanf(s.mArg.c_str(), "%d %d", &x, &y) == 2;
				KeeperTestHunt(anOn, x, y);
			}
			else if (s.mVerb == "keepermoney")
				KeeperTestSetMoney(atoi(s.mArg.c_str()));
			else if (s.mVerb == "keeperstate")
				fprintf(stderr, "[test] keeperstate %s %s\n", KeeperDebugState().c_str(), s.mArg.c_str());
			else if (s.mVerb == "mode")
			{
				// mode 0 co-op / 1 rivals (Adventure and Time Trial); 2 Tank Race / 3 Alien Keeper (Versus)
				int aMode = atoi(s.mArg.c_str());
				if (aMode >= 2)
					SetVersusKeeper(aMode == 3);
				else
					S().SetMode(aMode);
			}
			else if (s.mVerb == "wallets")
			{
				fprintf(stderr, "[test] rivals=%d actor=%d wallet0=%d eggs0=%d wallet1=%d eggs1=%d %s\n", RivalsActive() ? 1 : 0, RivalsActor(),
					RivalsWallet(0), RivalsEggPieces(0), RivalsWallet(1), RivalsEggPieces(1), s.mArg.c_str());
			}
			else if (s.mVerb == "dropcoin")
			{
				// dropcoin <type> <owner> <x> <y>
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				int aType = 0, anOwner = -1, x = 300, y = 200;
				sscanf(s.mArg.c_str(), "%d %d %d %d", &aType, &anOwner, &x, &y);
				if (anApp->mBoard != nullptr)
				{
					anApp->mBoard->DropCoin(x, y, aType, nullptr, -1.0, 0);
					anApp->mBoard->mCoinList->back()->mCoopOwner = anOwner;
				}
			}
			else if (s.mVerb == "buyguppy")
			{
				// buyguppy [count]: buy guppies (breeders on Tank 4) from the shop bar, as a click would.
				Sexy::Board* b = ((Sexy::WinFishApp*)Sexy::gSexyAppBase)->mBoard;
				int aSlot = (b != nullptr && b->mTank == 4) ? Sexy::SLOT_BREEDER : Sexy::SLOT_GUPPY;
				for (int i = 0; b != nullptr && i < std::max(1, atoi(s.mArg.c_str())); i++)
					b->HandleBuySlotPressed(aSlot);
			}
			else if (s.mVerb == "fakeversion")
				SetFakeVersion(s.mArg);		// the version told to the partner (before host/join)
			else if (s.mVerb == "advlevel")
			{
				// advlevel <tank> <level>: where this profile's Adventure resumes (videos).
				Sexy::WinFishApp* anApp = (Sexy::WinFishApp*)Sexy::gSexyAppBase;
				int aTank = 1, aLevel = 1;
				sscanf(s.mArg.c_str(), "%d %d", &aTank, &aLevel);
				if (anApp->mCurrentProfile != nullptr)
				{
					anApp->mCurrentProfile->mTank = std::clamp(aTank, 1, 4);
					anApp->mCurrentProfile->mLevel = std::clamp(aLevel, 1, 5);
				}
			}
			else if (s.mVerb == "log")
				S().Log("[test] %s", s.mArg.c_str());
			else if (s.mVerb == "video")
			{
				std::string aPath;
				int aFps = 30;
				std::istringstream as(s.mArg);
				as >> aPath >> aFps;
				StopVideo();
				if (aPath != "stop")
				{
					SDL_GL_GetDrawableSize((SDL_Window*)Sexy::gSexyAppBase->mWindow, &gVideoW, &gVideoH);
					aPath = ScriptPath(aPath);
					std::error_code ec;
					std::filesystem::create_directories(std::filesystem::path(aPath).parent_path(), ec);
					char aCmd[1024];
					snprintf(aCmd, sizeof(aCmd), "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - "
						"-vf \"vflip,scale=trunc(iw/2)*2:trunc(ih/2)*2\" -c:v libx264 -preset fast -crf 14 -pix_fmt yuv420p \"%s\"",
						gVideoW, gVideoH, std::max(1, aFps), aPath.c_str());
					gVideo = popen(aCmd, "w");
					gVideoEvery = (uint32_t)(1000 / std::max(1, aFps));
					gVideoNextAt = aNow;
					gVideoDue = gVideoFrames = gVideoRepeats = 0;
					fprintf(stderr, "[test] video %s %dx%d at %d fps: %s (ticks %u)\n", aPath.c_str(), gVideoW, gVideoH, aFps, gVideo ? "ok" : "FAILED", SDL_GetTicks());
				}
			}
			else if (s.mVerb == "quit")
			{
				StopVideo();
				fprintf(stderr, "[test] quit\n");
				S().Shutdown();
				exit(0);
			}
		}
		if (gQuitWhenIdle && !RaceBusy())
		{
			fprintf(stderr, "[test] quit (round over)\n");
			S().Shutdown();
			exit(0);
		}
		if (gVideo != nullptr && aNow >= gVideoNextAt)
		{
			while (gVideoNextAt <= aNow)
			{
				gVideoDue++;
				gVideoNextAt += gVideoEvery;
			}
			Sexy::gSexyAppBase->mWidgetManager->MarkAllDirty();
		}
		// One frame per interval, repeating the last picture when the game falls behind,
		// so a video made at 1000/every fps plays in real time.
		if (!gRecordPrefix.empty() && aNow >= gRecordNextAt)
		{
			while (gRecordNextAt <= aNow)
			{
				char aName[32];
				snprintf(aName, sizeof(aName), "%05d.png", gRecordFrame++);
				gPendingFrames.push_back(gRecordPrefix + aName);
				gRecordNextAt += gRecordEvery;
			}
			Sexy::gSexyAppBase->mWidgetManager->MarkAllDirty();
		}
	}

	void TestHarnessPostDraw()
	{
		if (!Sexy::gLastDrawScreenDrew)
			return;
		if (gVideo != nullptr && gVideoDue > 0)
		{
			int w = 0, h = 0;
			if (Sexy::ReadBackbuffer(gVideoBuf, w, h) && w == gVideoW && h == gVideoH)
				for (gVideoRepeats += gVideoDue - 1; gVideoDue > 0; gVideoDue--)
				{
					fwrite(gVideoBuf.data(), 4, gVideoBuf.size(), gVideo);
					gVideoFrames++;
				}
			gVideoDue = 0;
		}
		for (const std::string& aPath : gPendingFrames)
			if (!Sexy::WriteBackbufferPNG(aPath))
				fprintf(stderr, "[test] frame %s FAILED\n", aPath.c_str());
		gPendingFrames.clear();
		for (const std::string& aPath : gPendingShots)
		{
			bool ok = Sexy::WriteBackbufferPNG(aPath);
			fprintf(stderr, "[test] shot %s %s\n", aPath.c_str(), ok ? "ok" : "FAILED");
		}
		gPendingShots.clear();
	}

	bool TestHarnessActive()
	{
		return getenv("INSANIQ_TESTSCRIPT") != nullptr;
	}
}
