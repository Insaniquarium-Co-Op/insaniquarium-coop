// Insaniquarium Co-op - Pet Heroes: the HUD, the shop, banners, talents, the death recap,
// the tutorial panel, and the draft, help and result screens (see CoopHeroesDraw.h).

#include <SexyAppFramework/Font.h>
#include "CoopHeroesDraw.h"
#include "HeroesMap.h"
#include "WinFishApp.h"
#include "Res.h"
#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/Image.h>
#include <cmath>
#include <cstdio>
#include <map>

using namespace Sexy;
using namespace Heroes;

namespace Coop
{
	namespace HV
	{
		static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }

		///////////////////////////////////////////////////////////////////////
		// Top strip
		///////////////////////////////////////////////////////////////////////
		static void StructurePips(Graphics* g, int x, int y, const ArenaSnap* s, const Color& c, bool theMirror)
		{
			if (s == nullptr)
				return;
			for (int i = 0; i < 2; i++)
			{
				int bx = theMirror ? x - 30 * (i + 1) : x + 30 * i;
				Bar(g, bx, y, 26, 3, s->mTowerHp[i] / std::max(1.0f, s->mTowerMax), s->mTowerHp[i] > 0 ? c : Color(90, 90, 90));
			}
			int cx = theMirror ? x - 60 - 46 : x + 60;
			Bar(g, cx, y, 44, 3, s->mCoreHp / kCoreHealth, Color(255, 200, 80));
		}

		void DrawTopStrip(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			g->SetColor(Color(8, 14, 30, 235));
			g->FillRect(0, 0, kScreenW, kTop);
			ArenaSnap aMine = s.mArena.Snapshot();
			const ArenaSnap* aTheirs = s.LatestArena(1 - s.mTeam);
			Text(g, FONT_TINY, "YOU", 3, 9, TeamColor(s.mTeam));
			StructurePips(g, 22, 4, &aMine, TeamColor(s.mTeam), false);
			Text(g, FONT_TINY, "THEM", kScreenW - 28, 9, TeamColor(1 - s.mTeam));
			StructurePips(g, kScreenW - 31, 4, aTheirs, TeamColor(1 - s.mTeam), true);
			uint32_t ms = s.MatchMs();
			bool aSudden = s.mSuddenDeath;
			std::string aClock = Clock(ms) + (aSudden ? " SUDDEN DEATH" : "");
			const HeroSnap* o = s.OtherHero();
			if (o != nullptr)
				aClock = std::to_string(s.mHero.mKills) + "-" + std::to_string(o->mKills) + "  " + aClock;
			Centered(g, FONT_TINYBOLD, aClock, kScreenW / 2 - 30, 9, aSudden ? Color(255, 110, 90) : Color(230, 235, 255));
			// The big monsters' timers.
			std::string aObj;
			static const char* kShort[MON_COUNT] = { "Gus", "Balrog", "Squid", "Boss" };
			for (int sl = MON_SQUID; sl <= MON_BOSS; sl++)
			{
				uint32_t aIn = s.MonsterIn(sl);
				aObj += std::string(aObj.empty() ? "" : "  ") + kShort[sl] + " " + (aIn == 0 ? std::string("UP") : Clock(aIn + 999));
			}
			Centered(g, FONT_TINY, aObj, kScreenW / 2 + 85, 9, Color(210, 170, 255));
			const char* aWhere = v.mArena == kTrench ? "TRENCH" : (v.mArena == s.mTeam ? (s.mHero.mArena != s.mTeam ? "HOME (Tab)" : "") : "RIVAL'S TANK");
			Text(g, FONT_TINY, aWhere, 132, 9, v.mArena == kTrench ? Color(210, 170, 255) : (v.mArena == s.mTeam ? Color(150, 255, 170) : Color(255, 170, 140)));
		}

		///////////////////////////////////////////////////////////////////////
		// HUD
		///////////////////////////////////////////////////////////////////////
		int QuickShop(const Side& s, int theSlot)
		{
			static const int kQuick[4] = { SHOP_GUPPY, SHOP_FOOD_COUNT, SHOP_BREEDER, SHOP_CARNIVORE };
			if (theSlot < 4)
				return kQuick[std::clamp(theSlot, 0, 3)];
			int aItem = s.SuggestedItem();
			return aItem == ITEM_NONE ? -1 : SHOP_ITEM_FIRST + aItem;
		}

		static Rect AbilityRect(int i) { return Rect(196 + i * 37, kHudY + 5, 34, 34); }
		static Rect ItemRect(int i) { return Rect(347 + (i % 3) * 19, kHudY + 6 + (i / 3) * 19, 17, 17); }
		static Rect QuickRect(int i) { return Rect(406 + i * 27, kHudY + 4, 25, 36); }
		static Rect ShopButtonRect() { return Rect(542, kHudY + 4, 26, 36); }
		static Rect MapRect() { return Rect(570, kHudY + 4, 68, 36); }

		int HudHit(int x, int y)
		{
			if (y < kHudY)
				return -1;
			for (int i = 0; i < AB_COUNT; i++)
				if (AbilityRect(i).Contains(x, y))
					return i;
			for (int i = 0; i < kQuickSlots; i++)
				if (QuickRect(i).Contains(x, y))
					return 10 + i;
			if (ShopButtonRect().Contains(x, y))
				return 20;
			if (MapRect().Contains(x, y))
				return 30;
			return -1;
		}

		void ItemIcon(Graphics* g, int theItem, float cx, float cy, float theSize)
		{
			float sc = theSize / 72.0f;
			switch (theItem)
			{
			case ITEM_SHARP_FIN: ScreenSprite(g, IMAGE_ITCHY, 0, 0, cx, cy, theSize / 64, false); break;
			case ITEM_THICK_SHELL: ScreenSprite(g, IMAGE_SHELLS, 0, 0, cx, cy, theSize / 30, false); break;
			case ITEM_SPEED_KELP: ScreenSprite(g, IMAGE_SHELLS, 0, 2, cx, cy, theSize / 30, false); break;
			case ITEM_PEARL_CHARM: ScreenSprite(g, IMAGE_PEARL, 0, 0, cx, cy, sc, false); break;
			case ITEM_ELECTRIC_SCALE: ScreenSprite(g, IMAGE_ENERGYBALL, 2, 0, cx, cy, theSize / 60, false, Color(120, 230, 255), true); break;
			case ITEM_INK_SAC: Disc(g, cx, cy, theSize * 0.4f, Color(30, 20, 50), 12); Disc(g, cx - theSize * 0.12f, cy - theSize * 0.12f, theSize * 0.12f, Color(120, 100, 160), 8); break;
			case ITEM_KRAKEN_TOOTH: ScreenSprite(g, IMAGE_SHELLS, 0, 3, cx, cy, theSize / 30, false, Color(200, 120, 255), true); break;
			case ITEM_LEECH_TOOTH: ScreenSprite(g, IMAGE_SHELLS, 0, 3, cx, cy, theSize / 30, false, Color(255, 120, 120), true); break;
			case ITEM_CORAL_ARMOR: ScreenSprite(g, IMAGE_SHELLS, 0, 1, cx, cy, theSize / 30, false, Color(255, 150, 170), true); break;
			case ITEM_TOWER_BUSTER: ScreenSprite(g, IMAGE_EXPLOSION, 5, 0, cx, cy, theSize / 70, false); break;
			case ITEM_SEA_CROWN:
			{
				float k = theSize / 2;
				Point q[7] = { Point((int)(cx - k * 0.8f), (int)(cy + k * 0.5f)), Point((int)(cx - k * 0.8f), (int)(cy - k * 0.5f)), Point((int)(cx - k * 0.4f), (int)(cy - k * 0.05f)),
					Point((int)cx, (int)(cy - k * 0.7f)), Point((int)(cx + k * 0.4f), (int)(cy - k * 0.05f)), Point((int)(cx + k * 0.8f), (int)(cy - k * 0.5f)), Point((int)(cx + k * 0.8f), (int)(cy + k * 0.5f)) };
				g->SetColor(Color(255, 200, 60));
				g->PolyFill(q, 7, false);
				Disc(g, cx, cy + k * 0.15f, k * 0.22f, Color(255, 240, 250), 8);
				break;
			}
			default: ScreenSprite(g, IMAGE_MONEY, 0, 1, cx, cy, sc * 1.1f, false); break;
			}
		}

		static void ShopIcon(Graphics* g, int theShop, float cx, float cy, float theSize, uint32_t theNow, int theFoodRow = 1)
		{
			int f = (int)((theNow / 90) % 10);
			switch (theShop)
			{
			case SHOP_GUPPY: ScreenSprite(g, IMAGE_SMALLSWIM, f, 1, cx, cy, theSize / 60, false); break;
			case SHOP_BREEDER: ScreenSprite(g, IMAGE_BREEDER, f, 3, cx, cy, theSize / 60, false); break;
			case SHOP_CARNIVORE: ScreenSprite(g, IMAGE_SMALLSWIM, f, 4, cx, cy, theSize / 64, false); break;
			case SHOP_FOOD_QUALITY: ScreenSprite(g, IMAGE_FOOD, f, theFoodRow, cx, cy, theSize / 34, false); break;
			case SHOP_FOOD_COUNT: ScreenSprite(g, IMAGE_FOOD, f, 0, cx, cy, theSize / 34, false); break;
			case SHOP_COLLECTOR: ScreenSprite(g, IMAGE_STINKY, f, 0, cx, cy, theSize / 60, false); break;
			case SHOP_LASER: ScreenSprite(g, IMAGE_ENERGYBALL, f % 6, 0, cx, cy, theSize / 70, false, Color(255, 120, 90), true); break;
			case SHOP_REPAIR_LEFT: case SHOP_REPAIR_RIGHT: case SHOP_TOWER_UPGRADE: ScreenSprite(g, IMAGE_NIKO, 0, theShop == SHOP_TOWER_UPGRADE ? 1 : 0, cx, cy, theSize / 64, false); break;
			case SHOP_WAVE_SIZE: case SHOP_WAVE_TOUGH: ScreenSprite(g, IMAGE_MINISYLV, f, 0, cx, cy, theSize / 64, false, theShop == SHOP_WAVE_TOUGH ? Color(255, 160, 140) : Color(255, 255, 255), theShop == SHOP_WAVE_TOUGH); break;
			case SHOP_SEND_SYLV: ScreenSprite(g, IMAGE_SYLV, f, 0, cx, cy, theSize / 120, false); break;
			case SHOP_SEND_GUS: ScreenSprite(g, IMAGE_GUS, f, 0, cx, cy, theSize / 120, false); break;
			case SHOP_SEND_BALROG: ScreenSprite(g, IMAGE_BALROG, f, 0, cx, cy, theSize / 120, false); break;
			case SHOP_SEND_DESTRUCTOR: ScreenSprite(g, IMAGE_DESTRUCTOR, f, 0, cx, cy, theSize / 120, false); break;
			default:
				if (theShop >= SHOP_ITEM_FIRST && theShop <= SHOP_ITEM_LAST)
					ItemIcon(g, theShop - SHOP_ITEM_FIRST, cx, cy, theSize);
				break;
			}
		}

		static void BoltShape(Graphics* g, float cx, float cy, float theSize, const Color& c)
		{
			Point p[6] = { Point((int)(cx + theSize * 0.15f), (int)(cy - theSize * 0.5f)), Point((int)(cx - theSize * 0.3f), (int)(cy + theSize * 0.05f)),
				Point((int)(cx - theSize * 0.02f), (int)(cy + theSize * 0.05f)), Point((int)(cx - theSize * 0.15f), (int)(cy + theSize * 0.5f)),
				Point((int)(cx + theSize * 0.3f), (int)(cy - theSize * 0.08f)), Point((int)(cx + theSize * 0.02f), (int)(cy - theSize * 0.08f)) };
			g->SetColor(c);
			g->PolyFill(p, 6, false);
		}

		static void CardShape(Graphics* g, float cx, float cy, float w, float h, float theTilt)
		{
			float c = std::cos(theTilt), s = std::sin(theTilt);
			auto P = [&](float x, float y) { return Point((int)(cx + x * c - y * s), (int)(cy + x * s + y * c)); };
			Point q[4] = { P(-w / 2, -h / 2), P(w / 2, -h / 2), P(w / 2, h / 2), P(-w / 2, h / 2) };
			g->SetColor(Color(250, 250, 250));
			g->PolyFill(q, 4, true);
			Disc(g, cx, cy, w * 0.22f, Color(220, 30, 50), 8);
		}

		// A picture for each ability, from the game's own art.
		void AbilityIcon(Graphics* g, int theHero, int theSlot, float cx, float cy, float theSize, uint32_t theNow)
		{
			int f = (int)((theNow / 90) % 10);
			float k = theSize / 32.0f;
			switch (theHero * AB_COUNT + theSlot)
			{
			case HERO_ITCHY * AB_COUNT + AB_Q: ScreenSprite(g, IMAGE_ITCHY, f, 0, cx, cy, 0.42f * k, true); BoltShape(g, cx - 8 * k, cy, 10 * k, Color(255, 255, 255, 160)); break;
			case HERO_ITCHY * AB_COUNT + AB_W: ScreenSprite(g, IMAGE_ITCHY, 0, 0, cx, cy, 0.38f * k, true); ScreenSprite(g, IMAGE_SPARKLE, f % 10, 0, cx + 7 * k, cy - 5 * k, 1.1f * k, false); break;
			case HERO_ITCHY * AB_COUNT + AB_E: ScreenSprite(g, IMAGE_BUBBLES, f % 5, 0, cx, cy, 1.0f * k, false); ScreenSprite(g, IMAGE_ITCHY, f, 0, cx, cy, 0.3f * k, true, Color(255, 255, 255, 120)); break;
			case HERO_ITCHY * AB_COUNT + AB_R: Ring(g, cx, cy, 11 * k, Color(220, 240, 255), 2); ScreenSprite(g, IMAGE_ITCHY, 5, 1, cx, cy, 0.34f * k, false); break;
			case HERO_CLYDE * AB_COUNT + AB_Q: ScreenSprite(g, IMAGE_ENERGYBALL, f % 6, 0, cx, cy, 0.34f * k, false, Color(120, 220, 255), true); break;
			case HERO_CLYDE * AB_COUNT + AB_W: Disc(g, cx, cy, 11 * k, Color(120, 220, 255, 80)); Ring(g, cx, cy, 11 * k, Color(150, 230, 255), 1); BoltShape(g, cx, cy, 14 * k, Color(200, 240, 255)); break;
			case HERO_CLYDE * AB_COUNT + AB_E: ScreenSprite(g, IMAGE_WARPHOLE, (theNow / 70) % 17, 0, cx, cy, 0.13f * k, false); ScreenSprite(g, IMAGE_CLYDE, f, 0, cx + 4 * k, cy, 0.25f * k, false, Color(255, 255, 255, 170)); break;
			case HERO_CLYDE * AB_COUNT + AB_R: BoltShape(g, cx - 6 * k, cy, 16 * k, Color(255, 240, 120)); BoltShape(g, cx + 6 * k, cy + 2 * k, 14 * k, Color(170, 230, 255)); break;
			case HERO_RHUBARB * AB_COUNT + AB_Q: ScreenSprite(g, IMAGE_RHUBARB, f, 0, cx, cy - 3 * k, 0.36f * k, false); Ring(g, cx, cy + 10 * k, 8 * k, Color(230, 200, 150), 1); break;
			case HERO_RHUBARB * AB_COUNT + AB_W: ScreenSprite(g, IMAGE_RHUBARB, 5, 1, cx, cy, 0.4f * k, false); break;
			case HERO_RHUBARB * AB_COUNT + AB_E: ScreenSprite(g, IMAGE_SHELLS, (theNow / 60) % 20, 1, cx, cy, 0.8f * k, false); break;
			case HERO_RHUBARB * AB_COUNT + AB_R: ScreenSprite(g, IMAGE_EXPLOSION, 4, 0, cx, cy, 0.4f * k, false, Color(255, 220, 160), true); break;
			case HERO_ANGIE * AB_COUNT + AB_Q: Disc(g, cx, cy, 9 * k, Color(255, 245, 170, 120)); g->SetColor(Color(255, 255, 255)); g->FillRect((int)(cx - 2 * k), (int)(cy - 8 * k), (int)(4 * k), (int)(16 * k)); g->FillRect((int)(cx - 8 * k), (int)(cy - 2 * k), (int)(16 * k), (int)(4 * k)); break;
			case HERO_ANGIE * AB_COUNT + AB_W: ScreenSprite(g, IMAGE_HALO, f, 0, cx, cy - 4 * k, 1.1f * k, false); Ring(g, cx, cy + 2 * k, 10 * k, Color(255, 245, 170), 1); break;
			case HERO_ANGIE * AB_COUNT + AB_E: ScreenSprite(g, IMAGE_MINISYLV, f, 0, cx, cy, 0.34f * k, false, Color(255, 150, 220), true); break;
			case HERO_ANGIE * AB_COUNT + AB_R: ScreenSprite(g, IMAGE_ANGIE, f, 0, cx, cy + 2 * k, 0.32f * k, false); ScreenSprite(g, IMAGE_HALO, f, 0, cx, cy - 10 * k, 0.9f * k, false); break;
			case HERO_SPEEDY * AB_COUNT + AB_Q: Disc(g, cx, cy + 5 * k, 10 * k, Color(120, 220, 70, 170), 14); ScreenSprite(g, HeroImage(HERO_SPEEDY), f, 0, cx, cy - 3 * k, 0.28f * k, false); break;
			case HERO_SPEEDY * AB_COUNT + AB_W: ScreenSprite(g, IMAGE_SMOKESMALL, f, 0, cx, cy, 0.55f * k, false, Color(140, 220, 80), true); break;
			case HERO_SPEEDY * AB_COUNT + AB_E: ScreenSprite(g, HeroImage(HERO_SPEEDY), 9, 2, cx, cy, 0.4f * k, false); break;
			case HERO_SPEEDY * AB_COUNT + AB_R: ScreenSprite(g, IMAGE_MONEY, f, 1, cx, cy, 0.42f * k, false); break;
			case HERO_PRESTO * AB_COUNT + AB_Q: CardShape(g, cx - 6 * k, cy + 1 * k, 9 * k, 13 * k, -0.4f); CardShape(g, cx, cy, 9 * k, 13 * k, 0); CardShape(g, cx + 6 * k, cy + 1 * k, 9 * k, 13 * k, 0.4f); break;
			case HERO_PRESTO * AB_COUNT + AB_W: ScreenSprite(g, IMAGE_PRESTO, 0, 0, cx - 5 * k, cy, 0.22f * k, false); ScreenSprite(g, IMAGE_MINISYLV, 0, 0, cx + 7 * k, cy, 0.22f * k, false); ScreenSprite(g, IMAGE_SPARKLE, f, 0, cx, cy - 6 * k, 0.9f * k, false); break;
			case HERO_PRESTO * AB_COUNT + AB_E: ScreenSprite(g, IMAGE_PRESTO, 5, 2, cx, cy, 0.34f * k, false); break;
			case HERO_PRESTO * AB_COUNT + AB_R: ScreenSprite(g, IMAGE_PRESTO, f, 0, cx, cy, 0.34f * k, false, Color(255, 200, 255), true); Centered(g, FONT_TINYBOLD, "?", (int)(cx + 8 * k), (int)(cy - 4 * k), Color(255, 240, 120)); break;
			case HERO_NIKO * AB_COUNT + AB_Q: ScreenSprite(g, IMAGE_NIKO, 9, 2, cx, cy, 0.32f * k, false); break;
			case HERO_NIKO * AB_COUNT + AB_W: ScreenSprite(g, IMAGE_NIKO, 0, 0, cx, cy, 0.36f * k, false); Ring(g, cx, cy, 12 * k, Color(240, 200, 255), 2); break;
			case HERO_NIKO * AB_COUNT + AB_E: ScreenSprite(g, IMAGE_PEARL, 0, 0, cx, cy, 0.9f * k, false); break;
			case HERO_NIKO * AB_COUNT + AB_R: ScreenSprite(g, IMAGE_NIKO, 9, 1, cx, cy, 0.42f * k, false); break;
			case HERO_MERYL * AB_COUNT + AB_Q: ScreenSprite(g, IMAGE_MERYL, 4, 2, cx, cy, 0.32f * k, false); Text(g, FONT_TINYBOLD, "z", (int)(cx + 7 * k), (int)(cy - 5 * k), Color(200, 210, 255)); break;
			case HERO_MERYL * AB_COUNT + AB_W: Text(g, FONT_JUNGLEFEVER12OUTLINE, "#", (int)(cx - 9 * k), (int)(cy + 5 * k), Color(255, 160, 240)); Text(g, FONT_JUNGLEFEVER12OUTLINE, "~", (int)(cx + 1 * k), (int)(cy + 1 * k), Color(255, 220, 120)); break;
			case HERO_MERYL * AB_COUNT + AB_E: Ring(g, cx - 4 * k, cy, 6 * k, Color(255, 170, 240), 2); Ring(g, cx + 3 * k, cy, 9 * k, Color(255, 170, 240, 150), 1); break;
			case HERO_MERYL * AB_COUNT + AB_R: ScreenSprite(g, IMAGE_MERYLLIPS, f % 3, 0, cx, cy, 0.6f * k, false); break;
			case HERO_SHRAPNEL * AB_COUNT + AB_Q: Disc(g, cx, cy + 2 * k, 8 * k, Color(40, 40, 50), 12); Disc(g, cx + 4 * k, cy - 7 * k, 3 * k, Color(255, 200, 60), 6); break;
			case HERO_SHRAPNEL * AB_COUNT + AB_W: Disc(g, cx, cy, 7 * k, Color(70, 70, 80), 12); Disc(g, cx, cy - 2 * k, 2 * k, Color(255, 60, 40), 6); Ring(g, cx, cy, 10 * k, Color(120, 120, 130), 1); break;
			case HERO_SHRAPNEL * AB_COUNT + AB_E: ScreenSprite(g, IMAGE_EXPLOSION, 3, 0, cx, cy + 4 * k, 0.3f * k, false); ScreenSprite(g, IMAGE_SHRAPNEL, 0, 0, cx, cy - 4 * k, 0.22f * k, false); break;
			case HERO_SHRAPNEL * AB_COUNT + AB_R: ScreenSprite(g, IMAGE_MISSILE, f, 0, cx, cy, 0.42f * k, false); break;
			default: ScreenSprite(g, IMAGE_MONEY, f, 1, cx, cy, 0.42f * k, false); break;
			}
		}

		// When each ability last came off cooldown (the ready pulse).
		static uint32_t sReadyAt[AB_COUNT] = { 0, 0, 0, 0 };
		static uint32_t sWasLeft[AB_COUNT] = { 0, 0, 0, 0 };

		void DrawHud(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			const HeroState& h = s.mHero;
			const HeroDef& d = h.Def();
			g->SetColor(Color(12, 20, 42, 245));
			g->FillRect(0, kHudY, kScreenW, kScreenH - kHudY);
			g->SetColor(Color(90, 130, 190, 200));
			g->FillRect(0, kHudY, kScreenW, 1);

			// Portrait (evolved: a golden frame), level, XP.
			if (h.Evolved())
			{
				g->SetColor(Color(255, 205, 80, 160));
				g->FillRect(1, kHudY + 2, 44, 41);
			}
			ScreenSprite(g, HeroPortrait(h.Look()), (v.mNow / 100) % 10, 0, 22, kHudY + 22, 0.66f, false);
			Disc(g, 38, kHudY + 36, 7, h.Evolved() ? Color(90, 50, 10, 240) : Color(20, 20, 50, 240), 12);
			Centered(g, FONT_TINYBOLD, std::to_string(h.mLevel), 38, kHudY + 40, Color(255, 235, 150));
			float aMax = s.MaxHp();
			Bar(g, 48, kHudY + 6, 142, 9, h.mHp / std::max(1.0f, aMax), h.mHp < aMax * 0.3f ? Color(240, 90, 60) : Color(90, 220, 100));
			char b[64];
			snprintf(b, sizeof(b), "%d / %d", (int)std::max(0.0f, h.mHp), (int)aMax);
			Centered(g, FONT_CONTINUUMBOLD12 ? FONT_CONTINUUMBOLD12 : FONT_TINY, b, 119, kHudY + 15, Color(255, 255, 255));
			float aXpFrac = h.mLevel >= kMaxLevel ? 1.0f : h.mXp / XpForLevel(h.mLevel);
			Bar(g, 48, kHudY + 19, 142, 3, aXpFrac, Color(170, 130, 255));
			ScreenSprite(g, IMAGE_MONEY, (v.mNow / 90) % 10, 1, 56, kHudY + 32, 0.26f, false);
			snprintf(b, sizeof(b), "%d", s.mArena.mMoney);
			Text(g, FONT_CONTINUUMBOLD12OUTLINE ? FONT_CONTINUUMBOLD12OUTLINE : FONT_TINYBOLD, b, 66, kHudY + 38, Color(255, 225, 90));
			snprintf(b, sizeof(b), "%d/%d", h.mKills, h.mDeaths);
			Text(g, FONT_TINY, b, 166, kHudY + 38, Color(220, 220, 240));
			uint32_t aSick = s.WarpSicknessLeft();
			if (aSick > 0)
			{
				snprintf(b, sizeof(b), "stuck %ds", (int)std::ceil(aSick / 1000.0f));
				Text(g, FONT_TINY, b, 112, kHudY + 30, Color(200, 160, 255));
			}
			if (d.mWalker && (int32_t)(v.mNow - h.mHopReadyAt) < 0)
			{
				snprintf(b, sizeof(b), "hop %ds", (int)std::ceil((h.mHopReadyAt - v.mNow) / 1000.0f));
				Text(g, FONT_TINY, b, 112, kHudY + 40, Color(160, 220, 255));
			}

			// Buffs, just above the HUD on the left.
			{
				int bx = 4;
				auto Buff = [&](Image* theImg, int theRow, float theScale, int theSecs, const Color& c) {
					g->SetColor(Color(0, 0, 0, 160));
					g->FillRect(bx, kHudY - 20, 40, 18);
					ScreenSprite(g, theImg, (v.mNow / 90) % 10, theRow, bx + 10.0f, kHudY - 11.0f, theScale, false);
					Text(g, FONT_TINYBOLD, std::to_string(theSecs) + "s", bx + 20, kHudY - 7, c);
					bx += 43;
				};
				HeroSnap aMe = s.MySnap();
				if (aMe.mBuffS[BUFF_BOSS] > 0) Buff(IMAGE_BOSS, 0, 0.1f, aMe.mBuffS[BUFF_BOSS], Color(255, 120, 80));
				if (aMe.mBuffS[BUFF_GUS] > 0) Buff(IMAGE_GUS, 0, 0.12f, aMe.mBuffS[BUFF_GUS], Color(160, 255, 160));
				if (aMe.mBuffS[BUFF_BALROG] > 0) Buff(IMAGE_BALROG, 0, 0.12f, aMe.mBuffS[BUFF_BALROG], Color(255, 170, 80));
				if (h.mCopyHero >= 0) Buff(HeroImage(h.mCopyHero), 0, 0.14f, (int)((h.mCopyUntil - v.mNow + 999) / 1000), Color(255, 200, 255));
			}

			// Abilities: icons, cooldown sweeps, ranks, a pulse when one is ready again.
			for (int i = 0; i < AB_COUNT; i++)
			{
				Rect r = AbilityRect(i);
				bool aLocked = h.mRank[i] <= 0 && !(i == AB_R && h.mCopyHero >= 0);
				uint32_t aLeft = s.CooldownLeft(i);
				if (sWasLeft[i] > 0 && aLeft == 0)
					sReadyAt[i] = v.mNow;
				sWasLeft[i] = aLeft;
				g->SetColor(aLocked ? Color(30, 30, 40) : (v.mAimSlot == i ? Color(70, 100, 150) : Color(34, 60, 100)));
				g->FillRect(r);
				AbilityIcon(g, h.Look(), i, r.mX + 17.0f, r.mY + 17.0f, 32, v.mNow);
				if (aLeft > 0 && !aLocked)
				{
					float aTotal = (float)s.CooldownTotal(i);
					int aH = (int)(r.mHeight * std::min(1.0f, aLeft / std::max(1.0f, aTotal)));
					g->SetColor(Color(0, 0, 0, 170));
					g->FillRect(r.mX, r.mY + r.mHeight - aH, r.mWidth, aH);
					Centered(g, FONT_TINYBOLD, std::to_string((int)std::ceil(aLeft / 1000.0f)), r.mX + 17, r.mY + 22, Color(255, 255, 255));
				}
				if (aLocked)
				{
					g->SetColor(Color(0, 0, 0, 140));
					g->FillRect(r);
					Centered(g, FONT_TINY, i == AB_R ? "Lv6" : "Lv?", r.mX + 17, r.mY + 22, Color(180, 170, 200));
				}
				float aPulse = (v.mNow - sReadyAt[i]) / 350.0f;
				if (sReadyAt[i] != 0 && aPulse < 1)
				{
					g->SetColor(Color(255, 255, 255, (int)(180 * (1 - aPulse))));
					g->FillRect(r);
				}
				g->SetColor(v.mAimSlot == i ? Color(255, 255, 255) : (i == AB_R && h.Evolved() && !aLocked ? Color(255, 205, 80) : Color(120, 160, 220)));
				g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
				Text(g, FONT_TINYBOLD, kAbilityKeys[i], r.mX + 2, r.mY + 9, Color(255, 240, 160));
				int aMaxRank = i == AB_R ? 2 : kMaxRank;
				for (int k = 0; k < aMaxRank; k++)
				{
					g->SetColor(k < h.mRank[i] ? Color(255, 220, 90) : Color(60, 60, 80));
					g->FillRect(r.mX + 3 + k * 7, r.mY + r.mHeight - 4, 5, 2);
				}
			}
			// Items.
			for (int i = 0; i < kItemSlots; i++)
			{
				Rect r = ItemRect(i);
				g->SetColor(Color(28, 36, 60));
				g->FillRect(r);
				g->SetColor(Color(90, 110, 150));
				g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
				if (h.mItems[i] != ITEM_NONE)
					ItemIcon(g, h.mItems[i], r.mX + 8.5f, r.mY + 8.5f, 15);
			}
			// Quick-buy (5: the suggested item) and the shop button.
			for (int i = 0; i < kQuickSlots; i++)
			{
				Rect r = QuickRect(i);
				int aShop = QuickShop(s, i);
				bool aCan = aShop >= 0 && s.CanBuy(aShop);
				g->SetColor(aCan ? (i == 4 ? Color(70, 60, 20) : Color(40, 70, 50)) : Color(40, 36, 44));
				g->FillRect(r);
				if (i == 4)
				{
					g->SetColor(Color(255, 205, 80, 200));
					g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
				}
				if (aShop >= 0)
				{
					ShopIcon(g, aShop, r.mX + 12.5f, r.mY + 15.0f, 20, v.mNow, std::min(s.mArena.mFoodQuality + 1, 2));
					Centered(g, FONT_TINY, std::to_string(s.Price(aShop)), r.mX + 13, r.mY + 34, aCan ? Color(255, 225, 90) : Color(150, 140, 150));
				}
				else
					Centered(g, FONT_TINY, "done", r.mX + 13, r.mY + 24, Color(150, 140, 150));
				Text(g, FONT_TINY, std::to_string(i + 1), r.mX + 2, r.mY + 8, Color(255, 240, 160));
			}
			Rect sb = ShopButtonRect();
			g->SetColor(v.mShopTab >= 0 ? Color(90, 70, 30) : Color(60, 50, 24));
			g->FillRect(sb);
			ScreenSprite(g, IMAGE_MONEY, (v.mNow / 90) % 10, 4, sb.mX + 13.0f, sb.mY + 15.0f, 0.3f, false);
			Centered(g, FONT_TINYBOLD, "B", sb.mX + 13, sb.mY + 34, Color(255, 240, 160));
			// The world map: home, the Trench, the rival's tank.
			Rect mr = MapRect();
			DrawWorldMap(g, v, mr.mX, mr.mY, mr.mWidth, mr.mHeight);

			// Home under attack while you're looking elsewhere: a red banner.
			bool aAlert = !v.mAlert.empty() && v.mArena != s.mTeam && !Elapsed(v.mNow, v.mAlertAt + kAlertShowMs);
			if (aAlert && !v.mHomeWindow)
			{
				Font* f = FONT_JUNGLEFEVER12OUTLINE;
				int w = f != nullptr ? f->StringWidth(v.mAlert) : 200;
				int a = (v.mNow / 250) % 2 == 0 ? 235 : 200;
				g->SetColor(Color(150, 20, 20, a));
				g->FillRect(kScreenW / 2 - w / 2 - 12, kTop + 34, w + 24, 24);
				g->SetColor(Color(255, 150, 130, 230));
				g->DrawRect(kScreenW / 2 - w / 2 - 12, kTop + 34, w + 23, 23);
				Centered(g, f, v.mAlert, kScreenW / 2, kTop + 52, Color(255, 245, 235));
			}

			// A note (why something was refused, a tip...).
			if (!v.mNote.empty() && !Elapsed(v.mNow, v.mNoteAt + 2500))
			{
				Font* f = FONT_JUNGLEFEVER10OUTLINE;
				int w = f != nullptr ? f->StringWidth(v.mNote) : 100;
				g->SetColor(Color(0, 0, 0, 170));
				g->FillRect(kScreenW / 2 - w / 2 - 8, kHudY - 22, w + 16, 18);
				Centered(g, f, v.mNote, kScreenW / 2, kHudY - 8, Color(255, 230, 150));
			}
			// Hovering an ability or an item: what it does.
			std::string aTip;
			for (int i = 0; i < AB_COUNT; i++)
				if (AbilityRect(i).Contains(v.mMouseX, v.mMouseY))
				{
					const AbilityDef& ab = d.mAb[i];
					aTip = std::string(kAbilityKeys[i]) + " " + ab.mName + ": " + ab.mDesc + (i == AB_R && h.mCopyHero >= 0 ? " (F now ends Copycat.)" : "");
				}
			for (int i = 0; i < kItemSlots; i++)
				if (ItemRect(i).Contains(v.mMouseX, v.mMouseY) && h.mItems[i] != ITEM_NONE)
					aTip = std::string(ItemDefOf(h.mItems[i]).mName) + ": " + ItemDefOf(h.mItems[i]).mDesc;
			for (int i = 0; i < kQuickSlots; i++)
				if (QuickRect(i).Contains(v.mMouseX, v.mMouseY) && QuickShop(s, i) >= 0)
					aTip = std::string(i == 4 ? "5 Suggested next: " : std::to_string(i + 1) + " ") + ShopDefOf(QuickShop(s, i)).mName + ": " + ShopDefOf(QuickShop(s, i)).mDesc;
			if (MapRect().Contains(v.mMouseX, v.mMouseY))
				aTip = "The world: your tank, the Trench, their tank. Dots are heroes, minions and monsters.";
			if (!aTip.empty())
			{
				Font* f = FONT_TINY;
				int w = std::min(430, (f != nullptr ? f->StringWidth(aTip) : 200) + 12);
				int x = std::min(kScreenW - w - 4, 150);
				g->SetColor(Color(0, 0, 20, 225));
				g->FillRect(x, kHudY - 40, w, 34);
				Wrapped(g, f, aTip, x + 6, kHudY - 28, w - 12, Color(235, 240, 255));
			}
		}

		///////////////////////////////////////////////////////////////////////
		// Talents (D36): two cards above the HUD while one waits
		///////////////////////////////////////////////////////////////////////
		static Rect TalentRect(int i) { return Rect(318 + i * 160, kHudY - 46, 156, 42); }

		int TalentHit(const ViewState& v, int x, int y)
		{
			if (v.mSide == nullptr || v.mSide->PendingTalent() < 0)
				return -1;
			for (int i = 0; i < 2; i++)
				if (TalentRect(i).Contains(x, y))
					return i;
			return -1;
		}

		void DrawTalents(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			int t = s.PendingTalent();
			if (t < 0 || !s.mHero.mAlive)
				return;
			// Two compact cards at the bottom right, out of the way of the fight.
			const HeroDef& d = HeroDefOf(s.mHero.mHero);
			float p = 0.5f + 0.5f * std::sin(v.mNow / 200.0f);
			g->SetColor(Color(0, 0, 0, 150));
			g->FillRect(318, kHudY - 60, 316, 14);
			Text(g, FONT_TINYBOLD, "LEVEL " + std::to_string(kTalentLevel[t]) + " TALENT: press Z or X (or click)", 322, kHudY - 50, Color(255, 225, 120, 180 + (int)(75 * p)));
			for (int i = 0; i < 2; i++)
			{
				Rect r = TalentRect(i);
				bool aOver = r.Contains(v.mMouseX, v.mMouseY);
				g->SetColor(aOver ? Color(60, 40, 90, 245) : Color(26, 16, 50, 215));
				g->FillRect(r);
				g->SetColor(Color(255, 205, 80, 140 + (int)(100 * p)));
				g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
				Text(g, FONT_JUNGLEFEVER12OUTLINE, i == 0 ? "Z" : "X", r.mX + 5, r.mY + 16, Color(255, 240, 160));
				Text(g, FONT_JUNGLEFEVER10OUTLINE, d.mTalent[t][i].mName, r.mX + 20, r.mY + 13, Color(255, 255, 255));
				Wrapped(g, FONT_TINY, d.mTalent[t][i].mDesc, r.mX + 20, r.mY + 23, r.mWidth - 24, Color(220, 210, 255));
			}
		}

		///////////////////////////////////////////////////////////////////////
		// The shop
		///////////////////////////////////////////////////////////////////////
		static const Rect kShopRect(50, 30, 540, 398);
		static const char* kTabNames[TAB_COUNT] = { "Fish", "Upgrades", "Hero", "Towers", "Minions" };

		static std::vector<int> TabEntries(int theTab)
		{
			std::vector<int> v;
			for (int i = 0; i < SHOP_COUNT; i++)
				if (ShopDefOf(i).mTab == theTab)
					v.push_back(i);
			return v;
		}

		static Rect TabRect(int i) { return Rect(kShopRect.mX + 12 + i * 104, kShopRect.mY + 30, 100, 22); }
		static Rect EntryRect(int i) { return Rect(kShopRect.mX + 12 + (i % 2) * 260, kShopRect.mY + 58 + (i / 2) * 47, 254, 44); }

		int ShopHit(const ViewState& v, int x, int y)
		{
			if (v.mShopTab < 0)
				return -1;
			if (!kShopRect.Contains(x, y))
				return -100;
			for (int i = 0; i < TAB_COUNT; i++)
				if (TabRect(i).Contains(x, y))
					return -2 - i;
			std::vector<int> e = TabEntries(v.mShopTab);
			for (size_t i = 0; i < e.size(); i++)
				if (EntryRect((int)i).Contains(x, y))
					return e[i];
			return -1;
		}

		void DrawShop(Graphics* g, const ViewState& v)
		{
			if (v.mShopTab < 0)
				return;
			const Side& s = *v.mSide;
			const Rect& R = kShopRect;
			g->SetColor(Color(6, 14, 34, 240));
			g->FillRect(R);
			g->SetColor(Color(255, 205, 80, 220));
			g->DrawRect(R.mX, R.mY, R.mWidth - 1, R.mHeight - 1);
			Text(g, FONT_JUNGLEFEVER12OUTLINE, "SHOP", R.mX + 12, R.mY + 22, Color(255, 215, 90));
			char b[96];
			snprintf(b, sizeof(b), "$%d", s.mArena.mMoney);
			Text(g, FONT_JUNGLEFEVER12OUTLINE, b, R.mX + 90, R.mY + 22, Color(255, 235, 150));
			Text(g, FONT_TINY, "B or Esc closes. The game keeps going!", R.mX + R.mWidth - 196, R.mY + 18, Color(180, 190, 220));
			for (int i = 0; i < TAB_COUNT; i++)
			{
				Rect r = TabRect(i);
				bool aOn = i == v.mShopTab;
				g->SetColor(aOn ? Color(80, 60, 20) : Color(26, 36, 60));
				g->FillRect(r);
				Centered(g, FONT_JUNGLEFEVER10OUTLINE, kTabNames[i], r.mX + r.mWidth / 2, r.mY + 16, aOn ? Color(255, 230, 120) : Color(190, 200, 230));
			}
			int aSuggested = s.SuggestedItem();
			std::vector<int> e = TabEntries(v.mShopTab);
			int aHover = -1;
			for (size_t i = 0; i < e.size(); i++)
			{
				Rect r = EntryRect((int)i);
				int aShop = e[i];
				std::string aWhy;
				bool aCan = s.CanBuy(aShop, &aWhy);
				bool aOver = r.Contains(v.mMouseX, v.mMouseY);
				bool aNext = aShop == SHOP_ITEM_FIRST + aSuggested;
				if (aOver)
					aHover = aShop;
				g->SetColor(aOver ? Color(50, 70, 110) : (aNext ? Color(52, 44, 20) : Color(22, 32, 56)));
				g->FillRect(r);
				if (aNext)
				{
					g->SetColor(Color(255, 205, 80, 200));
					g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
					Text(g, FONT_TINYBOLD, "NEXT (5)", r.mX + r.mWidth - 50, r.mY + 12, Color(255, 215, 90));
				}
				ShopIcon(g, aShop, r.mX + 22.0f, r.mY + 22.0f, 34, v.mNow, std::min(s.mArena.mFoodQuality + 1, 2));
				const ShopDef& sd = ShopDefOf(aShop);
				Text(g, FONT_JUNGLEFEVER10OUTLINE, sd.mName, r.mX + 46, r.mY + 15, aCan ? Color(255, 255, 255) : Color(170, 170, 185));
				int aPrice = s.Price(aShop);
				int aOwned = s.Owned(aShop);
				std::string aSub = aPrice > 0 ? "$" + std::to_string(aPrice) : "maxed";
				if (aOwned > 0)
					aSub += "   (have " + std::to_string(aOwned) + ")";
				Text(g, FONT_TINY, aSub, r.mX + 46, r.mY + 28, aCan ? Color(255, 225, 90) : Color(190, 140, 130));
				if (!aCan && !aWhy.empty() && aWhy != "Not enough money.")
					Text(g, FONT_TINY, aWhy, r.mX + 46, r.mY + 39, Color(180, 150, 150));
			}
			if (aHover >= 0)
			{
				g->SetColor(Color(0, 0, 0, 170));
				g->FillRect(R.mX + 12, R.mY + R.mHeight - 30, R.mWidth - 24, 22);
				Text(g, FONT_TINY, ShopDefOf(aHover).mDesc, R.mX + 18, R.mY + R.mHeight - 15, Color(230, 240, 255));
			}
		}

		///////////////////////////////////////////////////////////////////////
		// Kill feed and announcer banners
		///////////////////////////////////////////////////////////////////////
		static std::string Who(const ViewState& v, int thePlayer) { return thePlayer >= 0 ? v.mNames[thePlayer % kMaxPlayers] : std::string("A tower"); }

		void DrawFeed(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			int y = kTop + 14;
			const Effect* aBanner = nullptr;
			for (const Effect& fx : s.mEffects)
			{
				const Event& e = fx.mEvent;
				float t = (v.mNow - fx.mAt) / 1000.0f;
				if (t > 4)
					continue;
				if ((e.mType == EV_ANNOUNCE && e.mParam != AN_CAMP) || e.mType == EV_TOWER_DOWN)
					if (t < 2.6f)
						aBanner = &fx;					// the newest banner wins
				int a = (int)(255 * Clamp(4 - t, 0, 1));
				std::string aLine;
				Color c(255, 255, 255, a);
				switch (e.mType)
				{
				case EV_KILL:
					aLine = Who(v, e.mPlayer) + " took down " + v.mNames[e.mId % kMaxPlayers] + "  (+$" + std::to_string((int)e.mValue) + ")";
					c = e.mPlayer == s.mPlayer ? Color(255, 225, 90, a) : Color(255, 130, 110, a);
					break;
				case EV_TOWER_DOWN:
					aLine = e.mArena == s.mTeam ? "Your tower fell!" : "Their tower fell!";
					c = e.mArena == s.mTeam ? Color(255, 110, 90, a) : Color(130, 255, 140, a);
					break;
				case EV_WAVE:
					if (e.mArena == s.mTeam)
					{
						aLine = "Minions coming through your portal!";
						c = Color(230, 160, 255, a);
					}
					break;
				case EV_CROSS:
					if (e.mPlayer != s.mPlayer && e.mArena == s.mTeam)
					{
						aLine = v.mNames[e.mPlayer % kMaxPlayers] + " is raiding your tank!";
						c = Color(255, 120, 100, a);
					}
					break;
				case EV_ANNOUNCE:
					if (e.mParam == AN_CAMP)
					{
						aLine = Who(v, e.mPlayer) + (e.mId == MON_GUS ? " took the Gus camp" : " took the Balrog camp");
						c = e.mPlayer == s.mPlayer ? Color(160, 255, 160, a) : Color(220, 180, 255, a);
					}
					break;
				default:
					break;
				}
				if (aLine.empty())
					continue;
				Text(g, FONT_JUNGLEFEVER10OUTLINE, aLine, 8, y, c);
				y += 14;
			}
			if (aBanner == nullptr)
				return;
			// The banner: big, centered under the strip; slides in and fades.
			const Event& e = aBanner->mEvent;
			float t = (v.mNow - aBanner->mAt) / 1000.0f;
			std::string aBig, aSmall;
			bool aGood = e.mPlayer == s.mPlayer;
			std::string aName = Who(v, e.mPlayer);
			switch (e.mType == EV_TOWER_DOWN ? 255 : e.mParam)
			{
			case 255: aBig = e.mArena == s.mTeam ? "YOUR TOWER FELL" : "TOWER DESTROYED!"; aGood = e.mArena != s.mTeam; break;
			case AN_FIRST_BLOOD: aBig = "FIRST BLOOD!"; aSmall = aName + " drew first blood"; break;
			case AN_SPREE: aBig = "KILLING SPREE!"; aSmall = aName + ": " + std::to_string((int)e.mValue) + " in a row"; break;
			case AN_RAMPAGE: aBig = "RAMPAGE!"; aSmall = aName + ": " + std::to_string((int)e.mValue) + " in a row"; break;
			case AN_UNSTOPPABLE: aBig = "UNSTOPPABLE!"; aSmall = aName + ": " + std::to_string((int)e.mValue) + " in a row"; break;
			case AN_GODLIKE: aBig = "GODLIKE!"; aSmall = aName + ": " + std::to_string((int)e.mValue) + " in a row"; break;
			case AN_SHUTDOWN: aBig = "SHUTDOWN!"; aSmall = aName + " ended " + v.mNames[e.mId % kMaxPlayers] + "'s streak: +$" + std::to_string((int)e.mValue); break;
			case AN_SQUID: aBig = aName + " SLEW THE PSYCHOSQUID!"; aSmall = "It joins their next wave"; break;
			case AN_BOSS: aBig = aName + " SLEW THE BOSS!"; aSmall = "+40% to everything for 2 minutes"; break;
			case AN_SQUID_UP: aBig = "THE PSYCHOSQUID AWAKENS"; aSmall = "in the Trench: whoever slays it sends it at the rival"; aGood = true; break;
			case AN_BOSS_UP: aBig = "THE BOSS RISES"; aSmall = "in the Trench: slay it for 2 minutes of power"; aGood = true; break;
			case AN_EVOLVE: aBig = aName + "'S " + std::string(HeroDefOf((int)e.mValue).mName) + " EVOLVED!"; aSmall = "Bigger, stronger, and F is unlocked"; break;
			default: return;
			}
			int a = (int)(255 * Clamp((2.6f - t) / 0.5f, 0, 1));
			float aSlide = std::max(0.0f, 1 - t / 0.18f);
			int y0 = kTop + 70 - (int)(aSlide * 30);
			// Centered on the screen, or left of the home window while it's up.
			int aCx = v.mHomeWindow ? (kHomeX - 6) / 2 : kScreenW / 2;
			int aRoom = v.mHomeWindow ? kHomeX - 20 : kScreenW - 40;
			Font* fb = FONT_JUNGLEFEVER17OUTLINE;
			if (fb != nullptr && fb->StringWidth(aBig) > aRoom - 40)
				fb = FONT_JUNGLEFEVER15OUTLINE;
			if (fb != nullptr && fb->StringWidth(aBig) > aRoom - 40)
				fb = FONT_JUNGLEFEVER12OUTLINE;
			int w = fb != nullptr ? fb->StringWidth(aBig) : 300;
			if (!aSmall.empty() && FONT_JUNGLEFEVER10OUTLINE != nullptr)
				w = std::max(w, FONT_JUNGLEFEVER10OUTLINE->StringWidth(aSmall));
			Color aBack = aGood ? Color(70, 50, 0, a * 3 / 4) : Color(80, 10, 10, a * 3 / 4);
			g->SetColor(aBack);
			g->FillRect(aCx - w / 2 - 14, y0 - 26, w + 28, aSmall.empty() ? 34 : 50);
			g->SetColor(aGood ? Color(255, 215, 90, a) : Color(255, 110, 90, a));
			g->DrawRect(aCx - w / 2 - 14, y0 - 26, w + 27, aSmall.empty() ? 33 : 49);
			Centered(g, fb, aBig, aCx, y0, aGood ? Color(255, 230, 120, a) : Color(255, 140, 120, a));
			if (!aSmall.empty())
				Centered(g, FONT_JUNGLEFEVER10OUTLINE, aSmall, aCx, y0 + 16, Color(235, 235, 255, a));
		}

		///////////////////////////////////////////////////////////////////////
		// Death: the countdown and what killed you
		///////////////////////////////////////////////////////////////////////
		void DrawDeath(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			const HeroState& h = s.mHero;
			if (h.mAlive)
				return;
			g->SetColor(Color(0, 0, 0, 100));
			g->FillRect(0, kTop, kScreenW, kHudY - kTop);
			char b[48];
			snprintf(b, sizeof(b), "Back in %d", (int)std::ceil((int32_t)(h.mRespawnAt - v.mNow) / 1000.0f));
			Centered(g, FONT_JUNGLEFEVER17OUTLINE, b, kScreenW / 2, 160, Color(255, 255, 255));
			// The recap: damage in the last 10 s, by source.
			std::map<std::string, float> aBy;
			float aTotal = 0;
			for (const RecapHit& r : s.mRecap)
			{
				std::string k;
				if (r.mSource == SRC_TOWER) k = "Tower shots";
				else if (r.mSource == SRC_LASER) k = "Their laser";
				else if (r.mSource == SRC_MINION) k = "Minions";
				else if (r.mSource == SRC_MONSTER) k = "Monsters";
				else
				{
					std::string aWho = r.mHero < HERO_COUNT ? std::string(HeroDefOf(r.mHero).mName) + "'s " : std::string("");
					if (r.mSource == SRC_BURN) k = aWho + "burn";
					else if (r.mAbility < AB_COUNT && r.mHero < HERO_COUNT) k = aWho + HeroDefOf(r.mHero).mAb[r.mAbility].mName;
					else k = aWho + "attacks";
				}
				aBy[k] += r.mDamage;
				aTotal += r.mDamage;
			}
			if (aBy.empty())
				return;
			std::vector<std::pair<float, std::string>> aSorted;
			for (auto& kv : aBy)
				aSorted.push_back({ kv.second, kv.first });
			std::sort(aSorted.rbegin(), aSorted.rend());
			Rect R(kScreenW / 2 - 140, 180, 280, 28 + 15 * (int)std::min<size_t>(5, aSorted.size()));
			g->SetColor(Color(10, 10, 30, 220));
			g->FillRect(R);
			g->SetColor(Color(255, 120, 100, 200));
			g->DrawRect(R.mX, R.mY, R.mWidth - 1, R.mHeight - 1);
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, "What got you (" + std::to_string((int)aTotal) + " damage):", kScreenW / 2, R.mY + 16, Color(255, 200, 180));
			int y = R.mY + 32;
			for (size_t i = 0; i < aSorted.size() && i < 5; i++)
			{
				Text(g, FONT_TINY, aSorted[i].second, R.mX + 14, y, Color(235, 235, 255));
				Bar(g, R.mX + 150, y - 7, 90, 6, aSorted[i].first / std::max(1.0f, aTotal), Color(240, 100, 80));
				Text(g, FONT_TINYBOLD, std::to_string((int)aSorted[i].first), R.mX + 246, y, Color(255, 200, 180));
				y += 15;
			}
		}

		///////////////////////////////////////////////////////////////////////
		// The guided first match
		///////////////////////////////////////////////////////////////////////
		void DrawTutorial(Graphics* g, const ViewState& v)
		{
			if (v.mTutorial.empty())
				return;
			Font* f = FONT_JUNGLEFEVER12OUTLINE;
			Rect R(70, kTop + 22, 500, 52);
			g->SetColor(Color(4, 30, 20, 225));
			g->FillRect(R);
			float p = 0.5f + 0.5f * std::sin(v.mNow / 250.0f);
			g->SetColor(Color(120, 255, 160, 140 + (int)(100 * p)));
			g->DrawRect(R.mX, R.mY, R.mWidth - 1, R.mHeight - 1);
			std::string aHead = v.mTutorialDone ? "You're ready!" : "Tutorial  " + std::to_string(v.mTutorialStep + 1) + " / " + std::to_string(v.mTutorialSteps);
			Text(g, FONT_JUNGLEFEVER10OUTLINE, aHead, R.mX + 10, R.mY + 15, Color(160, 255, 190));
			Text(g, FONT_TINYBOLD, "Esc: skip", R.mX + R.mWidth - 54, R.mY + 13, Color(180, 210, 190));
			Wrapped(g, f != nullptr && f->StringWidth(v.mTutorial) < R.mWidth - 20 ? f : FONT_JUNGLEFEVER10OUTLINE, v.mTutorial, R.mX + 10, R.mY + 33, R.mWidth - 20, Color(255, 255, 255));
			// Progress pips.
			for (int i = 0; i < v.mTutorialSteps; i++)
				Disc(g, R.mX + R.mWidth / 2.0f - v.mTutorialSteps * 5 + i * 10 + 5, R.mY + R.mHeight + 6.0f, 3, i < v.mTutorialStep || v.mTutorialDone ? Color(120, 255, 160) : (i == v.mTutorialStep ? Color(255, 255, 255) : Color(60, 90, 70)), 8);
		}

		///////////////////////////////////////////////////////////////////////
		// Draft (nine heroes: a 5 + 4 grid and a detail panel)
		///////////////////////////////////////////////////////////////////////
		static Rect CardRect(int i)
		{
			if (i < 5)
				return Rect(22 + i * 120, 56, 114, 92);
			return Rect(82 + (i - 5) * 120, 152, 114, 92);
		}
		static Rect DraftButtonRect(int theButton)
		{
			switch (theButton)
			{
			case DB_OPPONENT: return Rect(20, 404, 150, 30);
			case DB_SKILL: return Rect(176, 404, 120, 30);
			case DB_TUTORIAL: return Rect(302, 404, 120, 30);
			case DB_START: return Rect(440, 398, 180, 40);
			default: return Rect(20, 444, 100, 26);
			}
		}

		int DraftHit(int x, int y, bool thePractice)
		{
			for (int i = 0; i < HERO_COUNT; i++)
				if (CardRect(i).Contains(x, y))
					return i;
			static const int kButtons[] = { DB_OPPONENT, DB_SKILL, DB_TUTORIAL, DB_START, DB_BACK };
			for (int b : kButtons)
			{
				if (!thePractice && (b == DB_OPPONENT || b == DB_SKILL || b == DB_TUTORIAL))
					continue;
				if (DraftButtonRect(b).Contains(x, y))
					return b;
			}
			return DB_NONE;
		}

		static void StatBar(Graphics* g, int x, int y, const char* theLabel, float theFrac, const Color& c)
		{
			Text(g, FONT_TINY, theLabel, x, y + 5, Color(200, 210, 230));
			Bar(g, x + 40, y, 70, 4, theFrac, c);
		}

		static void Button(Graphics* g, const Rect& r, const std::string& theLabel, bool theOn, bool theHover)
		{
			g->SetColor(theOn ? (theHover ? Color(110, 150, 60) : Color(80, 120, 40)) : Color(50, 50, 60));
			g->FillRect(r);
			g->SetColor(Color(255, 230, 140, 200));
			g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
			Font* f = r.mHeight >= 36 ? FONT_JUNGLEFEVER12OUTLINE : FONT_JUNGLEFEVER10OUTLINE;
			Centered(g, f, theLabel, r.mX + r.mWidth / 2, r.mY + r.mHeight / 2 + 5, theOn ? Color(255, 255, 255) : Color(150, 150, 160));
		}

		void DrawDraft(Graphics* g, uint32_t theNow, int theHover, int theMine, int theTheirs, bool thePractice, int theBotHero, int theBotSkill, const std::string& theStatus)
		{
			g->SetLinearBlend(true);
			Image* b = IMAGE_AQUARIUM6 != nullptr ? IMAGE_AQUARIUM6 : IMAGE_AQUARIUM3;
			if (b != nullptr)
				g->DrawImage(b, Rect(0, 0, kScreenW, kScreenH), Rect(0, 0, b->mWidth, b->mHeight));
			g->SetColor(Color(0, 6, 26, 190));
			g->FillRect(0, 0, kScreenW, kScreenH);
			Centered(g, FONT_JUNGLEFEVER17OUTLINE, "PET HEROES", kScreenW / 2, 28, Color(255, 215, 80));
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, thePractice ? "Practice against the bot. Pick your hero:" : "Pick your hero (your rival sees it when you both lock in):", kScreenW / 2, 48, Color(220, 230, 255));
			for (int i = 0; i < HERO_COUNT; i++)
			{
				const HeroDef& d = HeroDefOf(i);
				Rect r = CardRect(i);
				bool aMine = i == theMine, aHover = i == theHover;
				g->SetColor(aMine ? Color(70, 60, 20, 240) : (aHover ? Color(30, 50, 90, 240) : Color(16, 26, 50, 230)));
				g->FillRect(r);
				g->SetColor(aMine ? Color(255, 215, 80) : Color(90, 120, 170));
				g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
				if (aMine)
					g->DrawRect(r.mX + 1, r.mY + 1, r.mWidth - 3, r.mHeight - 3);
				float aScale = (aHover || aMine ? 0.9f : 0.8f) * (i == HERO_NIKO ? 0.8f : 1.0f);
				ScreenSprite(g, HeroImage(i), (int)((theNow / 80) % 10), 0, r.mX + r.mWidth / 2.0f, r.mY + 38.0f, aScale, false);
				Centered(g, FONT_JUNGLEFEVER12OUTLINE, d.mName, r.mX + r.mWidth / 2, r.mY + 76, Color(255, 255, 255));
				Centered(g, FONT_TINY, d.mRole + std::string(d.mWalker ? " (walks)" : ""), r.mX + r.mWidth / 2, r.mY + 88, Color(255, 205, 90));
				Text(g, FONT_TINYBOLD, std::to_string(i + 1), r.mX + 4, r.mY + 11, Color(200, 210, 230));
				if (i >= HERO_PRESTO)
					Text(g, FONT_TINYBOLD, "NEW", r.mX + r.mWidth - 26, r.mY + 11, Color(120, 255, 160));
			}
			// The detail panel: the hovered hero, else the picked one.
			int aShow = theHover >= 0 ? theHover : theMine;
			Rect P(20, 250, 600, 146);
			g->SetColor(Color(8, 16, 36, 235));
			g->FillRect(P);
			g->SetColor(Color(90, 120, 170));
			g->DrawRect(P.mX, P.mY, P.mWidth - 1, P.mHeight - 1);
			if (aShow >= 0)
			{
				const HeroDef& d = HeroDefOf(aShow);
				Text(g, FONT_JUNGLEFEVER12OUTLINE, std::string(d.mName) + " - " + d.mRole, P.mX + 10, P.mY + 18, Color(120, 230, 255));
				Wrapped(g, FONT_TINY, d.mBlurb, P.mX + 10, P.mY + 30, 200, Color(255, 240, 200));
				int y = P.mY + 70;
				StatBar(g, P.mX + 10, y, "Health", d.mHealth / 800, Color(110, 230, 110));
				StatBar(g, P.mX + 10, y + 10, "Attack", d.mDamage / d.mAttackS / 45, Color(250, 110, 90));
				StatBar(g, P.mX + 10, y + 20, "Range", d.mRange / 260, Color(120, 190, 255));
				StatBar(g, P.mX + 10, y + 30, "Speed", d.mSpeed / 250, Color(255, 225, 90));
				Text(g, FONT_TINY, d.mWalker ? "Walks the floor (W hops, S crosses)" : "Swims anywhere", P.mX + 10, y + 52, Color(200, 190, 255));
				int x2 = P.mX + 222, ty = P.mY + 14;
				ty = Wrapped(g, FONT_TINY, std::string(d.mPassiveName) + ": " + d.mPassive, x2, ty, 372, Color(200, 230, 255));
				for (int k = 0; k < AB_COUNT; k++)
				{
					ty = Wrapped(g, FONT_TINY, std::string(kAbilityKeys[k]) + " " + d.mAb[k].mName + ": " + d.mAb[k].mDesc, x2, ty, 372, k == AB_R ? Color(255, 190, 120) : Color(235, 235, 245));
				}
				ty += 2;
				std::string aTal = "Talents: ";
				for (int t = 0; t < kTalentTiers; t++)
					aTal += std::string(t ? " / " : "") + d.mTalent[t][0].mName + " or " + d.mTalent[t][1].mName;
				Wrapped(g, FONT_TINY, aTal, x2, ty, 372, Color(210, 180, 255));
			}
			else
				Centered(g, FONT_JUNGLEFEVER12OUTLINE, "Hover a hero to see what it does.", P.mX + P.mWidth / 2, P.mY + 76, Color(190, 200, 230));
			if (thePractice)
			{
				std::string aOpp = std::string("Bot: ") + (theBotHero < 0 ? "random" : HeroDefOf(theBotHero).mName);
				static const char* kSkill[3] = { "Easy", "Normal", "Hard" };
				Button(g, DraftButtonRect(DB_OPPONENT), aOpp, true, false);
				Button(g, DraftButtonRect(DB_SKILL), std::string("Bot: ") + kSkill[std::clamp(theBotSkill, 0, 2)], true, false);
				Button(g, DraftButtonRect(DB_TUTORIAL), "Tutorial", true, false);
			}
			Button(g, DraftButtonRect(DB_START), thePractice ? "Start!" : "Lock in!", theMine >= 0, false);
			Button(g, DraftButtonRect(DB_BACK), "Back", true, false);
			if (!theStatus.empty())
				Centered(g, FONT_JUNGLEFEVER10OUTLINE, theStatus, kScreenW / 2 + 60, 462, Color(255, 225, 150));
			(void)theTheirs;
			g->SetLinearBlend(false);
		}

		void DrawCountdown(Graphics* g, int theSeconds)
		{
			g->SetColor(Color(0, 0, 0, 90));
			g->FillRect(0, kTop, kScreenW, kHudY - kTop);
			Centered(g, FONT_JUNGLEFEVER17OUTLINE, theSeconds > 0 ? std::to_string(theSeconds) : "GO!", kScreenW / 2, 220, Color(255, 225, 90));
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, "WASD move - you attack on your own - hold Q E R F to aim, let go to cast", kScreenW / 2, 250, Color(230, 240, 255));
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, "Through the portal: the Trench. B shop - Tab home - hold H: help", kScreenW / 2, 266, Color(230, 240, 255));
		}

		void DrawHelp(Graphics* g, int theHero, bool theWaiting)
		{
			Rect R(30, 20, 580, 420);
			g->SetColor(Color(4, 12, 32, 242));
			g->FillRect(R);
			g->SetColor(Color(255, 205, 80, 220));
			g->DrawRect(R.mX, R.mY, R.mWidth - 1, R.mHeight - 1);
			Centered(g, FONT_JUNGLEFEVER15OUTLINE, "HOW TO PLAY PET HEROES", kScreenW / 2, R.mY + 26, Color(255, 215, 80));
			Font* f = FONT_JUNGLEFEVER10OUTLINE;
			Color k(255, 230, 140), t(235, 240, 255), d(190, 200, 225);
			int y = R.mY + 50;
			auto Line = [&](const std::string& theKey, const std::string& theText) {
				Text(g, f, theKey, R.mX + 16, y, k);
				int aBottom = Wrapped(g, FONT_TINY, theText, R.mX + 132, y - 8, R.mWidth - 148, t);
				y = std::max(y + 18, aBottom + 10);
			};
			Line("W A S D", "Move. Your hero attacks the nearest enemy on its own. Walkers: A/D walk, W or Space hops, S near a portal or pad crosses.");
			Line("Q E R F", "Tap to cast at the mouse; hold to see where it goes, let go to cast (right-click cancels). F unlocks when your pet evolves at level 6.");
			Line("Right-click", "Attack something (a monster too), or move there.");
			Line("Z / X", "Pick a talent when the cards pop up (levels 3, 6 and 9).");
			Line("Left-click", "At home or in the home window (top right while away): coins, food ($5), your laser.");
			Line("1-4, 5, B", "Quick-buy a guppy, food, a breeder, a carnivore; 5 buys your suggested item; B is the shop.");
			Line("The Trench", "Your portal leads to it. Both waves meet and fight there: dead minions drop coins, grab them. Camps, the Psychosquid (4:00) and the Boss (8:00) wait to be fought.");
			Line("Win", "Push your wave through the far gate into their tank. Break both towers, then their treasure chest core.");
			Line("Tips", "Feed your fish: they pay for everything. Kill streaks raise your bounty. The Squid joins your next wave; the Boss makes you huge.");
			const HeroDef& h = HeroDefOf(theHero);
			y += 4;
			Text(g, FONT_JUNGLEFEVER12OUTLINE, std::string(h.mName) + " (" + h.mRole + ")", R.mX + 16, y, Color(120, 230, 255));
			y += 6;
			y = Wrapped(g, FONT_TINY, std::string(h.mPassiveName) + ": " + h.mPassive, R.mX + 16, y, R.mWidth - 32, d);
			for (int i = 0; i < AB_COUNT; i++)
				y = Wrapped(g, FONT_TINY, std::string(kAbilityKeys[i]) + " " + h.mAb[i].mName + ": " + h.mAb[i].mDesc, R.mX + 16, y, R.mWidth - 32, i == AB_R ? Color(255, 200, 130) : d);
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, theWaiting ? "Press any key or click to start!" : "Hold H to see this again.", kScreenW / 2, R.mY + R.mHeight - 8, Color(255, 225, 150));
		}

		///////////////////////////////////////////////////////////////////////
		// The result: both players and the awards
		///////////////////////////////////////////////////////////////////////
		static Rect ResultButtonRect() { return Rect(kScreenW / 2 - 120, 430, 240, 34); }
		int ResultHit(int x, int y) { return ResultButtonRect().Contains(x, y) ? 1 : 0; }

		void DrawResult(Graphics* g, const ViewState& v, bool theWon, const std::string& theReason, uint32_t theMatchMs,
			const HeroSnap& theOther, int theOtherFishLost, int theOtherEarned)
		{
			const Side& s = *v.mSide;
			g->SetColor(Color(0, 0, 0, 170));
			g->FillRect(0, 0, kScreenW, kScreenH);
			Rect R(60, 30, 520, 440);
			g->SetColor(Color(10, 20, 44, 245));
			g->FillRect(R);
			g->SetColor(theWon ? Color(255, 215, 80) : Color(230, 100, 90));
			g->DrawRect(R.mX, R.mY, R.mWidth - 1, R.mHeight - 1);
			Centered(g, FONT_JUNGLEFEVER17OUTLINE, theWon ? "VICTORY!" : "DEFEAT", kScreenW / 2, R.mY + 32, theWon ? Color(255, 215, 80) : Color(240, 110, 100));
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, theReason, kScreenW / 2, R.mY + 52, Color(220, 230, 255));
			Centered(g, FONT_TINY, "Match time " + Clock(theMatchMs), kScreenW / 2, R.mY + 66, Color(190, 200, 230));
			const HeroState& h = s.mHero;
			struct Col { std::string mName; int mHero, mLevel, mKills, mDeaths, mTowers, mFishLost, mEarned, mLane, mObj; float mStruct; };
			Col aCols[2] = {
				{ v.mNames[s.mPlayer % kMaxPlayers], h.mHero, h.mLevel, h.mKills, h.mDeaths, h.mTowers, s.mArena.mFishLost, s.mArena.mMoneyEarned, h.mLaneCoins, h.mObjectives, h.mStructDamage },
				{ v.mNames[s.mOtherPlayer % kMaxPlayers], theOther.mBaseHero, theOther.mLevel, theOther.mKills, theOther.mDeaths, theOther.mTowers, theOtherFishLost, theOtherEarned,
					theOther.mLaneCoins, theOther.mObjectives, (float)theOther.mStructDamage },
			};
			// Awards: whoever did more of each (a tie gives it to nobody).
			std::vector<std::string> aAwards[2];
			auto Award = [&](const char* theName, float a, float b, bool theLowWins = false) {
				if (a == b)
					return;
				bool aFirst = theLowWins ? a < b : a > b;
				aAwards[aFirst ? 0 : 1].push_back(theName);
			};
			auto Score = [](const Col& c) { return c.mKills * 3.0f - c.mDeaths * 1.5f + c.mTowers * 4 + c.mObj * 2 + c.mStruct / 600 + c.mLane / 300.0f; };
			Award("MVP", Score(aCols[0]), Score(aCols[1]));
			Award("Tower Toppler", aCols[0].mStruct, aCols[1].mStruct);
			Award("Lane Bully", (float)aCols[0].mLane, (float)aCols[1].mLane);
			Award("Monster Hunter", (float)aCols[0].mObj, (float)aCols[1].mObj);
			Award("Fish Whisperer", (float)aCols[0].mFishLost, (float)aCols[1].mFishLost, true);
			Award("Big Earner", (float)aCols[0].mEarned, (float)aCols[1].mEarned);
			for (int c = 0; c < 2; c++)
			{
				int cx = R.mX + 130 + c * 260;
				const Col& k = aCols[c];
				ScreenSprite(g, HeroImage(k.mHero), (int)((v.mNow / 80) % 10), 0, (float)cx, R.mY + 108.0f, k.mHero == HERO_NIKO ? 0.9f : 1.1f, c == 0 && !(k.mHero == HERO_RHUBARB || k.mHero == HERO_NIKO));
				Centered(g, FONT_JUNGLEFEVER12OUTLINE, k.mName, cx, R.mY + 158, TeamColor(c == 0 ? s.mTeam : 1 - s.mTeam));
				Centered(g, FONT_JUNGLEFEVER10OUTLINE, std::string(HeroDefOf(k.mHero).mName) + "  level " + std::to_string(k.mLevel), cx, R.mY + 175, Color(230, 230, 240));
				int y = R.mY + 196;
				auto Row = [&](const char* theLabel, const std::string& theValue) {
					Text(g, FONT_JUNGLEFEVER10OUTLINE, theLabel, cx - 100, y, Color(190, 200, 230));
					Text(g, FONT_JUNGLEFEVER10OUTLINE, theValue, cx + 50, y, Color(255, 255, 255));
					y += 17;
				};
				Row("Kills / deaths", std::to_string(k.mKills) + " / " + std::to_string(k.mDeaths));
				Row("Towers taken", std::to_string(k.mTowers));
				Row("Monsters slain", std::to_string(k.mObj));
				Row("Lane coins", "$" + std::to_string(k.mLane));
				Row("Fish lost", std::to_string(k.mFishLost));
				Row("Money earned", "$" + std::to_string(k.mEarned));
				// Awards: badges, two to a row.
				y += 6;
				for (size_t i = 0; i < aAwards[c].size(); i++)
				{
					const std::string& a = aAwards[c][i];
					int bx = cx - 112 + (int)(i % 2) * 114, by = y + (int)(i / 2) * 18;
					g->SetColor(a == "MVP" ? Color(120, 90, 10, 230) : Color(40, 30, 70, 230));
					g->FillRect(bx, by - 12, 110, 16);
					g->SetColor(a == "MVP" ? Color(255, 215, 80) : Color(200, 160, 255));
					g->DrawRect(bx, by - 12, 109, 15);
					Centered(g, FONT_JUNGLEFEVER10OUTLINE, a, bx + 55, by, a == "MVP" ? Color(255, 235, 140) : Color(235, 220, 255));
				}
			}
			Button(g, ResultButtonRect(), "Back to the menu (Enter)", true, ResultButtonRect().Contains(v.mMouseX, v.mMouseY));
		}
	}
}
