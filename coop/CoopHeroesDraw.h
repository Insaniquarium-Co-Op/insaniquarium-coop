// Insaniquarium Co-op - Pet Heroes drawing. The world (a tank or the Trench, 1280x848)
// is drawn through a movable transform: the main view at half scale between a 12 px top
// strip and a 44 px HUD, the home window (a small live picture of your tank while your
// hero is away), and the world map. CoopHeroesDraw.cpp draws the world; CoopHeroesHud.cpp
// the HUD, shop, banners, talents, draft, help, tutorial and result screens.

#ifndef __COOP_HEROES_DRAW_H__
#define __COOP_HEROES_DRAW_H__

#include "HeroesSide.h"
#include <string>

namespace Sexy { class Graphics; class Font; class Image; class Color; }

namespace Coop
{
	namespace HV
	{
		static const float	kScale = 0.5f;
		static const int	kTop = 12, kHudY = 436, kScreenW = 640, kScreenH = 480;

		// Where the world goes on screen (changed while drawing the home window or a map).
		struct WorldView { float mScale = kScale; float mX = 0; float mY = (float)kTop; };
		extern WorldView gView;
		inline float		SX(float x) { return gView.mX + x * gView.mScale; }
		inline float		SY(float y) { return gView.mY + y * gView.mScale; }
		// The main view's mapping (clicks).
		inline Heroes::Vec	ToWorld(int x, int y) { return Heroes::Vec(x / kScale, (y - kTop) / kScale); }
		inline bool			InTank(int x, int y) { return y >= kTop && y < kHudY; }

		// The home window: top right while your hero is out of your tank (D36).
		static const int	kHomeX = 394, kHomeY = kTop + 4, kHomeW = 242, kHomeH = 160;
		inline bool			InHome(int x, int y) { return x >= kHomeX && x < kHomeX + kHomeW && y >= kHomeY && y < kHomeY + kHomeH; }
		inline Heroes::Vec	HomeToWorld(int x, int y) { return Heroes::Vec((x - kHomeX) * Heroes::kWorldW / kHomeW, (y - kHomeY) * Heroes::kWorldH / kHomeH); }

		// What the screen tells the drawing code.
		struct ViewState
		{
			const Heroes::Side*	mSide = nullptr;
			int			mArena = 0;			// the arena on screen
			uint32_t	mNow = 0;
			int			mMouseX = -1, mMouseY = -1;
			int			mShopTab = -1;		// -1: shop closed
			std::string	mNote;				// a message on the HUD
			uint32_t	mNoteAt = 0;
			bool		mPractice = false;
			std::string	mNames[Heroes::kMaxPlayers];
			int			mAimSlot = -1;		// aiming an ability (hold Q/E/R/F, or clicked on the HUD)
			bool		mHomeWindow = false;	// draw the home window
			bool		mHomeFaded = false;		// your hero is behind it
			std::string	mAlert;				// home under attack while you look elsewhere
			uint32_t	mAlertAt = 0;
			int			mTalentHover = -1;
			// The guided first match.
			std::string	mTutorial;			// the current step ("" none)
			int			mTutorialStep = 0, mTutorialSteps = 0;
			bool		mTutorialDone = false;
		};
		static const uint32_t	kAlertShowMs = 3000;

		// ---- the world (CoopHeroesDraw.cpp) ----
		void	DrawTank(Sexy::Graphics* g, const ViewState& v);	// the main view (with the home window)
		void	DrawWorld(Sexy::Graphics* g, const ViewState& v, int theArena, bool theFull);	// at gView
		void	DrawAim(Sexy::Graphics* g, const ViewState& v);	// hold-to-aim indicators
		void	DrawHomeWindow(Sexy::Graphics* g, const ViewState& v);
		void	DrawWorldMap(Sexy::Graphics* g, const ViewState& v, int x, int y, int w, int h);

		// ---- the HUD and screens (CoopHeroesHud.cpp) ----
		void	DrawTopStrip(Sexy::Graphics* g, const ViewState& v);
		void	DrawHud(Sexy::Graphics* g, const ViewState& v);
		void	DrawShop(Sexy::Graphics* g, const ViewState& v);
		void	DrawFeed(Sexy::Graphics* g, const ViewState& v);	// kill feed, announcer banners
		void	DrawTalents(Sexy::Graphics* g, const ViewState& v);	// the two cards when a talent waits
		void	DrawDeath(Sexy::Graphics* g, const ViewState& v);	// respawn countdown and the death recap
		void	DrawTutorial(Sexy::Graphics* g, const ViewState& v);

		// Clicks on the HUD: 0-3 abilities, 10-14 quick buy, 20 shop, 30 world map; -1 none.
		int		HudHit(int x, int y);
		static const int	kQuickSlots = 5;		// 1-4: guppy, food, breeder, carnivore; 5: the suggested item
		static const char* const kAbilityKeys[4] = { "Q", "E", "R", "F" };	// keys for ability slots AB_Q..AB_R (WASD moves)
		int		QuickShop(const Heroes::Side& s, int theSlot);	// the shop entry behind quick-buy slot 1-5 (-1: none)
		// Clicks on the open shop: a shop entry, or -2 - tab; -1 none; -100 close.
		int		ShopHit(const ViewState& v, int x, int y);
		int		TalentHit(const ViewState& v, int x, int y);	// 0/1: a talent card; -1 none

		// Draft: which card (hero) is at x,y (-1 none), and the buttons.
		enum DraftButton { DB_NONE = -1, DB_OPPONENT = 100, DB_SKILL, DB_START, DB_BACK, DB_TUTORIAL };
		int		DraftHit(int x, int y, bool thePractice);
		void	DrawDraft(Sexy::Graphics* g, uint32_t theNow, int theHover, int theMine, int theTheirs, bool thePractice,
					int theBotHero, int theBotSkill, const std::string& theStatus);
		void	DrawCountdown(Sexy::Graphics* g, int theSeconds);
		void	DrawHelp(Sexy::Graphics* g, int theHero, bool theWaiting);	// the controls card (H)
		void	DrawResult(Sexy::Graphics* g, const ViewState& v, bool theWon, const std::string& theReason, uint32_t theMatchMs,
					const Heroes::HeroSnap& theOther, int theOtherFishLost, int theOtherEarned);
		int		ResultHit(int x, int y);		// 1: back to the menu

		void	PlaySound(uint8_t theSound);
		const char* HeroName(int theHero);
		Sexy::Image* HeroImage(int theHero);
		Sexy::Image* HeroPortrait(int theHero);

		// ---- shared helpers ----
		void	Sprite(Sexy::Graphics* g, Sexy::Image* theImage, int theCol, int theRow, float wx, float wy, float theScale, bool theMirror,
					const Sexy::Color& theTint, bool theColorize = false);
		void	Sprite(Sexy::Graphics* g, Sexy::Image* theImage, int theCol, int theRow, float wx, float wy, float theScale, bool theMirror);
		void	ScreenSprite(Sexy::Graphics* g, Sexy::Image* theImage, int theCol, int theRow, float sx, float sy, float theScale, bool theMirror,
					const Sexy::Color& theTint, bool theColorize = false);
		void	ScreenSprite(Sexy::Graphics* g, Sexy::Image* theImage, int theCol, int theRow, float sx, float sy, float theScale, bool theMirror);
		void	Disc(Sexy::Graphics* g, float cx, float cy, float r, const Sexy::Color& c, int theSides = 20);
		void	Ring(Sexy::Graphics* g, float cx, float cy, float r, const Sexy::Color& c, int theThick = 1);
		void	Bar(Sexy::Graphics* g, int x, int y, int w, int h, float theFrac, const Sexy::Color& theFill);
		void	Bar(Sexy::Graphics* g, int x, int y, int w, int h, float theFrac, const Sexy::Color& theFill, const Sexy::Color& theBack);
		void	Text(Sexy::Graphics* g, Sexy::Font* f, const std::string& s, int x, int y, const Sexy::Color& c);
		void	Centered(Sexy::Graphics* g, Sexy::Font* f, const std::string& s, int cx, int y, const Sexy::Color& c);
		int		Wrapped(Sexy::Graphics* g, Sexy::Font* f, const std::string& s, int x, int y, int w, const Sexy::Color& c);
		std::string Clock(uint32_t theMs);
		const Sexy::Color& TeamColor(int theTeam);
		void	AbilityIcon(Sexy::Graphics* g, int theHero, int theSlot, float cx, float cy, float theSize, uint32_t theNow);
		void	ItemIcon(Sexy::Graphics* g, int theItem, float cx, float cy, float theSize);
		Sexy::Image* MinionImage(int theKind);
	}
}

#endif
