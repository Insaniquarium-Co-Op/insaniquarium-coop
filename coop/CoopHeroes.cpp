#include <SexyAppFramework/Font.h>
#include "CoopHeroes.h"
#include "CoopHeroesDraw.h"
#include "CoopSession.h"
#include "CoopUI.h"
#include "CoopNet.h"
#include "CoopProtocol.h"
#include "HeroesMatch.h"
#include "HeroesBot.h"
#include "HeroesMap.h"
#include "WinFishApp.h"
#include "Res.h"
#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/Widget.h>
#include <SexyAppFramework/WidgetManager.h>
#include <SexyAppFramework/KeyCodes.h>
#include <SexyAppFramework/MusicInterface.h>
#include <cstring>
#include <SexyAppFramework/GLInterface.h>
#include <SDL.h>
#include <deque>
#include <memory>

using namespace Sexy;
using namespace Heroes;

namespace Coop
{
	static WinFishApp* App() { return (WinFishApp*)gSexyAppBase; }
	static uint32_t Now() { return NetMillis(); }
	static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }

	enum HeroesPhase { HP_IDLE, HP_DRAFT, HP_COUNTDOWN, HP_MATCH, HP_RESULT };

	class HeroesScreen;

	// The sides' messages over the game's connection (a network match).
	class SessionLink : public Link
	{
	public:
		uint32_t				mMatchId = 0;
		std::deque<Packet>		mIn;
		void Send(uint8_t theType, const std::vector<uint8_t>& theData) override
		{
			ByteWriter w;
			w.U32(mMatchId);
			w.U8(theType);
			w.Bytes(theData.data(), theData.size());
			S().SendMsg(MSG_HEROES_DATA, w.mData);
		}
		bool Receive(Packet& thePacket) override
		{
			if (mIn.empty())
				return false;
			thePacket = std::move(mIn.front());
			mIn.pop_front();
			return true;
		}
	};

	// The guided first match (D36): a step list over a practice match against a bot that
	// stays home until you're done.
	enum TutorialStep
	{
		TS_MOVE, TS_ATTACK, TS_AIM, TS_FARM, TS_BUY, TS_PORTAL, TS_LANE, TS_HOME_WINDOW, TS_CAMP, TS_TOWER, TS_COUNT
	};
	static const char* kTutorialText[TS_COUNT] =
	{
		"Move your hero with W A S D.",
		"An enemy minion got into your tank. Swim next to it: your hero attacks on its own.",
		"Hold Q to see where your ability goes, then let go to cast it. (Tap it to cast right away.)",
		"Left-click coins to collect them, and click the water to drop food for hungry fish ($5).",
		"Press 1 to buy a guppy. More fish, more coins!",
		"Go through the swirling portal at the top: it leads to the Trench, between the two tanks.",
		"Both waves of minions fight here. Help yours, and grab the coins that dead minions drop.",
		"Your tank is in the window at the top right. Click a coin in it to collect it from here.",
		"Fight a camp: right-click the Gus camp (up on the left) and beat it for a buff. Hurt? Go home to heal.",
		"Push through the far gate (right) into the rival's tank, and hit one of their towers.",
	};

	struct HeroesState
	{
		HeroesPhase	mPhase = HP_IDLE;
		bool		mPractice = true;
		HeroesScreen* mScreen = nullptr;

		// Draft.
		int			mMyHero = -1, mTheirHero = -1;
		int			mBotHero = -1;				// -1: random
		int			mBotSkill = 1;
		int			mHover = -1;
		std::string	mStatus;

		// Network: my side here, theirs on the partner's computer.
		uint32_t	mMatchId = 0;
		bool		mMyLocked = false, mTheirLocked = false;
		std::unique_ptr<Side>	mNetSide;
		std::unique_ptr<SessionLink> mLink;

		// Practice: both sides here, the bot plays the other.
		std::unique_ptr<Match>	mMatch;
		Bot			mBot[2];
		bool		mBotMine = false;			// tests: the bot plays my side too
		int			mSpeed = 1;

		uint32_t	mPhaseAt = 0;
		uint32_t	mSimNow = 0, mLastReal = 0, mAccum = 0;
		uint32_t	mLastSeq = 0;
		std::string	mNote;
		uint32_t	mNoteAt = 0;
		int			mShopTab = -1;
		bool		mTabHome = false;
		bool		mHeld[4] = { false, false, false, false };	// W A S D
		int			mAimSlot = -1;				// aiming: held key, or clicked on the HUD
		bool		mAimHeld = false;			// ...by holding its key (cast on release)
		KeyCode		mAimKey = KEYCODE_UNKNOWN;
		int			mTestAim = -1;				// tests: show this ability's aim
		bool		mRightDown = false;
		uint32_t	mLastDragOrder = 0;
		uint32_t	mEscAt = 0;
		bool		mWon = false;
		std::string	mReason;
		uint32_t	mMatchMs = 0;
		bool		mSudden = false;
		std::string	mNames[kMaxPlayers];
		// Home alerts (D30): what we last saw of home, and the current alert.
		float		mSeenTowerHp[2] = { 0, 0 }, mSeenCoreHp = 0;
		int			mSeenBitten = 0, mSeenKilled = 0, mSeenStarved = 0;
		bool		mSeenHeroHome = false;
		std::string	mAlert;
		uint32_t	mAlertAt = 0, mAlarmAt = 0;
		bool		mHelpHeld = false;			// H
		bool		mHelpIntro = false;			// the card before a first practice (the countdown waits)
		bool		mHelpSeen = false;
		bool		mPaused = false;			// practice only
		// The tutorial.
		bool		mTutorial = false;
		int			mTutStep = 0;
		bool		mTutDone = false;
		uint32_t	mTutStepAt = 0;
		Vec			mTutFrom;
		int			mTutFood = 0, mTutCoins = 0, mTutHomeCoins = 0, mTutGuppies = 0, mTutStartEarned = 0, mTutKills = 0;
		uint32_t	mTutMinion = 0;
		bool		mTutorialSeen = false;
	};
	static HeroesState gH;

	static Side* MySide()
	{
		if (gH.mMatch)
			return &gH.mMatch->mSide[0];
		return gH.mNetSide.get();
	}

	static void Note(const std::string& theText)
	{
		gH.mNote = theText;
		gH.mNoteAt = Now();
	}

	static int HeldIndex(KeyCode theKey)
	{
		switch (theKey)
		{
		case 'W': return 0;
		case 'A': return 1;
		case 'S': return 2;
		case 'D': return 3;
		default: return -1;
		}
	}

	static void ReleaseKeys()
	{
		for (bool& b : gH.mHeld)
			b = false;
		gH.mAimSlot = -1;
		gH.mAimHeld = false;
	}

	// One tune at a time: the game's songs are separate tracks, so stop the others first
	// (else the menu, match and sudden-death music play on top of each other).
	static void Music(int theSong, int theOffset)
	{
		WinFishApp* anApp = App();
		if (anApp == nullptr)
			return;
		anApp->mMusicInterface->StopAllMusic();
		anApp->PlayMusic(theSong, theOffset);
	}

	static void SetPhase(HeroesPhase thePhase)
	{
		ReleaseKeys();
		gH.mAlert.clear();
		gH.mAlarmAt = 0;
		gH.mPhase = thePhase;
		gH.mPhaseAt = Now();
	}

	static int ViewedArena()
	{
		Side* s = MySide();
		if (s == nullptr)
			return 0;
		if (gH.mTabHome || !s->mHero.mAlive)
			return s->mTeam;
		return s->mHero.mArena;
	}

	// The home window is up while my hero is out of my tank and I'm not looking at home.
	static bool HomeWindowUp()
	{
		Side* s = MySide();
		return s != nullptr && (gH.mPhase == HP_MATCH || gH.mPhase == HP_COUNTDOWN) && ViewedArena() != s->mTeam;
	}

	///////////////////////////////////////////////////////////////////////////
	// The screen
	///////////////////////////////////////////////////////////////////////////
	static void StartMatch();
	static void CloseScreen();
	static void CancelDraft();
	static void LockIn();
	static void Tick();
	static void StartTutorial();

	class HeroesScreen : public Widget
	{
	public:
		int		mMouseX = -1, mMouseY = -1;

		HeroesScreen()
		{
			mMouseVisible = true;
			mHasAlpha = false;
			mClip = true;
		}

		virtual bool WantsFocus() override { return true; }

		virtual void Update() override
		{
			Widget::Update();
			HeroesUpdate();
			MarkDirty();
		}

		HV::ViewState View() const
		{
			HV::ViewState v;
			v.mSide = MySide();
			v.mArena = ViewedArena();
			v.mNow = v.mSide != nullptr ? v.mSide->mNow : 0;
			v.mMouseX = mMouseX;
			v.mMouseY = mMouseY;
			v.mShopTab = gH.mShopTab;
			v.mNote = gH.mNote;
			v.mNoteAt = gH.mNoteAt;
			v.mPractice = gH.mPractice;
			for (int i = 0; i < kMaxPlayers; i++)
				v.mNames[i] = gH.mNames[i];
			v.mAimSlot = gH.mTestAim >= 0 ? gH.mTestAim : gH.mAimSlot;
			v.mAlert = gH.mAlert;
			v.mAlertAt = gH.mAlertAt;
			v.mHomeWindow = HomeWindowUp();
			if (v.mSide != nullptr && v.mHomeWindow)
			{
				const HeroState& h = v.mSide->mHero;
				float sx = HV::SX(h.mPos.x), sy = HV::SY(h.mPos.y);
				v.mHomeFaded = h.mArena == v.mArena && sx > HV::kHomeX - 20 && sy < HV::kHomeY + HV::kHomeH + 20;
			}
			if (gH.mTutorial)
			{
				v.mTutorialSteps = TS_COUNT;
				v.mTutorialStep = gH.mTutStep;
				v.mTutorialDone = gH.mTutDone;
				v.mTutorial = gH.mTutDone ? "You're ready! The bot wakes up now: keep playing, or press Esc to leave." : kTutorialText[std::clamp(gH.mTutStep, 0, TS_COUNT - 1)];
				if (gH.mTutDone && v.mSide != nullptr && Elapsed(v.mSide->mNow, gH.mTutStepAt + 9000))
					v.mTutorial.clear();
			}
			// Notes are timed on the real clock; the drawing code compares with v.mNow.
			if (v.mSide != nullptr)
				v.mNoteAt = v.mNow - std::min<uint32_t>(Now() - gH.mNoteAt, 100000);
			return v;
		}

		virtual void Draw(Graphics* g) override
		{
			if (gH.mPhase == HP_DRAFT)
			{
				HV::DrawDraft(g, Now(), gH.mHover, gH.mMyHero, gH.mTheirHero, gH.mPractice, gH.mBotHero, gH.mBotSkill, gH.mStatus);
				return;
			}
			Side* s = MySide();
			if (s == nullptr)
				return;
			HV::ViewState v = View();
			HV::DrawTank(g, v);
			HV::DrawTopStrip(g, v);
			HV::DrawFeed(g, v);
			HV::DrawDeath(g, v);
			HV::DrawTalents(g, v);
			HV::DrawHud(g, v);
			HV::DrawTutorial(g, v);
			HV::DrawShop(g, v);
			if (gH.mPhase == HP_COUNTDOWN && !gH.mHelpIntro)
				HV::DrawCountdown(g, 3 - (int)((Now() - gH.mPhaseAt) / 1000));
			if (gH.mHelpIntro || (gH.mHelpHeld && gH.mPhase != HP_RESULT))
				HV::DrawHelp(g, s->mHero.mHero, gH.mHelpIntro);
			if (gH.mPhase == HP_MATCH && (gH.mPaused || (!Elapsed(Now(), gH.mEscAt + 3000) && gH.mEscAt != 0)))
			{
				g->SetColor(Color(0, 0, 0, 170));
				g->FillRect(130, 180, 380, 70);
				g->SetFont(FONT_JUNGLEFEVER12OUTLINE);
				g->SetColor(Color(255, 230, 150));
				std::string t = gH.mPaused ? "Paused" : "Press Esc again to give up.";
				g->DrawString(t, 320 - FONT_JUNGLEFEVER12OUTLINE->StringWidth(t) / 2, 208);
				if (gH.mPaused)
				{
					std::string u = gH.mTutorial && !gH.mTutDone ? "Esc again: leave the tutorial.  Any other key: keep going." : "Esc again: give up.  Any other key: keep playing.";
					g->SetFont(FONT_JUNGLEFEVER10OUTLINE);
					g->SetColor(Color(230, 240, 255));
					g->DrawString(u, 320 - FONT_JUNGLEFEVER10OUTLINE->StringWidth(u) / 2, 232);
				}
			}
			if (gH.mPhase == HP_RESULT)
			{
				HeroSnap o;
				int aFishLost = 0, aEarned = 0;
				if (const HeroSnap* p = s->OtherHero())
					o = *p;
				aFishLost = o.mFishLost;
				aEarned = (int)o.mEarned;
				if (gH.mMatch)
				{
					Side& t = gH.mMatch->mSide[1];
					o = t.MySnap();
					o.mBaseHero = t.mHero.mHero;
					aFishLost = t.mArena.mFishLost;
					aEarned = t.mArena.mMoneyEarned;
				}
				HV::DrawResult(g, v, gH.mWon, gH.mReason, gH.mMatchMs, o, aFishLost, aEarned);
			}
		}

		virtual void MouseMove(int x, int y) override
		{
			mMouseX = x;
			mMouseY = y;
			if (gH.mPhase == HP_DRAFT)
			{
				int h = HV::DraftHit(x, y, gH.mPractice);
				gH.mHover = h >= 0 && h < HERO_COUNT ? h : -1;
			}
		}

		virtual void MouseDrag(int x, int y) override
		{
			MouseMove(x, y);
			// Holding the right button keeps moving toward the cursor.
			Side* s = MySide();
			if (gH.mPhase == HP_MATCH && gH.mRightDown && s != nullptr && HV::InTank(x, y) && ViewedArena() == s->mHero.mArena
				&& Elapsed(Now(), gH.mLastDragOrder + 150) && gH.mAimSlot < 0)
			{
				gH.mLastDragOrder = Now();
				s->OrderMove(HV::ToWorld(x, y));
			}
		}

		virtual void MouseUp(int x, int y, int theClickCount) override
		{
			if (theClickCount < 0)
				gH.mRightDown = false;
		}

		virtual void MouseDown(int x, int y, int theClickCount) override
		{
			mMouseX = x;
			mMouseY = y;
			if (gH.mHelpIntro)
			{
				gH.mHelpIntro = false;
				return;
			}
			switch (gH.mPhase)
			{
			case HP_DRAFT: DraftClick(x, y); break;
			case HP_MATCH: case HP_COUNTDOWN: MatchClick(x, y, theClickCount < 0); break;
			case HP_RESULT:
				if (HV::ResultHit(x, y))
					CloseScreen();
				break;
			default: break;
			}
		}

		void DraftClick(int x, int y)
		{
			int h = HV::DraftHit(x, y, gH.mPractice);
			if (h >= 0 && h < HERO_COUNT)
			{
				if (gH.mMyLocked)
					return;
				gH.mMyHero = h;
				HV::PlaySound(SND_BUY);
				return;
			}
			switch (h)
			{
			case HV::DB_OPPONENT:
				gH.mBotHero = gH.mBotHero + 1 >= HERO_COUNT ? -1 : gH.mBotHero + 1;
				HV::PlaySound(SND_COIN);
				break;
			case HV::DB_SKILL:
				gH.mBotSkill = (gH.mBotSkill + 1) % 3;
				HV::PlaySound(SND_COIN);
				break;
			case HV::DB_TUTORIAL:
				if (gH.mMyHero < 0)
					gH.mMyHero = HERO_ITCHY;
				StartTutorial();
				break;
			case HV::DB_START:
				if (gH.mMyHero < 0)
				{
					gH.mStatus = "Pick a hero first.";
					HV::PlaySound(SND_BUZZER);
				}
				else if (gH.mPractice)
					StartMatch();
				else
					LockIn();
				break;
			case HV::DB_BACK:
				CancelDraft();
				break;
			default:
				break;
			}
		}

		void MatchClick(int x, int y, bool theRight)
		{
			Side* s = MySide();
			if (s == nullptr)
				return;
			HV::ViewState v = View();
			// The shop first, when it's open.
			if (gH.mShopTab >= 0 && !theRight)
			{
				int h = HV::ShopHit(v, x, y);
				if (h == -100)
				{
					gH.mShopTab = -1;
					return;
				}
				if (h <= -2)
				{
					gH.mShopTab = -2 - h;
					HV::PlaySound(SND_COIN);
					return;
				}
				if (h >= 0)
				{
					if (!s->Buy(h))
						Note(s->mLastRefusal);
					else if (h == SHOP_GUPPY)
						gH.mTutGuppies++;
					return;
				}
				return;
			}
			if (theRight && gH.mAimSlot >= 0)
			{
				// Right-click cancels aiming.
				gH.mAimSlot = -1;
				gH.mAimHeld = false;
				return;
			}
			// A talent card.
			int aTalent = HV::TalentHit(v, x, y);
			if (aTalent >= 0 && !theRight)
			{
				PickTalent(aTalent);
				return;
			}
			if (y >= HV::kHudY)
			{
				int h = HV::HudHit(x, y);
				if (h >= 0 && h < AB_COUNT)
				{
					const AbilityDef& a = s->mHero.Def().mAb[h];
					if (a.mAim == AIM_SELF || (h == AB_R && s->mHero.mCopyHero >= 0))
						CastAt(h, s->mHero.mPos);
					else
					{
						gH.mAimSlot = gH.mAimSlot == h ? -1 : h;	// armed: the next left-click casts
						gH.mAimHeld = false;
					}
				}
				else if (h >= 10 && h < 10 + HV::kQuickSlots)
					QuickBuy(h - 10);
				else if (h == 20)
					gH.mShopTab = gH.mShopTab >= 0 ? -1 : TAB_FISH;
				return;
			}
			if (gH.mPhase != HP_MATCH)
				return;
			// The home window: left-clicks there are home clicks (coins reach farther).
			if (!theRight && v.mHomeWindow && HV::InHome(x, y))
			{
				int aCoins = s->mArena.mMoneyEarned;
				Arena::ClickKind k = s->HomeClick(HV::HomeToWorld(x, y), 70);
				if (k == Arena::CLICK_COIN)
				{
					gH.mTutHomeCoins++;
					(void)aCoins;
				}
				else if (k == Arena::CLICK_FOOD)
					gH.mTutFood++;
				else if (k == Arena::CLICK_REFUSED)
					RefusedFood(*s);
				return;
			}
			if (!HV::InTank(x, y))
				return;
			Vec w = HV::ToWorld(x, y);
			int aView = ViewedArena();
			if (theRight)
			{
				gH.mRightDown = true;
				gH.mLastDragOrder = Now();
				if (aView != s->mHero.mArena)
				{
					Note("You're looking at home (Tab): your hero is elsewhere.");
					return;
				}
				// An enemy under the cursor (a monster too): attack it. Else move there.
				std::vector<Target> v2;
				s->Targets(aView, v2, true);		// what you see is what you click
				const Target* aBest = nullptr;
				float aBestD = 1e9f;
				for (const Target& t : v2)
				{
					if (t.mTeam == s->mTeam)
						continue;
					float d = Dist(t.mPos, w) - t.mRadius;
					if (d < 16 && d < aBestD)
					{
						aBestD = d;
						aBest = &t;
					}
				}
				if (aBest != nullptr)
					s->OrderAttack(aBest->mRef);
				else
					s->OrderMove(w);
				return;
			}
			if (gH.mAimSlot >= 0 && !gH.mAimHeld)
			{
				CastAt(gH.mAimSlot, w);
				gH.mAimSlot = -1;
				return;
			}
			if (aView == s->mTeam)
			{
				Arena::ClickKind k = s->HomeClick(w);
				if (k == Arena::CLICK_COIN)
					gH.mTutCoins++;
				else if (k == Arena::CLICK_FOOD)
					gH.mTutFood++;
				else if (k == Arena::CLICK_REFUSED)
					RefusedFood(*s);
			}
			else
				Note(aView == kTrench ? "The Trench: right-click to move and attack. Your home is in the window up right." : "That's the rival's tank: right-click to move and attack.");
		}

		void RefusedFood(const Side& s)
		{
			if ((int)s.mArena.mFood.size() >= s.mArena.mPellets)
				Note("Food limit reached: buy Food Quantity for more pellets.");
			else if (s.mArena.mMoney < 5)
				Note("Food costs $5.");
		}

		void PickTalent(int theChoice)
		{
			Side* s = MySide();
			if (s == nullptr || gH.mPhase != HP_MATCH)
				return;
			if (!s->PickTalent(theChoice))
				Note(s->PendingTalent() < 0 ? "No talent to pick right now (levels 3, 6 and 9)." : "Can't pick that now.");
		}

		void CastAt(int theSlot, Vec theAim)
		{
			Side* s = MySide();
			if (s == nullptr || gH.mPhase != HP_MATCH)
				return;
			if (ViewedArena() != s->mHero.mArena)
			{
				Note("Your hero isn't in this tank.");
				return;
			}
			if (!s->Cast(theSlot, theAim))
			{
				Note(s->mLastRefusal);
				HV::PlaySound(SND_BUZZER);
			}
		}

		void QuickBuy(int theSlot)
		{
			Side* s = MySide();
			if (s == nullptr || gH.mPhase != HP_MATCH)
				return;
			int aShop = HV::QuickShop(*s, theSlot);
			if (aShop < 0)
			{
				Note("Your build is complete!");
				return;
			}
			if (!s->Buy(aShop))
				Note(s->mLastRefusal);
			else if (aShop == SHOP_GUPPY)
				gH.mTutGuppies++;
		}

		Vec MouseAim() const
		{
			Side* s = MySide();
			Vec aAim = s != nullptr ? s->mHero.mPos : Vec();
			if (mMouseX >= 0 && HV::InTank(mMouseX, mMouseY))
				aAim = HV::ToWorld(mMouseX, mMouseY);
			return aAim;
		}

		virtual void KeyDown(KeyCode theKey) override
		{
			Side* s = MySide();
			if (theKey == KEYCODE_CONTROL)
				return;
			if (gH.mHelpIntro)
			{
				gH.mHelpIntro = false;
				return;
			}
			if (gH.mPaused)
			{
				gH.mPaused = false;
				if (theKey == KEYCODE_ESCAPE && s != nullptr)
				{
					if (gH.mTutorial && !gH.mTutDone)
						CloseScreen();
					else
						s->GiveUp();
				}
				return;
			}
			if (theKey == 'H')
			{
				gH.mHelpHeld = true;
				return;
			}
			if (gH.mPhase == HP_DRAFT)
			{
				if (theKey == KEYCODE_ESCAPE)
					CancelDraft();
				else if (theKey == KEYCODE_RETURN && gH.mMyHero >= 0)
				{
					if (gH.mPractice)
						StartMatch();
					else
						LockIn();
				}
				else if (theKey >= '1' && theKey <= '0' + HERO_COUNT && !gH.mMyLocked)
					gH.mMyHero = theKey - '1';
				return;
			}
			if (gH.mPhase == HP_RESULT)
			{
				// Only Enter (or the button): Space hops and Esc gives up, so mashing either
				// as the match ends shouldn't close the results unread.
				if (theKey == KEYCODE_RETURN)
					CloseScreen();
				return;
			}
			if (s == nullptr || (gH.mPhase != HP_MATCH && gH.mPhase != HP_COUNTDOWN))
				return;
			switch (theKey)
			{
			case KEYCODE_TAB:
				gH.mTabHome = true;
				return;
			case KEYCODE_ESCAPE:
				if (gH.mShopTab >= 0)
					gH.mShopTab = -1;
				else if (gH.mAimSlot >= 0)
				{
					gH.mAimSlot = -1;
					gH.mAimHeld = false;
				}
				else if (gH.mPractice && gH.mPhase == HP_MATCH)
					gH.mPaused = true;
				else if (gH.mEscAt != 0 && !Elapsed(Now(), gH.mEscAt + 3000))
				{
					s->GiveUp();
					gH.mEscAt = 0;
				}
				else
					gH.mEscAt = Now();
				return;
			default:
				break;
			}
			if (gH.mPhase != HP_MATCH)
				return;
			if (theKey == KEYCODE_SPACE)
			{
				if (s->mHero.Def().mWalker)
					s->Hop();						// Space hops too (walkers)
				return;
			}
			int aHeld = HeldIndex(theKey);
			if (aHeld >= 0)
			{
				gH.mHeld[aHeld] = true;
				if (s->mHero.Def().mWalker)
				{
					if (theKey == 'W')
						s->Hop();
					else if (theKey == 'S' && !s->CrossNearby() && !s->mLastRefusal.empty())
						Note(s->mLastRefusal);
				}
				return;
			}
			int aSlot = -1;
			switch (theKey)
			{
			case 'Q': aSlot = AB_Q; break;
			case 'E': aSlot = AB_W; break;
			case 'R': aSlot = AB_E; break;
			case 'F': aSlot = AB_R; break;
			case 'Z': PickTalent(0); return;
			case 'X': PickTalent(1); return;
			case 'B': gH.mShopTab = gH.mShopTab >= 0 ? -1 : TAB_FISH; return;
			default: break;
			}
			if (theKey >= '1' && theKey <= '0' + HV::kQuickSlots)
			{
				QuickBuy(theKey - '1');
				return;
			}
			if (aSlot < 0)
				return;
			// Hold to aim (D36): self-casts go at once; the rest show where they'll go while
			// the key is down and cast when it comes up (a tap casts right away).
			const AbilityDef& a = s->mHero.Def().mAb[aSlot];
			if (a.mAim == AIM_SELF || (aSlot == AB_R && s->mHero.mCopyHero >= 0))
			{
				CastAt(aSlot, MouseAim());
				return;
			}
			if (gH.mAimHeld && gH.mAimKey == theKey)
				return;							// key repeat
			gH.mAimSlot = aSlot;
			gH.mAimHeld = true;
			gH.mAimKey = theKey;
		}

		virtual void LostFocus() override
		{
			Widget::LostFocus();
			ReleaseKeys();					// no key stuck down after switching windows
		}

		virtual void KeyUp(KeyCode theKey) override
		{
			if (theKey == KEYCODE_TAB)
				gH.mTabHome = false;
			int aHeld = HeldIndex(theKey);
			if (aHeld >= 0)
				gH.mHeld[aHeld] = false;
			if (theKey == 'H')
				gH.mHelpHeld = false;
			if (gH.mAimHeld && theKey == gH.mAimKey)
			{
				int aSlot = gH.mAimSlot;
				gH.mAimHeld = false;
				gH.mAimSlot = -1;
				if (aSlot >= 0 && gH.mPhase == HP_MATCH)
					CastAt(aSlot, MouseAim());
			}
		}
	};

	///////////////////////////////////////////////////////////////////////////
	// Lifecycle
	///////////////////////////////////////////////////////////////////////////
	static std::string MyName()
	{
		std::string aName = S().GetLocalName();
		return aName.empty() ? "You" : aName;
	}

	// The tank is twice the usual size: in a window, make the window as big as the
	// screen allows (4:3, up to 1280x960) so it's drawn at full detail; put it back after.
	static int sOldW = 0, sOldH = 0;

	static void GrowWindow()
	{
		WinFishApp* anApp = App();
		if (anApp == nullptr || !anApp->mIsWindowed || anApp->mWindow == nullptr || (getenv("INSANIQ_TESTSCRIPT") != nullptr && getenv("INSANIQ_HEROES_GROW") == nullptr))
			return;
		SDL_Window* w = (SDL_Window*)anApp->mWindow;
		SDL_Rect aUsable;
		if (SDL_GetDisplayUsableBounds(SDL_GetWindowDisplayIndex(w), &aUsable) != 0)
			return;
		int aTop = 0, aLeft = 0, aBottom = 0, aRight = 0;
		SDL_GetWindowBordersSize(w, &aTop, &aLeft, &aBottom, &aRight);
		int aH = std::min(960, aUsable.h - aTop - aBottom - 8);
		int aW = aH * 4 / 3;
		if (aW > aUsable.w - 8)
		{
			aW = aUsable.w - 8;
			aH = aW * 3 / 4;
		}
		SDL_GetWindowSize(w, &sOldW, &sOldH);
		if (aW <= sOldW)
		{
			sOldW = sOldH = 0;
			return;
		}
		SDL_SetWindowSize(w, aW, aH);
		SDL_SetWindowPosition(w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		if (anApp->mGLInterface != nullptr)
			anApp->mGLInterface->UpdateViewport();
	}

	static void RestoreWindow()
	{
		WinFishApp* anApp = App();
		if (sOldW <= 0 || anApp == nullptr || anApp->mWindow == nullptr)
			return;
		SDL_Window* w = (SDL_Window*)anApp->mWindow;
		SDL_SetWindowSize(w, sOldW, sOldH);
		SDL_SetWindowPosition(w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		if (anApp->mGLInterface != nullptr)
			anApp->mGLInterface->UpdateViewport();
		sOldW = sOldH = 0;
	}

	static void OpenScreen()
	{
		WinFishApp* anApp = App();
		if (gH.mScreen != nullptr || anApp == nullptr)
			return;
		if (!gH.mPractice)
		{
			// Each computer draws its own match: no co-op picture stream meanwhile.
			if (S().GetRole() == ROLE_HOST)
				S().SuspendStreaming(true);
			else
				S().SuspendGuestView();
		}
		gH.mMyLocked = gH.mTheirLocked = false;
		anApp->KillDialog(kDialogGuestMenu);
		anApp->KillDialog(kDialogCoop);
		anApp->KillDialog(kDialogVersus);
		anApp->CleanDialogs();
		anApp->RemoveGameSelector();
		GrowWindow();
		gH.mScreen = new HeroesScreen();
		gH.mScreen->Resize(0, 0, 640, 480);
		anApp->mWidgetManager->AddWidget(gH.mScreen);
		anApp->mWidgetManager->SetFocus(gH.mScreen);
		SetPhase(HP_DRAFT);
		gH.mStatus.clear();
		gH.mMyHero = -1;
		gH.mTheirHero = -1;
		if (gH.mPractice && !gH.mTutorialSeen)
			gH.mStatus = "New to Pet Heroes 3? Try the Tutorial.";
		Music(2, 0);
	}

	static void CloseScreen()
	{
		WinFishApp* anApp = App();
		bool aNetwork = !gH.mPractice;
		if (gH.mPhase == HP_MATCH || gH.mPhase == HP_COUNTDOWN)
			Music(2, 0);					// left mid-match: back to the menu's music
		SetPhase(HP_IDLE);
		gH.mMatch.reset();
		gH.mNetSide.reset();
		gH.mLink.reset();
		gH.mShopTab = -1;
		gH.mAimSlot = -1;
		gH.mTestAim = -1;
		gH.mTabHome = false;
		gH.mTutorial = false;
		if (gH.mScreen != nullptr && anApp != nullptr)
		{
			anApp->mWidgetManager->RemoveWidget(gH.mScreen);
			anApp->SafeDeleteWidget(gH.mScreen);
		}
		gH.mScreen = nullptr;
		RestoreWindow();
		if (anApp != nullptr && anApp->mGameSelector == nullptr)
			anApp->SwitchToGameSelector();
		if (aNetwork)
		{
			if (S().GetRole() == ROLE_HOST)
				S().SuspendStreaming(false);
			else if (S().GetRole() == ROLE_GUEST)
				S().ResumeGuestView();
		}
		gH.mPractice = true;
	}

	static void CancelDraft()
	{
		if (!gH.mPractice && S().IsConnected())
		{
			ByteWriter w;
			w.U32(gH.mMatchId);
			w.Str(S().GetLocalName() + " called it off.");
			S().SendMsg(MSG_HEROES_CANCEL, w.mData);
		}
		CloseScreen();
	}

	static void StartMatch()
	{
		Rng r(Now());
		int aBot = gH.mBotHero >= 0 ? gH.mBotHero : r.Int(HERO_COUNT);
		gH.mTheirHero = aBot;
		gH.mMatch.reset(new Match());
		gH.mSimNow = 1000;
		gH.mMatch->Start(gH.mMyHero, aBot, r.Next(), gH.mSimNow, 0);
		for (int t = 0; t < 2; t++)
		{
			gH.mBot[t] = Bot();
			gH.mBot[t].mSkill = gH.mBotSkill;
		}
		gH.mNames[0] = MyName();
		gH.mNames[1] = std::string("Bot ") + HeroDefOf(aBot).mName;
		gH.mLastSeq = 0;
		gH.mAccum = 0;
		gH.mLastReal = Now();
		gH.mEscAt = 0;
		gH.mPaused = false;
		gH.mSudden = false;
		SetPhase(HP_COUNTDOWN);
		gH.mHelpIntro = !gH.mHelpSeen && !gH.mTutorial && getenv("INSANIQ_TESTSCRIPT") == nullptr;
		gH.mHelpSeen = true;
		HV::PlaySound(SND_ALARM);
		Music(0, 13);
	}

	// A weak mini Sylvester comes into my tank, near my hero and out of the towers' reach.
	static void TrainingMinion(Side& s)
	{
		Arrival a;
		a.mKind = MIN_MINI;
		a.mMult = 0.6f;
		s.mArena.SpawnWave({ a }, 1 - s.mTeam, s.mNow);
		if (s.mArena.mMinions.empty())
			return;
		Minion& m = s.mArena.mMinions.back();
		gH.mTutMinion = m.mId;
		m.mPos = ClampToWater(s.mTeam, Vec(s.mHero.mPos.x < 640 ? s.mHero.mPos.x + 240 : s.mHero.mPos.x - 240, 380), 20);
	}

	static void TutorialStep(int theStep)
	{
		Side* s = MySide();
		gH.mTutStep = theStep;
		gH.mTutStepAt = s != nullptr ? s->mNow : 0;
		if (s == nullptr)
			return;
		gH.mTutFrom = s->mHero.mPos;
		gH.mTutStartEarned = s->mArena.mMoneyEarned;
		gH.mTutFood = gH.mTutCoins = gH.mTutHomeCoins = gH.mTutGuppies = 0;
		gH.mTutKills = s->mHero.mMinionKills;
		if (theStep == TS_ATTACK)
			TrainingMinion(*s);
		if (theStep == TS_FARM)
		{
			// Something to collect and someone to feed.
			for (Heroes::Fish& f : s->mArena.mFish)
				f.mHungryAt = s->mNow;
			for (int i = 0; i < 3; i++)
			{
				Heroes::Coin c;
				c.mId = s->mArena.mNextId++;
				c.mKind = COIN_GOLD;
				c.mPos = Vec(420.0f + 200 * i, 300);
				s->mArena.mCoins.push_back(c);
			}
		}
		HV::PlaySound(SND_LEVEL);
	}

	static void StartTutorial()
	{
		gH.mTutorial = true;
		gH.mTutDone = false;
		gH.mTutorialSeen = true;
		int aSkill = gH.mBotSkill, aBotHero = gH.mBotHero;
		gH.mBotSkill = 0;
		gH.mBotHero = HERO_CLYDE;
		StartMatch();
		gH.mBotSkill = aSkill;
		gH.mBotHero = aBotHero;
		gH.mBot[1].mPassive = true;
		gH.mNames[1] = "Bot Clyde";
		TutorialStep(TS_MOVE);
	}

	// Each step waits for the player to do it.
	static void CheckTutorial()
	{
		Side* s = MySide();
		if (!gH.mTutorial || gH.mTutDone || s == nullptr || gH.mPhase != HP_MATCH)
			return;
		const HeroState& h = s->mHero;
		bool aDone = false;
		switch (gH.mTutStep)
		{
		case TS_MOVE: aDone = Dist(h.mPos, gH.mTutFrom) > 180; break;
		case TS_ATTACK:
			aDone = h.mMinionKills > gH.mTutKills;
			if (!aDone && s->mArena.FindMinion(gH.mTutMinion) == nullptr)
				TrainingMinion(*s);					// a tower got it: another one
			break;
		case TS_AIM: aDone = s->CooldownLeft(AB_Q) > 0; break;
		case TS_FARM: aDone = gH.mTutCoins >= 1 && gH.mTutFood >= 1; break;
		case TS_BUY: aDone = gH.mTutGuppies >= 1; break;
		case TS_PORTAL: aDone = h.mArena == kTrench; break;
		case TS_LANE: aDone = h.mLaneCoins > 0; break;
		case TS_HOME_WINDOW:
			aDone = gH.mTutHomeCoins >= 1;
			if (!aDone && h.mArena != s->mTeam && s->mArena.mCoins.empty() && Elapsed(s->mNow, gH.mTutStepAt + 1500))
			{
				Heroes::Coin c;							// (a coin to click, if the fish haven't made one)
				c.mId = s->mArena.mNextId++;
				c.mKind = COIN_GOLD;
				c.mPos = Vec(640, 600);
				s->mArena.mCoins.push_back(c);
			}
			break;
		case TS_CAMP: aDone = h.mObjectives > 0; break;
		case TS_TOWER: aDone = h.mStructDamage > 0; break;
		default: break;
		}
		if (!h.mAlive)
			return;
		if (aDone)
		{
			if (gH.mTutStep + 1 >= TS_COUNT)
			{
				gH.mTutDone = true;
				gH.mTutStepAt = s->mNow;
				gH.mBot[1].mPassive = false;		// the real match begins
				HV::PlaySound(SND_EVOLVE);
			}
			else
				TutorialStep(gH.mTutStep + 1);
		}
	}

	static void Finish(bool theWon, const std::string& theReason)
	{
		Side* s = MySide();
		gH.mWon = theWon;
		gH.mReason = theReason;
		gH.mMatchMs = s != nullptr ? s->MatchMs() : 0;
		gH.mShopTab = -1;
		SetPhase(HP_RESULT);
		Music(2, 0);
		if (App() != nullptr)
			App()->PlaySample(theWon ? SOUND_APPLAUSE_ID : SOUND_EVILLAFF_ID);
		S().Log("Heroes: match over, %s (%s) after %u s", theWon ? "won" : "lost", theReason.c_str(), gH.mMatchMs / 1000);
		if (getenv("INSANIQ_TESTSCRIPT") != nullptr)
			fprintf(stderr, "[test] heroesevent %u finish won=%d\n", SDL_GetTicks(), theWon ? 1 : 0);
	}

	// WASD: the held keys steer my hero (walkers use only A and D; W hops, S crosses).
	static void SteerFromKeys()
	{
		Side* s = MySide();
		if (s == nullptr || gH.mBotMine)
			return;
		Vec aDir;
		if (gH.mPhase == HP_MATCH && !gH.mPaused)
		{
			bool aWalker = s->mHero.Def().mWalker;
			aDir.x = (gH.mHeld[3] ? 1.0f : 0.0f) - (gH.mHeld[1] ? 1.0f : 0.0f);
			if (!aWalker)
				aDir.y = (gH.mHeld[2] ? 1.0f : 0.0f) - (gH.mHeld[0] ? 1.0f : 0.0f);
		}
		s->Steer(aDir);
	}

	// INSANIQ_COOPSTATS: how far behind the rival is drawn, and how often it stalled (D34).
	static void LogMirrorStats(const Side& theSide)
	{
		static uint32_t sLast = 0;
		static uint64_t sSent = 0, sRecv = 0;
		if (getenv("INSANIQ_COOPSTATS") == nullptr || gH.mSimNow - sLast < 2000)
			return;
		uint32_t aSpan = gH.mSimNow - sLast;
		float aOut = (float)((S().NetBytesSent() - sSent) * 8.0 / aSpan), anIn = (float)((S().NetBytesReceived() - sRecv) * 8.0 / aSpan);
		sLast = gH.mSimNow;
		sSent = S().NetBytesSent();
		sRecv = S().NetBytesReceived();
		const Mirror& m = theSide.mOther;
		S().Log("Heroes view: delay %u ms (target %u), interval %.0f ms, worst lateness %d ms, stalls %u of %u, %.0f kbps out, %.0f in", m.DelayMs(), m.TargetDelayMs(), m.mIntervalMs, m.mWorstLateMs, m.mStarved, m.mLookups, aOut, anIn);
		m.mStarved = m.mLookups = 0;
	}

	static void Tick()
	{
		gH.mSimNow += kTickMs;
		SteerFromKeys();
		if (gH.mMatch)
		{
			if (gH.mBotMine)
				gH.mBot[0].Think(gH.mMatch->mSide[0]);
			gH.mBot[1].Think(gH.mMatch->mSide[1]);
			gH.mMatch->Step(gH.mSimNow);
		}
		else if (gH.mNetSide)
		{
			if (gH.mBotMine)
				gH.mBot[0].Think(*gH.mNetSide);
			gH.mNetSide->Step(gH.mSimNow);
			LogMirrorStats(*gH.mNetSide);
		}
	}

	static void PlayNewSounds()
	{
		Side* s = MySide();
		if (s == nullptr)
			return;
		int aView = ViewedArena();
		int aPlayed = 0;
		for (const Effect& fx : s->mEffects)
		{
			if (fx.mSeq <= gH.mLastSeq)
				continue;
			const Event& e = fx.mEvent;
			if (aPlayed < 6 && e.mType == EV_SOUND && e.mArena == aView)
			{
				HV::PlaySound(e.mParam);
				aPlayed++;
			}
			// Test videos: when the big moments happen.
			if (getenv("INSANIQ_TESTSCRIPT") != nullptr && (e.mType == EV_KILL || e.mType == EV_TOWER_DOWN || e.mType == EV_ANNOUNCE
				|| (e.mType == EV_BURST && (e.mParam == LOOK_STORM || e.mParam == LOOK_SLAM || e.mParam == LOOK_RESURRECT || e.mParam == LOOK_GOLD || e.mParam == LOOK_EVOLVE))
				|| (e.mType == EV_TEXT && e.mText == "Thunderstorm!")))
				fprintf(stderr, "[test] heroesevent %u type %d param %d arena %d t=%u\n", SDL_GetTicks(), e.mType, e.mParam, e.mArena, s->MatchMs() / 1000);
			// Always hear what matters: waves and raiders at home, towers falling, banners.
			if (e.mType == EV_WAVE && e.mArena == s->mTeam)
				HV::PlaySound(SND_ALARM);
			if (e.mType == EV_CROSS && e.mPlayer != s->mPlayer && e.mArena == s->mTeam)
				HV::PlaySound(SND_ALARM);
			if (e.mType == EV_TOWER_DOWN)
				HV::PlaySound(SND_EXPLODE);
			if (e.mType == EV_ANNOUNCE)
			{
				switch (e.mParam)
				{
				case AN_BOSS_UP: HV::PlaySound(SND_SCREAM); break;
				case AN_SQUID_UP: HV::PlaySound(SND_ROAR); break;
				case AN_CAMP: break;
				default: HV::PlaySound(SND_EVOLVE); break;
				}
			}
		}
		if (!s->mEffects.empty())
			gH.mLastSeq = std::max(gH.mLastSeq, s->mEffects.back().mSeq);
	}

	// While you look elsewhere, say what's hurting at home (the home window's frame and the
	// world map flash, an alarm at most every 8 s). What's worse wins: the core, a tower,
	// the enemy hero, fish eaten, fish starving.
	static void CheckHomeAlerts()
	{
		Side* s = MySide();
		if (s == nullptr)
			return;
		const Arena& a = s->mArena;
		HeroSnap o;
		bool aHeroHome = s->OtherHeroIn(s->mTeam, &o) && (o.mFlags & HF_ALIVE) && !(o.mFlags & HF_HIDDEN);
		std::string aAlert;
		if (a.mCoreHp < gH.mSeenCoreHp - 0.5f)
			aAlert = "Your core is under attack!";
		else if (a.mTower[0].mAlive && a.mTower[0].mHp < gH.mSeenTowerHp[0] - 0.5f)
			aAlert = "Your left tower is under attack!";
		else if (a.mTower[1].mAlive && a.mTower[1].mHp < gH.mSeenTowerHp[1] - 0.5f)
			aAlert = "Your right tower is under attack!";
		else if (aHeroHome && !gH.mSeenHeroHome)
			aAlert = "The enemy hero is in your tank!";
		else if (a.mFishBitten > gH.mSeenBitten || a.mFishKilled > gH.mSeenKilled)
			aAlert = "Your fish are being eaten!";
		else if (a.mStarved > gH.mSeenStarved)
			aAlert = "Your fish are starving!";
		gH.mSeenTowerHp[0] = a.mTower[0].mHp;
		gH.mSeenTowerHp[1] = a.mTower[1].mHp;
		gH.mSeenCoreHp = a.mCoreHp;
		gH.mSeenBitten = a.mFishBitten;
		gH.mSeenKilled = a.mFishKilled;
		gH.mSeenStarved = a.mStarved;
		gH.mSeenHeroHome = aHeroHome;
		if (aAlert.empty() || ViewedArena() == s->mTeam)
			return;
		uint32_t aNow = s->mNow;
		static const char* kOrder[] = { "Your core", "Your left", "Your right", "The enemy", "Your fish are being", "Your fish are starving" };
		auto Rank = [](const std::string& t) { for (int i = 0; i < 6; i++) if (t.compare(0, std::strlen(kOrder[i]), kOrder[i]) == 0) return i; return 9; };
		if (gH.mAlert.empty() || Elapsed(aNow, gH.mAlertAt + HV::kAlertShowMs) || Rank(aAlert) <= Rank(gH.mAlert))
		{
			gH.mAlert = aAlert;
			gH.mAlertAt = aNow;
		}
		if (gH.mAlarmAt == 0 || Elapsed(aNow, gH.mAlarmAt + 8000))
		{
			gH.mAlarmAt = aNow;
			HV::PlaySound(SND_ALARM);
			if (getenv("INSANIQ_TESTSCRIPT") != nullptr)
				fprintf(stderr, "[test] heroesalert %s\n", aAlert.c_str());
		}
	}

	void HeroesUpdate()
	{
		if (gH.mPhase == HP_IDLE)
			return;
		uint32_t aNow = Now();
		if (gH.mPhase == HP_COUNTDOWN && gH.mHelpIntro)
			gH.mPhaseAt = aNow;				// the countdown waits for the help card
		if (gH.mPhase == HP_COUNTDOWN && Elapsed(aNow, gH.mPhaseAt + 3000))
		{
			SetPhase(HP_MATCH);
			gH.mLastReal = aNow;
			gH.mAccum = 0;
			if (gH.mTutorial)
				TutorialStep(gH.mTutStep);
		}
		if (gH.mPhase == HP_MATCH && gH.mPaused)
		{
			gH.mLastReal = aNow;
			return;
		}
		if (gH.mPhase == HP_MATCH)
		{
			gH.mAccum += std::min<uint32_t>(aNow - gH.mLastReal, 250) * std::max(1, gH.mSpeed);
			gH.mLastReal = aNow;
			int n = 0;
			while (gH.mAccum >= (uint32_t)kTickMs && n < 8 * std::max(1, gH.mSpeed))
			{
				gH.mAccum -= kTickMs;
				Tick();
				n++;
			}
			PlayNewSounds();
			CheckHomeAlerts();
			CheckTutorial();
			Side* s = MySide();
			if (s != nullptr)
			{
				if (s->mSuddenDeath && !gH.mSudden)
				{
					gH.mSudden = true;
					Music(1, 1);
					Note("Sudden death: waves grow every minute and the cores lose their armor!");
				}
				if (s->mWon)
					Finish(true, s->mOtherGaveUp ? "Your rival gave up." : "You destroyed their core!");
				else if (s->mLost)
					Finish(false, s->mArena.mCoreDead ? "Your core was destroyed." : "You gave up.");
			}
		}
	}

	bool HeroesBusy() { return gH.mPhase != HP_IDLE; }
	bool HeroesNetworkBusy() { return gH.mPhase != HP_IDLE && !gH.mPractice; }

	void HeroesOpenPractice()
	{
		gH.mPractice = true;
		OpenScreen();
	}

	///////////////////////////////////////////////////////////////////////////
	// A match with the partner
	///////////////////////////////////////////////////////////////////////////
	bool HeroesCanStart(std::string& theWhy)
	{
		if (!S().HasGuest())
		{
			theWhy = "Nobody has joined yet.";
			return false;
		}
		if (HeroesBusy())
		{
			theWhy = "A match is already on.";
			return false;
		}
		return true;
	}

	static void OpenNetworkDraft(uint32_t theMatchId)
	{
		gH.mPractice = false;
		gH.mMatchId = theMatchId;
		gH.mNames[0] = S().PlayerName(0);
		gH.mNames[1] = S().PlayerName(1);
		OpenScreen();
		gH.mStatus = "Pick a hero, then Lock in!";
		S().Log("Heroes %u: draft", theMatchId);
	}

	void HeroesHostStart()
	{
		std::string aWhy;
		if (!HeroesCanStart(aWhy))
			return;
		uint32_t anId = (Now() & 0x7FFFFFFF) | 1;
		ByteWriter w;
		w.U32(anId);
		S().SendMsg(MSG_HEROES_SETUP, w.mData);
		OpenNetworkDraft(anId);
	}

	static void StartNetMatch(uint64_t theSeed, int theHostHero, int theGuestHero)
	{
		int aTeam = S().GetRole() == ROLE_HOST ? 0 : 1;
		gH.mMyHero = aTeam == 0 ? theHostHero : theGuestHero;
		gH.mTheirHero = aTeam == 0 ? theGuestHero : theHostHero;
		gH.mLink.reset(new SessionLink());
		gH.mLink->mMatchId = gH.mMatchId;
		gH.mNetSide.reset(new Side());
		gH.mNetSide->mLink = gH.mLink.get();
		gH.mSimNow = 1000;
		gH.mNetSide->Init(aTeam, aTeam, gH.mMyHero, 1 - aTeam, theSeed + aTeam * 7919, gH.mSimNow);
		gH.mBot[0] = Bot();
		gH.mLastSeq = 0;
		gH.mAccum = 0;
		gH.mLastReal = Now();
		gH.mEscAt = 0;
		gH.mSudden = false;
		gH.mTutorial = false;
		SetPhase(HP_COUNTDOWN);
		HV::PlaySound(SND_ALARM);
		Music(0, 13);
		S().Log("Heroes %u: %s vs %s", gH.mMatchId, HeroDefOf(theHostHero).mName, HeroDefOf(theGuestHero).mName);
	}

	static void HostMaybeGo()
	{
		if (S().GetRole() != ROLE_HOST || !gH.mMyLocked || !gH.mTheirLocked || gH.mPhase != HP_DRAFT)
			return;
		uint64_t aSeed = ((uint64_t)Now() << 20) ^ 0x5EED;
		ByteWriter w;
		w.U32(gH.mMatchId);
		w.U64(aSeed);
		w.U8((uint8_t)gH.mMyHero);
		w.U8((uint8_t)gH.mTheirHero);
		S().SendMsg(MSG_HEROES_GO, w.mData);
		StartNetMatch(aSeed, gH.mMyHero, gH.mTheirHero);
	}

	static void LockIn()
	{
		if (gH.mMyLocked || gH.mMyHero < 0)
			return;
		gH.mMyLocked = true;
		ByteWriter w;
		w.U32(gH.mMatchId);
		w.U8((uint8_t)gH.mMyHero);
		S().SendMsg(MSG_HEROES_PICK, w.mData);
		gH.mStatus = gH.mTheirLocked ? "Starting..." : "Locked in! Waiting for " + gH.mNames[S().GetRole() == ROLE_HOST ? 1 : 0] + "...";
		HV::PlaySound(SND_SHIELD);
		HostMaybeGo();
	}

	void HeroesHandleMessage(uint8_t theType, ByteReader& r)
	{
		uint32_t anId = r.U32();
		switch (theType)
		{
		case MSG_HEROES_SETUP:
			if (S().GetRole() == ROLE_GUEST && gH.mPhase == HP_IDLE)
				OpenNetworkDraft(anId);
			break;
		case MSG_HEROES_PICK:
		{
			int aHero = r.U8();
			if (anId != gH.mMatchId || gH.mPhase != HP_DRAFT || aHero >= HERO_COUNT)
				break;
			gH.mTheirLocked = true;
			gH.mTheirHero = aHero;
			std::string aName = gH.mNames[S().GetRole() == ROLE_HOST ? 1 : 0];
			gH.mStatus = gH.mMyLocked ? "Starting..." : aName + " has locked in: your turn!";
			HostMaybeGo();
			break;
		}
		case MSG_HEROES_GO:
		{
			uint64_t aSeed = r.U64();
			int aHost = r.U8(), aGuest = r.U8();
			if (anId != gH.mMatchId || S().GetRole() != ROLE_GUEST || gH.mPhase != HP_DRAFT || aHost >= HERO_COUNT || aGuest >= HERO_COUNT)
				break;
			StartNetMatch(aSeed, aHost, aGuest);
			break;
		}
		case MSG_HEROES_DATA:
		{
			if (anId != gH.mMatchId || !gH.mLink)
				break;
			Packet p;
			p.mType = r.U8();
			size_t n = r.Remaining();
			const uint8_t* d = r.Bytes(n);
			if (d != nullptr)
				p.mData.assign(d, d + n);
			gH.mLink->mIn.push_back(std::move(p));
			break;
		}
		case MSG_HEROES_CANCEL:
		{
			std::string aWhy = r.Str();
			if (anId != gH.mMatchId || gH.mPractice || gH.mPhase == HP_IDLE)
				break;
			if (gH.mPhase == HP_DRAFT)
			{
				CloseScreen();
				S().AddToast(aWhy.empty() ? "The match was called off." : aWhy, -1, 400);
			}
			break;
		}
		default:
			break;
		}
	}

	void HeroesPeerLost(const std::string& theWhy)
	{
		if (!HeroesNetworkBusy())
			return;
		if (gH.mPhase == HP_MATCH || gH.mPhase == HP_COUNTDOWN)
			Finish(true, theWhy);
		else if (gH.mPhase == HP_DRAFT)
		{
			CloseScreen();
			S().AddToast(theWhy, -1, 400);
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Test harness
	///////////////////////////////////////////////////////////////////////////
	std::string HeroesDebugState()
	{
		char b[640];
		Side* s = MySide();
		if (s == nullptr)
		{
			snprintf(b, sizeof(b), "phase=%d hero=%d", gH.mPhase, gH.mMyHero);
			return b;
		}
		const Side* o = gH.mMatch ? &gH.mMatch->mSide[1] : nullptr;
		snprintf(b, sizeof(b), "phase=%d t=%u me(%s L%d hp %.0f arena %d $%d fish %d towers %.0f/%.0f core %.0f K/D %d/%d lane $%d obj %d talent %d) them(%s L%d towers %.0f/%.0f core %.0f) tutorial %d/%d",
			gH.mPhase, s->MatchMs() / 1000, HeroDefOf(s->mHero.mHero).mName, s->mHero.mLevel, s->mHero.mHp, s->mHero.mArena, s->mArena.mMoney,
			s->mArena.FishCount(), s->mArena.mTower[0].mHp, s->mArena.mTower[1].mHp, s->mArena.mCoreHp, s->mHero.mKills, s->mHero.mDeaths,
			s->mHero.mLaneCoins, s->mHero.mObjectives, s->PendingTalent(),
			o ? HeroDefOf(o->mHero.mHero).mName : "?", o ? o->mHero.mLevel : 0, o ? o->mArena.mTower[0].mHp : 0, o ? o->mArena.mTower[1].mHp : 0, o ? o->mArena.mCoreHp : 0,
			gH.mTutorial ? gH.mTutStep : -1, gH.mTutDone ? 1 : 0);
		std::string r = b;
		if (App() != nullptr && App()->mMusicInterface != nullptr)
		{
			r += " music";
			for (int i = 0; i < 5; i++)
				if (App()->mMusicInterface->IsPlaying(i))
					r += " " + std::to_string(i);
		}
		return r;
	}

	bool HeroesTestPick(int theHero)
	{
		if (gH.mPhase != HP_DRAFT || theHero < 0 || theHero >= HERO_COUNT)
			return false;
		gH.mMyHero = theHero;
		StartMatch();
		return true;
	}

	bool HeroesTestLock(int theHero)
	{
		if (gH.mPhase != HP_DRAFT || gH.mPractice || theHero < 0 || theHero >= HERO_COUNT || gH.mMyLocked)
			return false;
		gH.mMyHero = theHero;
		LockIn();
		return true;
	}

	void HeroesTestBot(bool theOn) { gH.mBotMine = theOn; }
	void HeroesTestBotHero(int theHero) { gH.mBotHero = theHero >= 0 && theHero < HERO_COUNT ? theHero : -1; }
	void HeroesTestBotSkill(int theSkill) { gH.mBotSkill = std::clamp(theSkill, 0, 2); }
	void HeroesTestSpeed(int theTicksPerFrame) { gH.mSpeed = std::clamp(theTicksPerFrame, 1, 40); }
	void HeroesTestGiveUp()
	{
		if (Side* s = MySide())
			s->GiveUp();
	}
	void HeroesTestTutorial()
	{
		if (gH.mPhase != HP_DRAFT)
			return;
		if (gH.mMyHero < 0)
			gH.mMyHero = HERO_ITCHY;
		StartTutorial();
	}
	void HeroesTestWarp(int theArena)
	{
		Side* s = MySide();
		if (s == nullptr || theArena < 0 || theArena >= kArenaCount)
			return;
		s->mHero.mArena = theArena;
		const HeroDef& d = s->mHero.Def();
		Vec p = theArena == kTrench ? TrenchMap().mGateExit[s->mTeam] + Vec(s->mTeam == 0 ? 200.0f : -200.0f, 0) : TankMap().mPortalExit + Vec(0, 120);
		s->mHero.mPos = d.mWalker ? WalkerPos(theArena, p.x, d.mRadius) : p;
		s->OrderStop();
	}
	void HeroesTestLevel(int theLevel)
	{
		Side* s = MySide();
		if (s == nullptr)
			return;
		while (s->mHero.mLevel < std::min(theLevel, kMaxLevel))
			s->TestXp((float)XpForLevel(s->mHero.mLevel) + 1);
	}
	void HeroesTestAim(int theSlot) { gH.mTestAim = theSlot >= 0 && theSlot < AB_COUNT ? theSlot : -1; }
}
