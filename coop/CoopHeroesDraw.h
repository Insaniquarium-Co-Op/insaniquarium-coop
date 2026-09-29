// Insaniquarium Co-op - Pet Heroes drawing: the 2x arena drawn at half scale with the
// game's art, the HUD, the shop, the draft and the result screens. Screen layout
// (640x480): a 12 px top strip, the tank (world 1280x848 at 0.5 scale), a 44 px HUD.

#ifndef __COOP_HEROES_DRAW_H__
#define __COOP_HEROES_DRAW_H__

#include "HeroesSide.h"
#include <string>

namespace Sexy { class Graphics; class Font; class Image; }

namespace Coop
{
	namespace HV
	{
		static const float	kScale = 0.5f;
		static const int	kTop = 12, kHudY = 436, kScreenW = 640, kScreenH = 480;

		inline float		SX(float x) { return x * kScale; }
		inline float		SY(float y) { return kTop + y * kScale; }
		inline Heroes::Vec	ToWorld(int x, int y) { return Heroes::Vec(x / kScale, (y - kTop) / kScale); }
		inline bool			InTank(int x, int y) { return y >= kTop && y < kHudY; }

		// What the screen tells the drawing code.
		struct ViewState
		{
			const Heroes::Side*	mSide = nullptr;
			int			mArena = 0;			// the tank on screen
			uint32_t	mNow = 0;
			int			mMouseX = -1, mMouseY = -1;
			int			mShopTab = -1;		// -1: shop closed
			int			mHoverShop = -1;
			std::string	mNote;				// a message on the HUD
			uint32_t	mNoteAt = 0;
			bool		mPractice = false;
			std::string	mNames[Heroes::kMaxPlayers];
			int			mAimSlot = -1;		// holding Q/W/E/R: show its reach
		};

		void	DrawTank(Sexy::Graphics* g, const ViewState& v);
		void	DrawTopStrip(Sexy::Graphics* g, const ViewState& v);
		void	DrawHud(Sexy::Graphics* g, const ViewState& v);
		void	DrawShop(Sexy::Graphics* g, const ViewState& v);
		void	DrawFeed(Sexy::Graphics* g, const ViewState& v);	// kill feed and banners

		// Clicks on the HUD: 0-3 abilities, 10-13 quick buy, 20 shop, 30 mini-map; -1 none.
		int		HudHit(int x, int y);
		static const int	kQuickSlots = 4;
		int		QuickShop(int theSlot);			// the shop entry behind quick-buy slot 1-4
		// Clicks on the open shop: a shop entry, or -2 - tab; -1 none; -100 close.
		int		ShopHit(const ViewState& v, int x, int y);

		// Draft: which card (hero) is at x,y (-1 none), and the practice buttons.
		enum DraftButton { DB_NONE = -1, DB_OPPONENT = 100, DB_SKILL, DB_START, DB_BACK };
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
	}
}

#endif
