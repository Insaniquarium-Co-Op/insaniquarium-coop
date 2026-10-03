#include <SexyAppFramework/Font.h>
#include "CoopHeroesDraw.h"
#include "HeroesMap.h"
#include "WinFishApp.h"
#include "Res.h"
#include <SexyAppFramework/Graphics.h>
#include <SexyAppFramework/Image.h>
#include <SexyAppFramework/MemoryImage.h>
#include <SexyAppFramework/SexyAppBase.h>
#include <SexyAppFramework/SexyMatrix.h>
#include <SexyAppFramework/TriVertex.h>
#include <cmath>
#include <cstdio>
#include <map>
#include <tuple>

using namespace Sexy;
using namespace Heroes;

namespace Coop
{
	namespace HV
	{
		WorldView gView;
		static const Color kTeamColor[3] = { Color(255, 205, 60), Color(80, 220, 255), Color(200, 120, 255) };
		static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }
		const Color& TeamColor(int theTeam) { return kTeamColor[std::clamp(theTeam, 0, 2)]; }

		///////////////////////////////////////////////////////////////////////
		// Small drawing helpers
		///////////////////////////////////////////////////////////////////////
		// A cel of a sprite sheet. A few sheets have no rows/cols in resources.xml (the
		// game computes their cels itself): they're 80x80 cels.
		static Rect Cel(Image* theImage, int theCol, int theRow)
		{
			if (theImage == IMAGE_SMALLEAT || theImage == IMAGE_SMALLDIE || theImage == IMAGE_HUNGRYBREEDER
				|| theImage == IMAGE_SMALLTURN || theImage == IMAGE_HUNGRYTURN || theImage == IMAGE_HUNGRYEAT)
			{
				int aCols = std::max(1, theImage->mWidth / 80), aRows = std::max(1, theImage->mHeight / 80);
				return Rect((theCol % aCols) * 80, (theRow % aRows) * 80, 80, 80);
			}
			return theImage->GetCelRect(theCol % std::max(1, theImage->mNumCols), theRow % std::max(1, theImage->mNumRows));
		}

		void Sprite(Graphics* g, Image* theImage, int theCol, int theRow, float wx, float wy, float theScale, bool theMirror, const Color& theTint, bool theColorize)
		{
			if (theImage == nullptr)
				return;
			Rect aSrc = Cel(theImage, theCol, theRow);
			Transform t;
			float k = theScale * gView.mScale;
			t.Scale(theMirror ? -k : k, k);
			if (theColorize || theTint.mAlpha < 255)
			{
				g->SetColorizeImages(true);
				g->SetColor(theTint);
			}
			g->DrawImageTransformF(theImage, t, aSrc, SX(wx), SY(wy));
			g->SetColorizeImages(false);
		}
		void Sprite(Graphics* g, Image* theImage, int theCol, int theRow, float wx, float wy, float theScale, bool theMirror)
		{
			Sprite(g, theImage, theCol, theRow, wx, wy, theScale, theMirror, Color(255, 255, 255), false);
		}

		// The same in screen coordinates (HUD, draft).
		void ScreenSprite(Graphics* g, Image* theImage, int theCol, int theRow, float sx, float sy, float theScale, bool theMirror, const Color& theTint, bool theColorize)
		{
			if (theImage == nullptr)
				return;
			Rect aSrc = Cel(theImage, theCol, theRow);
			Transform t;
			t.Scale(theMirror ? -theScale : theScale, theScale);
			if (theColorize || theTint.mAlpha < 255)
			{
				g->SetColorizeImages(true);
				g->SetColor(theTint);
			}
			g->DrawImageTransformF(theImage, t, aSrc, sx, sy);
			g->SetColorizeImages(false);
		}
		void ScreenSprite(Graphics* g, Image* theImage, int theCol, int theRow, float sx, float sy, float theScale, bool theMirror)
		{
			ScreenSprite(g, theImage, theCol, theRow, sx, sy, theScale, theMirror, Color(255, 255, 255), false);
		}

		void Disc(Graphics* g, float cx, float cy, float r, const Color& c, int theSides)
		{
			if (r < 0.5f)
				return;
			std::vector<Point> p;
			for (int k = 0; k < theSides; k++)
				p.push_back(Point((int)std::lround(cx + r * std::cos(k * 6.2832f / theSides)), (int)std::lround(cy + r * std::sin(k * 6.2832f / theSides))));
			g->SetColor(c);
			g->PolyFill(p.data(), (int)p.size(), true);
		}

		void Ring(Graphics* g, float cx, float cy, float r, const Color& c, int theThick)
		{
			g->SetColor(c);
			int n = r > 40 ? 48 : 32;
			for (int t = 0; t < theThick; t++)
			{
				float rr = r + t;
				for (int k = 0; k < n; k++)
				{
					float a0 = k * 6.2832f / n, a1 = (k + 1) * 6.2832f / n;
					g->DrawLine((int)(cx + rr * std::cos(a0)), (int)(cy + rr * std::sin(a0)), (int)(cx + rr * std::cos(a1)), (int)(cy + rr * std::sin(a1)));
				}
			}
		}

		void Bar(Graphics* g, int x, int y, int w, int h, float theFrac, const Color& theFill, const Color& theBack)
		{
			g->SetColor(theBack);
			g->FillRect(x - 1, y - 1, w + 2, h + 2);
			int f = (int)std::lround(w * Clamp(theFrac, 0, 1));
			g->SetColor(theFill);
			g->FillRect(x, y, f, h);
		}
		void Bar(Graphics* g, int x, int y, int w, int h, float theFrac, const Color& theFill) { Bar(g, x, y, w, h, theFrac, theFill, Color(20, 20, 30, 200)); }

		void Text(Graphics* g, Font* f, const std::string& s, int x, int y, const Color& c)
		{
			if (f == nullptr)
				return;
			g->SetFont(f);
			g->SetColor(c);
			g->DrawString(s, x, y);
		}

		void Centered(Graphics* g, Font* f, const std::string& s, int cx, int y, const Color& c)
		{
			if (f == nullptr)
				return;
			Text(g, f, s, cx - f->StringWidth(s) / 2, y, c);
		}

		int Wrapped(Graphics* g, Font* f, const std::string& s, int x, int y, int w, const Color& c)
		{
			// Simple word wrap; returns the y below the text.
			if (f == nullptr)
				return y;
			g->SetFont(f);
			g->SetColor(c);
			std::string aLine, aWord;
			int aLineH = f->GetHeight();
			auto Flush = [&]() { if (!aLine.empty()) { g->DrawString(aLine, x, y); y += aLineH; aLine.clear(); } };
			for (size_t i = 0; i <= s.size(); i++)
			{
				char ch = i < s.size() ? s[i] : ' ';
				if (ch == ' ')
				{
					std::string aTry = aLine.empty() ? aWord : aLine + " " + aWord;
					if (f->StringWidth(aTry) > w && !aLine.empty())
					{
						Flush();
						aLine = aWord;
					}
					else
						aLine = aTry;
					aWord.clear();
				}
				else
					aWord.push_back(ch);
			}
			Flush();
			return y;
		}

		std::string Clock(uint32_t theMs)
		{
			char b[16];
			snprintf(b, sizeof(b), "%u:%02u", theMs / 60000, (theMs / 1000) % 60);
			return b;
		}

		const char* HeroName(int theHero) { return HeroDefOf(theHero).mName; }

		// Speedy (D30): Stinky's art in a neon, purple-heavy palette, made once per image.
		static Image* Neon(Image* theSrc)
		{
			static std::map<Image*, MemoryImage*> sCache;
			if (theSrc == nullptr)
				return nullptr;
			auto it = sCache.find(theSrc);
			if (it != sCache.end())
				return it->second != nullptr ? it->second : theSrc;
			MemoryImage* aSrc = dynamic_cast<MemoryImage*>(theSrc);
			uint32_t* sb = aSrc != nullptr ? aSrc->GetBits() : nullptr;
			MemoryImage* m = nullptr;
			if (sb != nullptr)
			{
				int w = aSrc->mWidth, h = aSrc->mHeight;
				m = new MemoryImage(gSexyAppBase);
				m->Create(w, h);
				m->mNumCols = aSrc->mNumCols;
				m->mNumRows = aSrc->mNumRows;
				uint32_t* db = m->GetBits();
				for (int i = 0; i < w * h; i++)
				{
					uint32_t p = sb[i];
					float r = ((p >> 16) & 255) / 255.0f, gr = ((p >> 8) & 255) / 255.0f, b = (p & 255) / 255.0f;
					float mx = std::max(r, std::max(gr, b)), mn = std::min(r, std::min(gr, b)), d = mx - mn;
					float hue = 0, sat = mx > 0 ? d / mx : 0, val = mx;
					if (d > 0)
					{
						if (mx == r) hue = 60 * std::fmod((gr - b) / d, 6.0f);
						else if (mx == gr) hue = 60 * ((b - r) / d + 2);
						else hue = 60 * ((r - gr) / d + 4);
						if (hue < 0) hue += 360;
					}
					if (sat < 0.18f)
					{
						hue = 275;
						sat = std::min(1.0f, sat + 0.18f);
					}
					else
					{
						bool aLine = hue < 50 && val < 0.42f && val > 0.12f;
						hue = aLine ? 185 : (hue < 50 ? 270 + hue * 0.3f : (hue < 150 ? 300 + (hue - 50) * 0.25f : std::fmod(hue + 120, 360.0f)));
						sat = std::min(1.0f, sat * 1.3f + 0.25f);
						val = aLine ? 0.95f : std::min(1.0f, val * 1.25f + 0.12f);
					}
					float c = val * sat, x = c * (1 - std::fabs(std::fmod(hue / 60, 2.0f) - 1)), o = val - c;
					float rr, gg, bb;
					int k = (int)(hue / 60) % 6;
					switch (k)
					{
					case 0: rr = c; gg = x; bb = 0; break;
					case 1: rr = x; gg = c; bb = 0; break;
					case 2: rr = 0; gg = c; bb = x; break;
					case 3: rr = 0; gg = x; bb = c; break;
					case 4: rr = x; gg = 0; bb = c; break;
					default: rr = c; gg = 0; bb = x; break;
					}
					auto B = [](float v) { return (uint32_t)std::clamp((int)std::lround(v * 255), 0, 255); };
					db[i] = (p & 0xFF000000) | (B(rr + o) << 16) | (B(gg + o) << 8) | B(bb + o);
				}
				m->BitsChanged();
			}
			sCache[theSrc] = m;
			return m != nullptr ? m : theSrc;
		}

		Image* HeroImage(int theHero)
		{
			switch (theHero)
			{
			case HERO_ITCHY: return IMAGE_ITCHY;
			case HERO_CLYDE: return IMAGE_CLYDE;
			case HERO_RHUBARB: return IMAGE_RHUBARB;
			case HERO_ANGIE: return IMAGE_ANGIE;
			case HERO_PRESTO: return IMAGE_PRESTO;
			case HERO_NIKO: return IMAGE_NIKO;
			case HERO_MERYL: return IMAGE_MERYL;
			case HERO_SHRAPNEL: return IMAGE_SHRAPNEL;
			default: return Neon(IMAGE_STINKY);
			}
		}

		Image* HeroPortrait(int theHero)
		{
			switch (theHero)
			{
			case HERO_ITCHY: return IMAGE_SCL_ITCHY;
			case HERO_CLYDE: return IMAGE_SCL_CLYDE;
			case HERO_RHUBARB: return IMAGE_SCL_RHUBARB;
			case HERO_ANGIE: return IMAGE_SCL_ANGIE;
			case HERO_PRESTO: return IMAGE_SCL_PRESTO;
			case HERO_NIKO: return IMAGE_SCL_NIKO;
			case HERO_MERYL: return IMAGE_SCL_MERYL;
			case HERO_SHRAPNEL: return IMAGE_SCL_SHRAPNEL;
			default: return Neon(IMAGE_SCL_STINKY);
			}
		}

		// Sprites that face the viewer (no mirroring when they turn).
		static bool FacesFront(int theHero) { return theHero == HERO_RHUBARB || theHero == HERO_NIKO; }

		static Image* Backdrop(int theArena)
		{
			if (theArena == kTrench)
				return IMAGE_AQUARIUM6 != nullptr ? IMAGE_AQUARIUM6 : IMAGE_AQUARIUM3;
			return theArena == 0 ? IMAGE_AQUARIUM1 : IMAGE_AQUARIUM4;
		}

		void PlaySound(uint8_t theSound)
		{
			WinFishApp* anApp = (WinFishApp*)gSexyAppBase;
			if (anApp == nullptr)
				return;
			int anId = -1;
			switch (theSound)
			{
			case SND_HIT: anId = SOUND_HIT_ID; break;
			case SND_ZAP: anId = SOUND_ZAP_ID; break;
			case SND_SLASH: anId = SOUND_PUNCH_ID; break;
			case SND_TOWER: anId = SOUND_PEARL_ID; break;
			case SND_EXPLODE: anId = SOUND_EXPLODE_ID; break;
			case SND_COIN: anId = SOUND_POINTS_ID; break;
			case SND_DIAMOND: anId = SOUND_DIAMOND_ID; break;
			case SND_BUY: anId = SOUND_BUY_ID; break;
			case SND_BUZZER: anId = SOUND_BUZZER_ID; break;
			case SND_DIE: anId = SOUND_DIE_ID; break;
			case SND_LEVEL: anId = SOUND_TREASURE_ID; break;
			case SND_WARP: anId = SOUND_UNLEASH_ID; break;
			case SND_HEAL: anId = SOUND_HEAL_ID; break;
			case SND_SHIELD: anId = SOUND_ZZAM_ID; break;
			case SND_ROAR: anId = SOUND_ROAR_ID; break;
			case SND_SPLASH: anId = SOUND_DROPFOOD_ID; break;
			case SND_CHOMP: anId = SOUND_CHOMP_ID; break;
			case SND_LASER: anId = SOUND_HIT_ID; break;
			case SND_ALARM: anId = SOUND_AWOOGA_ID; break;
			case SND_GROW: anId = SOUND_GROW_ID; break;
			case SND_STINK: anId = SOUND_FART_ID; break;
			case SND_THUNDER: anId = SOUND_EEL2_ID; break;
			case SND_CARD: anId = SOUND_RICOCHET_ID; break;
			case SND_SING: anId = SOUND_SING_ID; break;
			case SND_MISSILE: anId = SOUND_MISSLE_ID; break;
			case SND_CLAM_OPEN: anId = SOUND_NIKOOPEN_ID; break;
			case SND_CLAM_CLOSE: anId = SOUND_NIKOCLOSE_ID; break;
			case SND_EVOLVE: anId = SOUND_CROWNED_ID; break;
			case SND_SCREAM: anId = SOUND_PRIMALSCREAM_ID; break;
			case SND_BIG_SPLASH: anId = SOUND_SPLASHBIG_ID; break;
			case SND_SONAR: anId = SOUND_SONAR_ID; break;
			case SND_TONE: anId = SOUND_TONEHI_ID; break;
			case SND_BOOM: anId = SOUND_EXPLOSION1_ID; break;
			default: break;
			}
			if (anId >= 0)
				anApp->PlaySample(anId);
		}

		///////////////////////////////////////////////////////////////////////
		// The scenery
		///////////////////////////////////////////////////////////////////////
		// A small copy of part of an image, made once. Textured triangles need a texture
		// that fits in one piece; the 640x480 paintings don't.
		static MemoryImage* Cut(Image* theSrc, int x, int y, int w, int h)
		{
			static std::map<std::tuple<Image*, int, int, int, int>, MemoryImage*> sCache;
			if (theSrc == nullptr)
				return nullptr;
			auto aKey = std::make_tuple(theSrc, x, y, w, h);
			auto it = sCache.find(aKey);
			if (it != sCache.end())
				return it->second;
			MemoryImage* aSrc = dynamic_cast<MemoryImage*>(theSrc);
			uint32_t* sb = aSrc != nullptr ? aSrc->GetBits() : nullptr;
			MemoryImage* m = nullptr;
			if (sb != nullptr)
			{
				m = new MemoryImage(gSexyAppBase);
				m->Create(w, h);
				uint32_t* db = m->GetBits();
				for (int j = 0; j < h; j++)
					for (int i = 0; i < w; i++)
					{
						int sx = std::clamp(x + i, 0, aSrc->mWidth - 1), sy = std::clamp(y + j, 0, aSrc->mHeight - 1);
						db[j * w + i] = sb[sy * aSrc->mWidth + sx] | 0xFF000000;
					}
				m->BitsChanged();
			}
			sCache[aKey] = m;
			return m;
		}

		static MemoryImage* StoneTex() { return Cut(IMAGE_AQUARIUM3, 410, 330, 52, 48); }

		// A polygon filled with a texture from a region of an image (u0,v0)-(u1,v1).
		static void TexturedPoly(Graphics* g, Image* theTex, const std::vector<Vec>& thePoly, float u0, float v0, float u1, float v1, uint32_t theRim)
		{
			if (theTex == nullptr || thePoly.size() < 3)
				return;
			float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
			Vec c;
			for (const Vec& p : thePoly)
			{
				x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y);
				c += p;
			}
			c = c * (1.0f / thePoly.size());
			auto UV = [&](Vec p, float& u, float& v) {
				u = u0 + (u1 - u0) * (p.x - x0) / std::max(1.0f, x1 - x0);
				v = v0 + (v1 - v0) * (p.y - y0) / std::max(1.0f, y1 - y0);
			};
			std::vector<TriVertex> aTris;
			float cu, cv;
			UV(c, cu, cv);
			for (size_t i = 0; i < thePoly.size(); i++)
			{
				const Vec& a = thePoly[i];
				const Vec& b = thePoly[(i + 1) % thePoly.size()];
				float au, av, bu, bv;
				UV(a, au, av);
				UV(b, bu, bv);
				aTris.push_back(TriVertex(SX(c.x), SY(c.y), cu, cv, 0xFFFFFFFF));
				aTris.push_back(TriVertex(SX(a.x), SY(a.y), au, av, theRim));
				aTris.push_back(TriVertex(SX(b.x), SY(b.y), bu, bv, theRim));
			}
			g->DrawTrianglesTex(theTex, (const TriVertex(*)[3])aTris.data(), (int)aTris.size() / 3);
		}

		static void DrawBackdrop(Graphics* g, int theArena, uint32_t theNow, bool theFull)
		{
			Image* b = Backdrop(theArena);
			Rect aDest((int)SX(0), (int)SY(0), (int)std::ceil(kWorldW * gView.mScale), (int)std::ceil(kWorldH * gView.mScale));
			if (b != nullptr)
				g->DrawImage(b, aDest, Rect(0, 0, b->mWidth, b->mHeight));
			// Deep water: the painting becomes distant scenery behind the play. The Trench is
			// darker and a little purple (the alien lair's painting).
			g->SetColor(theArena == kTrench ? Color(10, 4, 34, 175) : Color(0, 18, 44, 160));
			g->FillRect(aDest);
			if (!theFull)
				return;
			// Light rays.
			for (int i = 0; i < 5; i++)
			{
				float x = 180.0f + i * 250 + 40 * std::sin(theNow / 3000.0f + i);
				Point p[4] = { Point((int)SX(x), (int)SY(0)), Point((int)SX(x + 68), (int)SY(0)), Point((int)SX(x + 180), (int)SY(kWorldH - 80)), Point((int)SX(x + 40), (int)SY(kWorldH - 80)) };
				g->SetColor(theArena == kTrench ? Color(220, 180, 255, 10) : Color(200, 230, 255, 14));
				g->PolyFill(p, 4, true);
			}
			if (theArena == kTrench)
			{
				// Marine snow drifting down.
				for (int i = 0; i < 40; i++)
				{
					float x = std::fmod(i * 131.0f + theNow / (60.0f + i % 7 * 9), kWorldW);
					float y = std::fmod(i * 71.0f + theNow / (25.0f + i % 5 * 6), kWorldH);
					Disc(g, SX(x), SY(y), 1.2f + (i % 3) * 0.4f, Color(220, 210, 255, 50 + (i % 4) * 15), 6);
				}
			}
		}

		static void DrawFloor(Graphics* g, int theArena)
		{
			const std::vector<Vec>& f = MapOf(theArena).mFloor;
			// The painting's own sand, tiled along the floor in 256-unit columns.
			MemoryImage* b = theArena == kTrench ? Cut(Backdrop(theArena), 120, 420, 256, 56) : Cut(Backdrop(theArena), 100, 404, 256, 64);
			if (b == nullptr)
				return;
			const float kTile = 256;
			std::vector<TriVertex> v;
			uint32_t aDark = theArena == kTrench ? 0xFF5A4A70 : 0xFF8A8A8A;
			for (float x = 0; x < kWorldW; x += 32)
			{
				float x1 = std::min(kWorldW, x + 32);
				float u0 = std::fmod(x, kTile) / kTile, u1 = u0 + (x1 - x) / kTile;
				float y0 = FloorY(theArena, x), y1 = FloorY(theArena, x1);
				TriVertex t0(SX(x), SY(y0), u0, 0, 0xFFFFFFFF), t1(SX(x1), SY(y1), u1, 0, 0xFFFFFFFF);
				TriVertex b0(SX(x), SY(kWorldH), u0, 1, aDark), b1(SX(x1), SY(kWorldH), u1, 1, aDark);
				v.push_back(t0); v.push_back(t1); v.push_back(b1);
				v.push_back(t0); v.push_back(b1); v.push_back(b0);
			}
			g->DrawTrianglesTex(b, (const TriVertex(*)[3])v.data(), (int)v.size() / 3);
			g->SetColor(Color(255, 240, 200, 90));
			for (size_t i = 1; i < f.size(); i++)
				g->DrawLine((int)SX(f[i - 1].x), (int)SY(f[i - 1].y), (int)SX(f[i].x), (int)SY(f[i].y));
		}

		// Walls are solid slate blocks (D35): a bevel (edges facing up are lit, edges facing
		// down are shaded), a flat face, and a thick dark outline. Trench rock is darker.
		static void DrawWalls(Graphics* g, int theArena)
		{
			bool aDeep = theArena == kTrench;
			const Color kLit = aDeep ? Color(196, 180, 236) : Color(228, 238, 250);
			const Color kSide = aDeep ? Color(134, 116, 176) : Color(168, 182, 202);
			const Color kShade = aDeep ? Color(54, 40, 86) : Color(72, 84, 106);
			const Color kFace = aDeep ? Color(112, 96, 156) : Color(150, 166, 188);
			const Color kEdge(12, 10, 22);
			const float kBevel = 11, kOutline = 3.2f;
			for (const WallDef& w : MapOf(theArena).mWalls)
			{
				const std::vector<Vec>& p = w.mPoly;
				size_t n = p.size();
				if (n < 3)
					continue;
				Vec c;
				for (const Vec& q : p)
					c += q;
				c = c * (1.0f / n);
				float aRadius = 0;
				for (const Vec& q : p)
					aRadius += Dist(q, c);
				aRadius /= n;
				for (size_t i = 0; i < n; i++)
				{
					const Vec& a = p[i];
					const Vec& b = p[(i + 1) % n];
					Vec anOut(b.y - a.y, a.x - b.x);
					if ((anOut.x * ((a.x + b.x) / 2 - c.x) + anOut.y * ((a.y + b.y) / 2 - c.y)) < 0)
						anOut = anOut * -1.0f;
					float aUp = -anOut.y / std::max(0.01f, Len(anOut));
					g->SetColor(aUp > 0.35f ? kLit : (aUp < -0.35f ? kShade : kSide));
					Point t[3] = { Point((int)SX(c.x), (int)SY(c.y)), Point((int)SX(a.x), (int)SY(a.y)), Point((int)SX(b.x), (int)SY(b.y)) };
					g->PolyFill(t, 3, true);
				}
				float k = std::max(0.3f, 1 - kBevel / std::max(1.0f, aRadius));
				std::vector<Point> aFace;
				for (const Vec& q : p)
					aFace.push_back(Point((int)SX(c.x + (q.x - c.x) * k), (int)SY(c.y + (q.y - c.y) * k)));
				g->SetColor(kFace);
				g->PolyFill(aFace.data(), (int)aFace.size(), true);
				g->SetColor(kEdge);
				for (size_t i = 0; i < n; i++)
				{
					const Vec& a = p[i];
					const Vec& b = p[(i + 1) % n];
					Vec d = b - a;
					Vec m = Vec(-d.y, d.x) * (kOutline / std::max(0.01f, Len(d)));
					Point q[4] = { Point((int)SX(a.x + m.x), (int)SY(a.y + m.y)), Point((int)SX(b.x + m.x), (int)SY(b.y + m.y)),
						Point((int)SX(b.x - m.x), (int)SY(b.y - m.y)), Point((int)SX(a.x - m.x), (int)SY(a.y - m.y)) };
					g->PolyFill(q, 4, true);
					Disc(g, SX(a.x), SY(a.y), kOutline * gView.mScale, kEdge, 8);
				}
			}
		}

		// Swaying kelp; theFront draws a see-through layer over whatever hides inside.
		static void DrawKelp(Graphics* g, int theArena, uint32_t theNow, bool theFront)
		{
			const std::vector<KelpDef>& aKelp = MapOf(theArena).mKelp;
			bool aDeep = theArena == kTrench;
			for (size_t k = 0; k < aKelp.size(); k++)
			{
				const KelpDef& p = aKelp[k];
				int n = std::max(3, (int)((p.mX1 - p.mX0) / 16));
				for (int s = 0; s < n; s++)
				{
					if (theFront != (s % 2 == 1))
						continue;
					float x = p.mX0 + (s + 0.5f) * (p.mX1 - p.mX0) / n;
					float aTop = p.mY0 + ((s * 37 + (int)k * 13) % 50);
					float aBottom = std::min(p.mY1, FloorY(theArena, x)) + 4;
					int aSegs = 10;
					Color c = theFront ? (aDeep ? Color(120, 70, 170, 150) : Color(40, 160, 70, 150))
						: (aDeep ? Color(80 + (s * 20) % 40, 50, 120 + (s * 17) % 50, 235) : Color(30 + (s * 20) % 40, 120 + (s * 17) % 50, 50, 235));
					for (int i = 0; i < aSegs; i++)
					{
						float t0 = (float)i / aSegs, t1 = (float)(i + 1) / aSegs;
						float y0 = aBottom + (aTop - aBottom) * t0, y1 = aBottom + (aTop - aBottom) * t1;
						float w0 = 7 * (1 - t0 * 0.6f), w1 = 7 * (1 - t1 * 0.6f);
						float sw0 = std::sin(theNow / 700.0f + s * 1.3f + t0 * 3) * 14 * t0;
						float sw1 = std::sin(theNow / 700.0f + s * 1.3f + t1 * 3) * 14 * t1;
						Point q[4] = { Point((int)SX(x + sw0 - w0), (int)SY(y0)), Point((int)SX(x + sw0 + w0), (int)SY(y0)),
							Point((int)SX(x + sw1 + w1), (int)SY(y1)), Point((int)SX(x + sw1 - w1), (int)SY(y1)) };
						g->SetColor(c);
						g->PolyFill(q, 4, true);
						if (i % 2 == 1 && !theFront)
							Disc(g, SX(x + sw1 + (i % 4 == 1 ? 10.0f : -10.0f)), SY(y1), 7 * gView.mScale, aDeep ? Color(170, 110, 230, 220) : Color(60, 190, 90, 220), 8);
					}
				}
			}
		}

		static void Warp(Graphics* g, Vec p, float r, uint32_t theNow, const Color& theRim, float theScale)
		{
			Disc(g, SX(p.x), SY(p.y), r * 1.25f * gView.mScale, Color(theRim.mRed / 2, theRim.mGreen / 3, theRim.mBlue, 60));
			if (IMAGE_WARPHOLE != nullptr)
				Sprite(g, IMAGE_WARPHOLE, (theNow / 70) % 17, 0, p.x, p.y, theScale, false);
			Ring(g, SX(p.x), SY(p.y), r * gView.mScale, Color(theRim.mRed, theRim.mGreen, theRim.mBlue, 90 + (int)(60 * std::sin(theNow / 300.0f))), 2);
		}

		static void DrawPortals(Graphics* g, int theArena, int theMyTeam, uint32_t theNow, bool theFull)
		{
			const MapDef& m = MapOf(theArena);
			if (theArena == kTrench)
			{
				// A gate at each end, ringed in its team's color; the lane between them.
				for (int t = 0; t < 2; t++)
				{
					Warp(g, m.mGate[t], m.mGateR, theNow, TeamColor(t), 0.7f);
					if (theFull)
						Centered(g, FONT_TINYBOLD, t == theMyTeam ? "HOME" : "RIVAL", (int)SX(m.mGate[t].x), (int)SY(m.mGate[t].y - m.mGateR - 12), TeamColor(t));
				}
				if (theFull)
				{
					const std::vector<Vec>& l = m.mTrenchLane;
					for (size_t i = 1; i < l.size(); i++)
						for (float t = 0; t < 1; t += 0.08f)
						{
							Vec p = Lerp(l[i - 1], l[i], t);
							Disc(g, SX(p.x), SY(p.y + 40), 3 * gView.mScale, Color(230, 200, 255, 26), 6);
						}
				}
				return;
			}
			Warp(g, m.mPortal, m.mPortalR, theNow, Color(210, 160, 255), 0.62f);
			if (theFull)
				Centered(g, FONT_TINYBOLD, "TO THE TRENCH", (int)SX(m.mPortal.x), (int)SY(m.mPortal.y - m.mPortalR - 10), Color(220, 190, 255, 200));
			// Floor pads, and the portal's beam down to the floor (walkers ride it).
			for (int i = 0; i < 2; i++)
			{
				Vec p = m.mPad[i];
				float pulse = 0.8f + 0.2f * std::sin(theNow / 250.0f + i);
				for (int k = 0; k < 3; k++)
				{
					Point q[4] = { Point((int)SX(p.x - m.mPadR * pulse + k * 6), (int)SY(p.y - 2)), Point((int)SX(p.x + m.mPadR * pulse - k * 6), (int)SY(p.y - 2)),
						Point((int)SX(p.x + m.mPadR * 0.5f - k * 4), (int)SY(p.y - 60 + k * 14)), Point((int)SX(p.x - m.mPadR * 0.5f + k * 4), (int)SY(p.y - 60 + k * 14)) };
					g->SetColor(Color(180, 120, 255, 30 + k * 20));
					g->PolyFill(q, 4, true);
				}
				Ring(g, SX(p.x), SY(p.y - 4), m.mPadR * gView.mScale, Color(200, 150, 255, 150));
			}
			float x = m.mPortal.x;
			Point q[4] = { Point((int)SX(x - 16), (int)SY(m.mPortal.y + m.mPortalR)), Point((int)SX(x + 16), (int)SY(m.mPortal.y + m.mPortalR)),
				Point((int)SX(x + 34), (int)SY(FloorY(theArena, x))), Point((int)SX(x - 34), (int)SY(FloorY(theArena, x))) };
			g->SetColor(Color(190, 140, 255, 16));
			g->PolyFill(q, 4, true);
		}

		///////////////////////////////////////////////////////////////////////
		// Structures
		///////////////////////////////////////////////////////////////////////
		static void DrawTower(Graphics* g, int theArena, int theIndex, const ArenaSnap& s, uint32_t theNow, bool theFiring)
		{
			const MapDef& m = TankMap();
			Vec p = m.mTower[theIndex];
			float aBase = FloorY(theArena, p.x);
			bool aAlive = s.mTowerHp[theIndex] > 0;
			std::vector<Vec> aCol = { Vec(p.x - 34, aBase + 4), Vec(p.x - 26, p.y - 20), Vec(p.x + 26, p.y - 20), Vec(p.x + 34, aBase + 4) };
			if (!aAlive)
				aCol = { Vec(p.x - 40, aBase + 4), Vec(p.x - 30, aBase - 30), Vec(p.x - 6, aBase - 44), Vec(p.x + 20, aBase - 26), Vec(p.x + 40, aBase + 4) };
			TexturedPoly(g, StoneTex(), aCol, 0, 0, 1, 1, 0xFF707080);
			if (!aAlive)
				return;
			const Color& c = TeamColor(theArena);
			Disc(g, SX(p.x), SY(p.y - 34), 44 * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, 50));
			int aRow = theFiring ? 1 : 0;
			int aCol2 = theFiring ? 9 : (int)((theNow / 150 + theIndex * 3) % 10);
			bool aBlind = (s.mTowerBlind >> theIndex) & 1;
			Sprite(g, IMAGE_NIKO, aCol2, aRow, p.x, p.y - 44, 1.45f, false, aBlind ? Color(140, 200, 120) : Color(255, 255, 255), aBlind);
			if (s.mTowerShield[theIndex] > 0)
				Ring(g, SX(p.x), SY(p.y - 30), 70 * gView.mScale, Color(255, 245, 170, 170), 2);
			float aFrac = s.mTowerHp[theIndex] / std::max(1.0f, s.mTowerMax);
			Bar(g, (int)SX(p.x - 44), (int)SY(p.y - 104), (int)(88 * gView.mScale), gView.mScale < 0.3f ? 1 : 3, aFrac, aFrac > 0.5f ? Color(90, 230, 110) : (aFrac > 0.25f ? Color(240, 210, 60) : Color(240, 80, 60)));
		}

		static void DrawCore(Graphics* g, int theArena, const ArenaSnap& s, uint32_t theNow)
		{
			const MapDef& m = TankMap();
			Vec p = m.mCore;
			const Color& c = TeamColor(theArena);
			bool aOpen = s.mTowerHp[0] <= 0 && s.mTowerHp[1] <= 0;
			Disc(g, SX(p.x), SY(p.y + 10), 96 * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, 44));
			if (s.mCoreHp > 0)
				Sprite(g, IMAGE_MONEY, (theNow / 90) % 10, 4, p.x, p.y - 6, 2.5f, false);
			else
				Sprite(g, IMAGE_MONEY, 0, 4, p.x, p.y, 2.2f, false, Color(90, 80, 80), true);
			if (!aOpen && s.mCoreHp > 0)
			{
				Ring(g, SX(p.x), SY(p.y - 10), 96 * gView.mScale, Color(200, 230, 255, 70 + (int)(30 * std::sin(theNow / 400.0f))), 1);
				Disc(g, SX(p.x), SY(p.y - 10), 92 * gView.mScale, Color(200, 230, 255, 18));
			}
			if (s.mCoreShield > 0)
				Ring(g, SX(p.x), SY(p.y - 10), 104 * gView.mScale, Color(255, 245, 170, 180), 2);
			float aFrac = s.mCoreHp / kCoreHealth;
			Bar(g, (int)SX(p.x - 80), (int)SY(p.y - 100), (int)(160 * gView.mScale), gView.mScale < 0.3f ? 1 : 4, aFrac, aOpen ? Color(240, 90, 60) : Color(255, 210, 80));
		}

		///////////////////////////////////////////////////////////////////////
		// Creatures
		///////////////////////////////////////////////////////////////////////
		static void DrawFish(Graphics* g, const FishSnap& f, uint32_t theNow, bool theMarkHungry)
		{
			int aFrame = (int)((theNow / 90 + f.mId * 3) % 10);
			bool aMirror = (f.mFlags & FF_RIGHT) != 0;
			bool aHungry = (f.mFlags & FF_HUNGRY) != 0;
			if (f.mFlags & FF_DYING)
			{
				if (f.mKind == FISH_BREEDER)
				{
					Sprite(g, IMAGE_HUNGRYBREEDER, aFrame, 3, f.mPos.x, f.mPos.y, 1.0f, aMirror, Color(170, 170, 170, 160), true);
					return;
				}
				Sprite(g, IMAGE_SMALLDIE, aFrame, f.mKind == FISH_CARNIVORE ? 4 : std::min<int>(f.mSize, 2), f.mPos.x, f.mPos.y, 1.0f, aMirror, Color(255, 255, 255, 160));
				return;
			}
			if (theMarkHungry && aHungry)
				Disc(g, SX(f.mPos.x), SY(f.mPos.y), 34 * gView.mScale, Color(255, 80, 60, 70 + (int)(50 * std::sin(theNow / 150.0f))));
			switch (f.mKind)
			{
			case FISH_GUPPY:
				Sprite(g, (f.mFlags & FF_EATING) ? IMAGE_SMALLEAT : (aHungry ? IMAGE_HUNGRYSWIM : IMAGE_SMALLSWIM), aFrame, std::min<int>(f.mSize, 2), f.mPos.x, f.mPos.y, 1.0f, aMirror);
				break;
			case FISH_BREEDER:
				Sprite(g, aHungry ? IMAGE_HUNGRYBREEDER : IMAGE_BREEDER, aFrame, 3, f.mPos.x, f.mPos.y, 1.0f, aMirror);
				break;
			default:
				Sprite(g, (f.mFlags & FF_EATING) ? IMAGE_SMALLEAT : (aHungry ? IMAGE_HUNGRYSWIM : IMAGE_SMALLSWIM), aFrame, 4, f.mPos.x, f.mPos.y, 1.15f, aMirror);
				break;
			}
		}

		Image* MinionImage(int theKind)
		{
			switch (theKind)
			{
			case MIN_MINI: return IMAGE_MINISYLV;
			case MIN_SYLV: return IMAGE_SYLV;
			case MIN_GUS: case MIN_CAMP_GUS: return IMAGE_GUS;
			case MIN_BALROG: case MIN_CAMP_BALROG: return IMAGE_BALROG;
			case MIN_SQUID: case MIN_PSYCHO: return IMAGE_PSYCHOSQUID;
			case MIN_BOSS: return IMAGE_BOSS;
			default: return IMAGE_DESTRUCTOR;
			}
		}

		static void DrawMinion(Graphics* g, const MinionSnap& m, uint32_t theNow, bool theFull)
		{
			Image* anImg = MinionImage(m.mKind);
			int aFrame = (int)((theNow / 70 + m.mId) % 10);
			const MinionDef& d = MinionDefOf(m.mKind);
			bool aMonster = m.mTeam == kNeutralTeam;
			float aScale = m.mKind == MIN_MINI ? 1.0f : 0.8f;
			bool aMirror = (m.mFlags & MF_RIGHT) != 0;
			int aRow = (m.mKind == MIN_GUS || m.mKind == MIN_CAMP_GUS) && (m.mFlags & MF_ATTACKING) ? 2 : 0;
			switch (m.mKind)
			{
			case MIN_SQUID:			// the Squid's gift: calm blue, smaller
				aScale = 0.75f;
				aRow = 2;
				aMirror = false;
				break;
			case MIN_PSYCHO:		// red when angry, blue when calm
				aScale = 0.95f;
				aRow = (m.mFlags & MF_ANGRY) ? 0 : 2;
				aMirror = false;
				break;
			case MIN_BOSS:
				aScale = 1.25f;
				aRow = 0;
				aMirror = false;
				break;
			case MIN_CAMP_GUS: case MIN_CAMP_BALROG:
				aScale = 0.85f;
				break;
			default:
				break;
			}
			Color aTint = (m.mFlags & MF_CHARMED) ? Color(255, 150, 220) : Color(255, 255, 255);
			float rr = d.mRadius * 0.8f;
			const Color& tc = TeamColor(m.mTeam);
			if (aMonster)
			{
				// A monster: a glow ring, brighter when it's fighting.
				bool aAngry = (m.mFlags & MF_ANGRY) != 0;
				Disc(g, SX(m.mPos.x), SY(m.mPos.y + d.mRadius * 0.6f), d.mRadius * 0.9f * gView.mScale, Color(150, 70, 220, aAngry ? 70 : 34));
				Ring(g, SX(m.mPos.x), SY(m.mPos.y), (d.mRadius + 6) * gView.mScale, aAngry ? Color(255, 90, 80, 170) : Color(200, 140, 255, 90), aAngry ? 2 : 1);
			}
			else
			{
				// Whose side it's on: a glow in that team's color (the Trench has both).
				Disc(g, SX(m.mPos.x), SY(m.mPos.y), rr * 1.25f * gView.mScale, Color(tc.mRed, tc.mGreen, tc.mBlue, 70), 14);
				Ring(g, SX(m.mPos.x), SY(m.mPos.y), rr * 1.25f * gView.mScale, Color(tc.mRed, tc.mGreen, tc.mBlue, 170), 1);
			}
			if (m.mFlags & MF_RALLIED)
				Disc(g, SX(m.mPos.x), SY(m.mPos.y), (rr + 4) * gView.mScale, Color(255, 120, 200, 50));
			Sprite(g, anImg, aFrame, aRow, m.mPos.x, m.mPos.y, aScale, aMirror, aTint, (m.mFlags & MF_CHARMED) != 0);
			float r = d.mRadius;
			if (m.mHpFrac < 0.999f || (theFull && (aMonster || m.mKind == MIN_SQUID)))
			{
				int w = (int)((aMonster ? r * 2.4f : r * 2) * gView.mScale);
				Bar(g, (int)SX(m.mPos.x) - w / 2, (int)SY(m.mPos.y - r - 16), w, aMonster && gView.mScale > 0.3f ? 3 : 2, m.mHpFrac, aMonster ? Color(200, 120, 255) : (m.mTeam == 0 ? Color(240, 170, 60) : Color(90, 190, 255)));
			}
			if (aMonster && theFull && (m.mKind == MIN_PSYCHO || m.mKind == MIN_BOSS))
				Centered(g, FONT_TINYBOLD, d.mName, (int)SX(m.mPos.x), (int)SY(m.mPos.y - r - 22), Color(230, 200, 255));
			if (m.mFlags & (MF_STUNNED | MF_ASLEEP))
				Sprite(g, IMAGE_ZZZ, 0, 0, m.mPos.x + r * 0.5f, m.mPos.y - r - 20, 1.0f, false);
		}

		// What the hero's sprite and scale are (evolution, buffs, Fortress).
		static float HeroScale(const HeroSnap& h)
		{
			float s = 1.35f;
			if (h.mLevel >= kEvolveLevel)
				s *= kEvolveScale;
			if (h.mBuffS[BUFF_GUS] > 0)
				s *= kGusScale;
			if (h.mFlags & HF_FORTRESS)
				s *= 1.8f;
			return s;
		}

		// Recently hurt heroes flash white (per player, on this screen).
		static uint32_t sFlashUntil[kMaxPlayers] = { 0, 0, 0, 0 };
		static float sLastHp[kMaxPlayers] = { -1, -1, -1, -1 };

		static void DrawHero(Graphics* g, const HeroSnap& h, uint32_t theNow, bool theMine, const std::string& theName, bool theFull)
		{
			const HeroDef& d = HeroDefOf(h.mHero);
			const Color& c = TeamColor(h.mTeam);
			if (!(h.mFlags & HF_ALIVE))
			{
				Sprite(g, HeroImage(h.mHero), 0, 0, h.mPos.x, h.mPos.y, 1.2f, false, Color(120, 120, 140, 110), true);
				return;
			}
			int aAlpha = (h.mFlags & HF_HIDDEN) ? 110 : ((h.mFlags & HF_UNTARGETABLE) ? 90 : 255);
			bool aRight = (h.mFlags & HF_RIGHT) != 0;
			float r = d.mRadius;
			float aScale = HeroScale(h);
			bool aEvolved = h.mLevel >= kEvolveLevel;
			// Auras under the hero: evolution glow, the Boss's surge, Balrog's fire, Anthem.
			if (h.mBuffS[BUFF_BOSS] > 0)
			{
				float p = 0.5f + 0.5f * std::sin(theNow / 120.0f);
				g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
				Disc(g, SX(h.mPos.x), SY(h.mPos.y), (r * 2.1f + 8 * p) * gView.mScale, Color(255, 90, 30, 70));
				g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), (r * 2.2f + 8 * p) * gView.mScale, Color(255, 200, 60, 200), 2);
			}
			if (aEvolved)
			{
				g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
				Disc(g, SX(h.mPos.x), SY(h.mPos.y), r * 1.7f * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, 38 + (int)(14 * std::sin(theNow / 300.0f))));
				g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
			}
			if (h.mFlags & HF_ANTHEM)
				for (int k = 0; k < 3; k++)
				{
					float a = theNow / 300.0f + k * 2.09f;
					Text(g, FONT_TINYBOLD, k == 1 ? "~" : "#", (int)SX(h.mPos.x + std::cos(a) * r * 1.6f), (int)SY(h.mPos.y + std::sin(a) * r * 1.2f), Color(255, 150, 230, aAlpha));
				}
			// A ring in the owner's color.
			Ring(g, SX(h.mPos.x), SY(h.mPos.y + (d.mWalker ? r * 0.6f : 0)), (r * aScale / 1.35f + 6) * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, aAlpha * 7 / 10), aEvolved ? 3 : 2);
			Image* anImg = HeroImage(h.mHero);
			int aFrame = (int)((theNow / 75) % 10);
			int aRow = 0;
			bool aMirror = FacesFront(h.mHero) ? false : aRight;
			switch (h.mHero)
			{
			case HERO_ITCHY: aRow = (h.mFlags & (HF_ATTACKING | HF_STORM)) ? 2 : 0; break;
			case HERO_RHUBARB: aRow = (h.mFlags & HF_ATTACKING) ? 1 : 0; break;
			case HERO_SPEEDY: aRow = (h.mFlags & HF_IMMUNE) ? 2 : 0; if (h.mFlags & HF_IMMUNE) aFrame = 9; break;
			case HERO_NIKO:
				// Closed while shielded; open, pearl showing, when it attacks or holds the fort.
				aRow = (h.mFlags & HF_CLAM) ? 0 : ((h.mFlags & (HF_ATTACKING | HF_FORTRESS)) ? 1 : 0);
				aFrame = (h.mFlags & HF_CLAM) ? 0 : ((h.mFlags & (HF_ATTACKING | HF_FORTRESS)) ? 9 : aFrame);
				break;
			case HERO_MERYL: aRow = (h.mFlags & HF_ATTACKING) ? 2 : 0; if (h.mFlags & HF_ATTACKING) aFrame = 3 + aFrame % 3; break;
			default: break;
			}
			uint32_t aFlash = sFlashUntil[h.mPlayer % kMaxPlayers];
			bool aFlashing = !Elapsed(theNow, aFlash);
			if (h.mFlags & HF_STORM)
			{
				Transform t;
				t.RotateRad(theNow / 60.0f);
				t.Scale(aScale * gView.mScale, aScale * gView.mScale);
				g->DrawImageTransformF(anImg, t, anImg->GetCelRect(aFrame, 0), SX(h.mPos.x), SY(h.mPos.y));
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), 110 * gView.mScale, Color(220, 240, 255, 120), 1);
			}
			else
			{
				if (h.mHero == HERO_SPEEDY)
				{
					g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
					Sprite(g, anImg, aFrame, aRow, h.mPos.x, h.mPos.y, aScale * 1.12f, aMirror, Color(170, 60, 255, aAlpha * 65 / 100), true);
					g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				}
				Color aTint(255, 255, 255, aAlpha);
				bool aColorize = false;
				if (h.mHero == HERO_NIKO)
				{
					aTint = Color(255, 225, 190, aAlpha);	// warmer than the towers' clams
					aColorize = true;
				}
				Sprite(g, anImg, aFrame, aRow, h.mPos.x, h.mPos.y, aScale, aMirror, aTint, aColorize);
				if (aFlashing)
				{
					g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
					Sprite(g, anImg, aFrame, aRow, h.mPos.x, h.mPos.y, aScale, aMirror, Color(255, 255, 255, 150), true);
					g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				}
				if (h.mFlags & HF_BURNING)
					for (int k = 0; k < 4; k++)
					{
						float a = theNow / 160.0f + k * 1.57f;
						Disc(g, SX(h.mPos.x + std::cos(a) * r * 0.9f), SY(h.mPos.y - r * 0.4f + std::sin(a * 1.7f) * r * 0.4f), 5 * gView.mScale, Color(255, 120 + k * 30, 40, 170), 8);
					}
			}
			if (h.mFlags & HF_COPY)
			{
				// Presto in disguise: a sparkle gives it away, if you look closely.
				Sprite(g, IMAGE_SPARKLE, (theNow / 60) % 14, 0, h.mPos.x + r * 0.7f, h.mPos.y - r * 0.8f, 1.0f, false);
			}
			if (h.mFlags & HF_IMMUNE)
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), (r + 12) * gView.mScale, Color(255, 240, 150, 200), 2);
			if (h.mFlags & HF_CLAM)
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), (r * aScale / 1.35f + 14) * gView.mScale, Color(240, 200, 255, 200), 3);
			if (h.mShield > 0)
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), (r + 16) * gView.mScale, Color(255, 250, 200, 160), 1);
			if (h.mFlags & (HF_STUNNED | HF_ASLEEP))
				Sprite(g, IMAGE_ZZZ, 0, 0, h.mPos.x + 10, h.mPos.y - r - 30, 1.2f, false);
			if (h.mFlags & HF_GOLDRUSH)
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), (r + 20 + 6 * std::sin(theNow / 90.0f)) * gView.mScale, Color(255, 220, 60, 160), 2);
			if (!theFull)
			{
				Disc(g, SX(h.mPos.x), SY(h.mPos.y), 3, Color(c.mRed, c.mGreen, c.mBlue, 230), 8);
				return;
			}
			// Name, level and health.
			int bw = (int)(68 * aScale / 1.35f), bx = (int)SX(h.mPos.x) - bw / 2, by = (int)SY(h.mPos.y - r * aScale / 1.35f - 30);
			Bar(g, bx, by, bw, 3, h.mHp / std::max(1.0f, h.mMaxHp), theMine ? Color(110, 240, 110) : Color(250, 90, 80));
			if (h.mShield > 0)
				Bar(g, bx, by - 3, (int)(bw * std::min(1.0f, h.mShield / std::max(1.0f, h.mMaxHp))), 1, 1, Color(255, 250, 200), Color(0, 0, 0, 0));
			Disc(g, bx - 5.0f, by + 1.5f, 5, aEvolved ? Color(80, 40, 10, 230) : Color(20, 20, 40, 220), 12);
			Centered(g, FONT_TINYBOLD, std::to_string(h.mLevel), bx - 5, by + 5, aEvolved ? Color(255, 210, 90) : Color(255, 240, 180));
			std::string aName = theName;
			if (h.mStreak >= kShutdownStreak)
				aName += " *" + std::to_string(h.mStreak);
			Centered(g, FONT_TINY, aName, (int)SX(h.mPos.x), by - 3, Color(c.mRed, c.mGreen, c.mBlue, aAlpha));
		}

		///////////////////////////////////////////////////////////////////////
		// Effects
		///////////////////////////////////////////////////////////////////////
		static Color LookColor(uint8_t theLook)
		{
			switch (theLook)
			{
			case LOOK_ZAP: case LOOK_CLYDE_ATTACK: case LOOK_STATIC: return Color(120, 220, 255);
			case LOOK_ANGIE_ATTACK: case LOOK_HALO: case LOOK_RESURRECT: return Color(255, 235, 150);
			case LOOK_STINK: case LOOK_SLIME: return Color(140, 230, 80);
			case LOOK_GOLD: return Color(255, 210, 60);
			case LOOK_DESTRUCTOR: case LOOK_BOMB: case LOOK_MISSILE: return Color(255, 120, 60);
			case LOOK_CHARM: case LOOK_SIREN: case LOOK_ANTHEM: return Color(255, 140, 220);
			case LOOK_SLAM: case LOOK_LEAP: return Color(230, 200, 150);
			case LOOK_STORM: case LOOK_LUNGE: return Color(220, 250, 255);
			case LOOK_TELEPORT: case LOOK_SPARKLE: return Color(200, 160, 255);
			case LOOK_CARD: return Color(255, 255, 255);
			case LOOK_PEARL: case LOOK_FORTRESS: return Color(255, 225, 240);
			case LOOK_NOTE: return Color(255, 170, 240);
			case LOOK_SLEEP: return Color(170, 190, 255);
			case LOOK_MINE: return Color(255, 90, 60);
			case LOOK_INK: return Color(40, 30, 70);
			case LOOK_BURN: return Color(255, 110, 40);
			case LOOK_EVOLVE: return Color(255, 240, 160);
			case LOOK_BOSS: return Color(255, 80, 60);
			default: return Color(255, 255, 255);
			}
		}

		static void Lightning(Graphics* g, Vec a, Vec b, uint32_t theSeed, const Color& c)
		{
			Rng r(theSeed);
			Vec aPrev = a;
			int n = 7;
			for (int i = 1; i <= n; i++)
			{
				Vec p = Lerp(a, b, (float)i / n);
				if (i < n)
					p += Vec(r.Range(-18, 18), r.Range(-18, 18));
				g->SetColor(c);
				g->DrawLine((int)SX(aPrev.x), (int)SY(aPrev.y), (int)SX(p.x), (int)SY(p.y));
				g->SetColor(Color(255, 255, 255, c.mAlpha));
				g->DrawLine((int)SX(aPrev.x) + 1, (int)SY(aPrev.y), (int)SX(p.x) + 1, (int)SY(p.y));
				aPrev = p;
			}
		}

		// A playing card, a music note or a bomb in flight.
		static void Missile(Graphics* g, uint8_t theLook, Vec p, Vec theDir, uint32_t theNow, float theScale)
		{
			switch (theLook)
			{
			case LOOK_CARD:
			{
				Vec d = Norm(theDir), n(-d.y, d.x);
				float w = 9 * theScale, hh = 13 * theScale;
				Point q[4] = { Point((int)SX(p.x - d.x * hh - n.x * w), (int)SY(p.y - d.y * hh - n.y * w)), Point((int)SX(p.x + d.x * hh - n.x * w), (int)SY(p.y + d.y * hh - n.y * w)),
					Point((int)SX(p.x + d.x * hh + n.x * w), (int)SY(p.y + d.y * hh + n.y * w)), Point((int)SX(p.x - d.x * hh + n.x * w), (int)SY(p.y - d.y * hh + n.y * w)) };
				g->SetColor(Color(250, 250, 250));
				g->PolyFill(q, 4, true);
				Disc(g, SX(p.x), SY(p.y), 4 * gView.mScale * theScale, Color(220, 30, 50), 8);
				break;
			}
			case LOOK_NOTE:
				Text(g, FONT_JUNGLEFEVER12OUTLINE, (theNow / 150) % 2 ? "#" : "~", (int)SX(p.x) - 4, (int)SY(p.y) + 5, Color(255, 160, 240));
				break;
			case LOOK_PEARL:
				Sprite(g, IMAGE_PEARL, 0, 0, p.x, p.y, 0.45f * theScale, false);
				break;
			case LOOK_BOMB:
				Disc(g, SX(p.x), SY(p.y), 9 * gView.mScale * theScale, Color(40, 40, 50), 12);
				Disc(g, SX(p.x + 4), SY(p.y - 9), 3 * gView.mScale, Color(255, 200, 60), 6);
				break;
			case LOOK_MISSILE:
				Sprite(g, IMAGE_MISSILE, (theNow / 60) % 16, 0, p.x, p.y, 0.7f * theScale, false);
				break;
			case LOOK_SPARKLE:
				Sprite(g, IMAGE_SPARKLE, (theNow / 50) % 14, 0, p.x, p.y, 1.2f * theScale, false);
				break;
			default:
			{
				g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
				Sprite(g, IMAGE_ENERGYBALL, (theNow / 60) % 6, 0, p.x, p.y, (theLook == LOOK_ZAP ? 0.7f : 0.42f) * theScale, false, LookColor(theLook), true);
				g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				break;
			}
			}
		}

		static void DrawEffects(Graphics* g, const ViewState& v, int theArena, bool theUnder)
		{
			const Side& s = *v.mSide;
			for (const Effect& fx : s.mEffects)
			{
				const Event& e = fx.mEvent;
				if (e.mArena != theArena)
					continue;
				float t = (v.mNow - fx.mAt) / 1000.0f;
				if (t < 0)
					t = 0;
				Color c = LookColor(e.mParam);
				switch (e.mType)
				{
				case EV_ZONE:
				{
					if (!theUnder)
						break;
					float aLife = e.mMs / 1000.0f;
					if (t > aLife)
						break;
					int a = (int)(160 * std::min(1.0f, (aLife - t) / 0.4f));
					float R = e.mValue * gView.mScale;
					if (e.mParam == LOOK_SLIME)
					{
						Disc(g, SX(e.mA.x), SY(e.mA.y), R, Color(110, 210, 60, a / 2), 14);
						break;
					}
					if (e.mParam == LOOK_STINK || e.mParam == LOOK_INK)
					{
						Color k = e.mParam == LOOK_INK ? Color(30, 20, 60, a / 2) : Color(130, 190, 60, a / 3);
						for (int i = 0; i < 7; i++)
						{
							float ang = i * 0.9f + v.mNow / 900.0f;
							Disc(g, SX(e.mA.x + std::cos(ang) * e.mValue * 0.5f), SY(e.mA.y + std::sin(ang) * e.mValue * 0.35f), R * 0.45f, k, 14);
						}
						break;
					}
					if (e.mParam == LOOK_MISSILE)
					{
						// The marked area: a pulsing red crosshair.
						Ring(g, SX(e.mA.x), SY(e.mA.y), R, Color(255, 70, 50, a), 2);
						Ring(g, SX(e.mA.x), SY(e.mA.y), R * (0.5f + 0.15f * std::sin(v.mNow / 90.0f)), Color(255, 120, 60, a / 2), 1);
						g->SetColor(Color(255, 70, 50, a / 2));
						g->DrawLine((int)(SX(e.mA.x) - R), (int)SY(e.mA.y), (int)(SX(e.mA.x) + R), (int)SY(e.mA.y));
						g->DrawLine((int)SX(e.mA.x), (int)(SY(e.mA.y) - R), (int)SX(e.mA.x), (int)(SY(e.mA.y) + R));
						break;
					}
					Disc(g, SX(e.mA.x), SY(e.mA.y), R, Color(c.mRed, c.mGreen, c.mBlue, a / 4));
					Ring(g, SX(e.mA.x), SY(e.mA.y), R, Color(c.mRed, c.mGreen, c.mBlue, a), 1);
					if (e.mParam == LOOK_STATIC && ((v.mNow / 120) % 2 == 0))
					{
						Rng r(v.mNow / 120 + e.mId);
						for (int k = 0; k < 2; k++)
						{
							float a0 = r.Range(0, 6.28f), a1 = r.Range(0, 6.28f);
							Lightning(g, e.mA + Vec(std::cos(a0), std::sin(a0)) * e.mValue * 0.9f, e.mA + Vec(std::cos(a1), std::sin(a1)) * e.mValue * 0.9f, r.Next(), Color(150, 230, 255, a));
						}
					}
					break;
				}
				case EV_TURRET:
				{
					// Niko's turrets (both sides see them) and Shrapnel's mines (only he does).
					if (theUnder)
						break;
					bool aEnded = false;
					for (const Effect& o : s.mEffects)
						if (o.mEvent.mType == EV_TURRET_END && o.mEvent.mId == e.mId && o.mSeq > fx.mSeq && o.mEvent.mPlayer == e.mPlayer)
							aEnded = true;
					if (aEnded || t > e.mMs / 1000.0f)
						break;
					if (e.mParam == 1)
					{
						bool aArmed = t > 1.0f;
						Disc(g, SX(e.mA.x), SY(e.mA.y), 12 * gView.mScale, Color(60, 60, 70, 220), 12);
						for (int k = 0; k < 6; k++)
						{
							float a = k * 1.047f;
							Disc(g, SX(e.mA.x + std::cos(a) * 14), SY(e.mA.y + std::sin(a) * 14), 3 * gView.mScale, Color(90, 90, 100), 6);
						}
						Disc(g, SX(e.mA.x), SY(e.mA.y - 3), 3 * gView.mScale, aArmed && (v.mNow / 300) % 2 ? Color(255, 60, 40) : Color(120, 40, 40), 6);
						break;
					}
					const Color& tc = TeamColor(e.mPlayer);
					Disc(g, SX(e.mA.x), SY(e.mA.y + 6), 30 * gView.mScale, Color(tc.mRed, tc.mGreen, tc.mBlue, 50));
					Ring(g, SX(e.mA.x), SY(e.mA.y), HeroDefOf(HERO_NIKO).mAb[AB_Q].mRadius * gView.mScale, Color(tc.mRed, tc.mGreen, tc.mBlue, 26), 1);
					Sprite(g, IMAGE_NIKO, (int)((v.mNow / 120) % 10), 2, e.mA.x, e.mA.y - 10, 0.75f, false, Color(255, 230, 200), true);
					float aLeft = 1 - t / std::max(0.1f, e.mMs / 1000.0f);
					Bar(g, (int)SX(e.mA.x - 20), (int)SY(e.mA.y - 40), (int)(40 * gView.mScale), 1, aLeft, tc);
					break;
				}
				case EV_DECOY:
				{
					if (theUnder || t > e.mMs / 1000.0f)
						break;
					// Presto's decoy: to the rival it's Presto; to Presto a see-through copy.
					bool aMine = e.mPlayer == s.mTeam;
					HeroSnap aFake;
					aFake.mHero = (uint8_t)e.mId;
					aFake.mTeam = (uint8_t)e.mPlayer;
					aFake.mPlayer = (uint8_t)(e.mPlayer % kMaxPlayers);
					aFake.mFlags = HF_ALIVE | (e.mValue > 0.5f ? HF_RIGHT : 0) | (aMine ? HF_UNTARGETABLE : 0);
					aFake.mPos = e.mA + Vec(0, std::sin(v.mNow / 400.0f) * 4);
					aFake.mHp = aFake.mMaxHp = 1;
					aFake.mLevel = 1;
					const HeroSnap* o = aMine ? nullptr : s.OtherHero();
					if (o != nullptr)
					{
						aFake.mLevel = o->mLevel;
						aFake.mHp = o->mHp;
						aFake.mMaxHp = o->mMaxHp;
					}
					DrawHero(g, aFake, v.mNow, false, v.mNames[aFake.mPlayer], true);
					if (t > e.mMs / 1000.0f - 0.5f)
						Ring(g, SX(e.mA.x), SY(e.mA.y), (40 + 200 * (t - e.mMs / 1000.0f + 0.5f)) * gView.mScale, Color(200, 160, 255, 160), 2);
					break;
				}
				case EV_CONE:
				{
					if (!theUnder || t > e.mMs / 1000.0f)
						break;
					int a = (int)(150 * (1 - t / (e.mMs / 1000.0f)));
					Vec d = e.mB - e.mA;
					float l = Len(d), base = std::atan2(d.y, d.x), half = e.mValue * 3.14159f / 180;
					std::vector<Point> p;
					p.push_back(Point((int)SX(e.mA.x), (int)SY(e.mA.y)));
					for (int k = 0; k <= 8; k++)
					{
						float an = base - half + 2 * half * k / 8;
						p.push_back(Point((int)SX(e.mA.x + std::cos(an) * l * std::min(1.0f, t * 3)), (int)SY(e.mA.y + std::sin(an) * l * std::min(1.0f, t * 3))));
					}
					g->SetColor(Color(c.mRed, c.mGreen, c.mBlue, a / 2));
					g->PolyFill(p.data(), (int)p.size(), true);
					g->SetColor(Color(220, 230, 255, a));
					for (size_t k = 1; k < p.size(); k++)
						g->DrawLine(p[k - 1].mX, p[k - 1].mY, p[k].mX, p[k].mY);
					for (int k = 0; k < 4; k++)
					{
						float an = base + (k - 1.5f) * half * 0.5f;
						Vec q = e.mA + Vec(std::cos(an), std::sin(an)) * l * std::fmod(t * 2 + k * 0.25f, 1.0f);
						Text(g, FONT_TINYBOLD, "z", (int)SX(q.x), (int)SY(q.y), Color(220, 230, 255, a * 2));
					}
					break;
				}
				case EV_LINE:
				{
					if (theUnder || t > e.mMs / 1000.0f + 0.3f)
						break;
					float f = std::min(1.0f, t / std::max(0.05f, e.mMs / 1000.0f));
					Vec p = Lerp(e.mA, e.mB, f);
					int a = (int)(200 * (1 - std::max(0.0f, t - e.mMs / 1000.0f) / 0.3f));
					for (int k = 0; k < 3; k++)
					{
						Vec q = Lerp(e.mA, p, 1 - k * 0.15f);
						Ring(g, SX(q.x), SY(q.y), (e.mValue - k * 8) * gView.mScale, Color(255, 170, 240, a / (k + 1)), 2);
					}
					break;
				}
				case EV_LOB:
				{
					if (theUnder)
						break;
					float aLife = std::max(0.05f, e.mMs / 1000.0f);
					// Missiles show only as they come down; the rest arc from the thrower.
					if (e.mParam == LOOK_MISSILE)
					{
						float aFall = 0.55f;
						if (t > aLife || t < aLife - aFall)
							break;
						float f = (t - (aLife - aFall)) / aFall;
						Vec p = Lerp(e.mA, e.mB, f);
						Missile(g, LOOK_MISSILE, p, e.mB - e.mA, v.mNow, 1);
						Ring(g, SX(e.mB.x), SY(e.mB.y), e.mValue * gView.mScale, Color(255, 80, 60, 140), 1);
						break;
					}
					if (t > aLife)
						break;
					float f = t / aLife;
					Vec p = Lerp(e.mA, e.mB, f) - Vec(0, std::sin(f * 3.14159f) * std::min(220.0f, Dist(e.mA, e.mB) * 0.5f + 60));
					Missile(g, e.mParam, p, e.mB - e.mA, v.mNow, 1.3f);
					Ring(g, SX(e.mB.x), SY(e.mB.y), e.mValue * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, (int)(60 + 100 * f)), 1);
					break;
				}
				case EV_TEXT:
				{
					if (theUnder || t > 1.3f)
						break;
					static const Color kText[] = { Color(255, 90, 80), Color(120, 255, 130), Color(255, 225, 80), Color(190, 150, 255), Color(230, 240, 255), Color(255, 120, 100) };
					Color tc = kText[std::min<int>(e.mParam, 5)];
					tc.mAlpha = (int)(255 * Clamp(1.3f - t, 0, 1));
					// Damage numbers pop (bigger first), then float up.
					Font* f = e.mParam == TC_DAMAGE ? (t < 0.15f ? FONT_CONTINUUMBOLD12OUTLINE : FONT_TINYBOLD) : FONT_TINY;
					if (f == nullptr)
						f = FONT_TINYBOLD;
					Centered(g, f, e.mText, (int)SX(e.mA.x), (int)SY(e.mA.y - t * 50), tc);
					break;
				}
				case EV_PROJECTILE:
				{
					if (theUnder)
						break;
					float aLife = e.mMs / 1000.0f;
					bool aEnded = false;
					for (const Effect& o : s.mEffects)
						if (o.mEvent.mType == EV_PROJ_END && o.mEvent.mId == e.mId && o.mSeq > fx.mSeq)
							aEnded = true;
					if (aEnded || t > aLife)
						break;
					Vec p = e.mA + e.mB * t;
					Missile(g, e.mParam, p, e.mB, v.mNow, 1);
					break;
				}
				case EV_BOLT:
				{
					if (theUnder)
						break;
					float aLife = std::max(0.05f, e.mMs / 1000.0f);
					if (t > aLife)
						break;
					Vec p = Lerp(e.mA, e.mB, t / aLife);
					g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
					Sprite(g, IMAGE_ENERGYBALL, (v.mNow / 60) % 6, 0, p.x, p.y, 0.45f, false, TeamColor(e.mArena % 2), true);
					g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
					Sprite(g, IMAGE_PEARL, 0, 0, p.x, p.y, 0.35f, false);
					break;
				}
				case EV_LIGHTNING:
					if (!theUnder && t < 0.25f)
						Lightning(g, e.mA, e.mB, fx.mSeq * 7919u, Color(170, 230, 255, (int)(255 * (1 - t / 0.25f))));
					break;
				case EV_SLASH:
				{
					if (theUnder || t > 0.22f)
						break;
					int a = (int)(255 * (1 - t / 0.22f));
					Vec d = Norm(e.mA - e.mB);
					Vec n(-d.y, d.x);
					for (int k = -1; k <= 1; k++)
					{
						Vec o = n * (k * 10.0f);
						g->SetColor(Color(255, 255, 255, a));
						g->DrawLine((int)SX(e.mA.x - d.x * 24 + o.x - n.x * 20), (int)SY(e.mA.y - d.y * 24 + o.y - n.y * 20),
							(int)SX(e.mA.x + d.x * 24 + o.x + n.x * 20), (int)SY(e.mA.y + d.y * 24 + o.y + n.y * 20));
					}
					break;
				}
				case EV_BURST:
				{
					float aLife = (e.mParam == LOOK_EVOLVE || e.mParam == LOOK_BOSS) ? 1.0f : 0.5f;
					if (theUnder || t > aLife)
						break;
					if ((e.mParam == LOOK_NONE || e.mParam == LOOK_BOMB || e.mParam == LOOK_MISSILE || e.mParam == LOOK_MINE) && IMAGE_EXPLOSION != nullptr)
					{
						int aCel = std::min(9, (int)(t / 0.05f));
						Sprite(g, IMAGE_EXPLOSION, aCel, 0, e.mA.x, e.mA.y, std::max(0.5f, e.mValue / 40.0f), false);
						if (e.mParam != LOOK_NONE)
							Ring(g, SX(e.mA.x), SY(e.mA.y), e.mValue * (0.5f + t * 1.5f) * gView.mScale, Color(255, 150, 60, (int)(200 * (1 - t / aLife))), 2);
						break;
					}
					if (e.mParam == LOOK_EVOLVE)
					{
						// Evolution: rays and rising rings.
						int a = (int)(230 * (1 - t / aLife));
						for (int k = 0; k < 12; k++)
						{
							float an = k * 0.5236f + t * 2;
							g->SetColor(Color(255, 240, 160, a));
							g->DrawLine((int)SX(e.mA.x), (int)SY(e.mA.y), (int)SX(e.mA.x + std::cos(an) * e.mValue * (0.4f + t)), (int)SY(e.mA.y + std::sin(an) * e.mValue * (0.4f + t)));
						}
						for (int k = 0; k < 3; k++)
							Ring(g, SX(e.mA.x), SY(e.mA.y - t * 40), (e.mValue * (0.3f + t * 0.8f) - k * 12) * gView.mScale, Color(255, 230, 120, a), 2);
						break;
					}
					float r = e.mValue * (0.3f + t / aLife * 0.9f);
					int a = (int)(220 * (1 - t / aLife));
					Ring(g, SX(e.mA.x), SY(e.mA.y), r * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, a), 2);
					Disc(g, SX(e.mA.x), SY(e.mA.y), r * 0.9f * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, a / 5));
					if (e.mParam == LOOK_SIREN || e.mParam == LOOK_ANTHEM)
						for (int k = 0; k < 6; k++)
						{
							float an = k * 1.047f + t * 3;
							Text(g, FONT_TINYBOLD, "~", (int)SX(e.mA.x + std::cos(an) * r * 0.7f), (int)SY(e.mA.y + std::sin(an) * r * 0.7f), Color(255, 170, 240, a));
						}
					break;
				}
				case EV_BEAM:
				{
					float aLife = std::max(0.2f, e.mMs / 1000.0f);
					if (theUnder || t > aLife)
						break;
					int a = (int)(220 * std::min(1.0f, (aLife - t) / 0.2f));
					Color bc = e.mParam == 1 ? Color(230, 90, 70, a) : Color(255, 245, 170, a);
					for (int k = -1; k <= 1; k++)
					{
						g->SetColor(bc);
						g->DrawLine((int)SX(e.mA.x) + k, (int)SY(e.mA.y), (int)SX(e.mB.x) + k, (int)SY(e.mB.y));
					}
					if (e.mParam == 0)
						for (int k = 0; k < 4; k++)
						{
							float f = std::fmod(t * 1.7f + k * 0.25f, 1.0f);
							Vec p = Lerp(e.mA, e.mB, f);
							Disc(g, SX(p.x), SY(p.y), 2.5f, Color(255, 255, 220, a), 8);
						}
					break;
				}
				case EV_LEVEL_UP:
					if (!theUnder && t < 1.4f)
					{
						Ring(g, SX(e.mA.x), SY(e.mA.y), (40 + t * 60) * gView.mScale, Color(255, 230, 120, (int)(200 * (1 - t / 1.4f))), 2);
						Centered(g, FONT_JUNGLEFEVER10OUTLINE, "Level " + std::to_string((int)e.mValue) + "!", (int)SX(e.mA.x), (int)SY(e.mA.y - 70 - t * 30), Color(255, 230, 120, (int)(255 * (1 - t / 1.4f))));
					}
					break;
				case EV_CROSS: case EV_REVIVE:
					if (!theUnder && t < 0.6f)
					{
						Color rc = e.mType == EV_CROSS ? Color(210, 160, 255) : Color(255, 245, 180);
						rc.mAlpha = (int)(220 * (1 - t / 0.6f));
						Ring(g, SX(e.mA.x), SY(e.mA.y), (20 + t * 90) * gView.mScale, rc, 2);
					}
					break;
				case EV_FISH_DIED:
					if (!theUnder && t < 0.8f)
						for (int k = 0; k < 4; k++)
							Disc(g, SX(e.mA.x + (k - 1.5f) * 10), SY(e.mA.y - t * 70 - k * 8), 2.5f, Color(220, 240, 255, (int)(200 * (1 - t / 0.8f))), 8);
					break;
				case EV_BUMP:
					if (!theUnder && t < 0.45f)
						for (int k = 0; k < 7; k++)
						{
							float anAngle = k * 0.8976f + e.mId * 0.7f, r = 8 + t * 60;
							Disc(g, SX(e.mA.x + std::cos(anAngle) * r), SY(e.mA.y + std::sin(anAngle) * r * 0.7f - t * 16), (8.0f - t * 9) * gView.mScale,
								Color(236, 218, 172, (int)(235 * (1 - t / 0.45f))), 10);
						}
					break;
				case EV_WAVE:
					if (!theUnder && t < 0.8f)
						Ring(g, SX(e.mA.x), SY(e.mA.y), (60 + t * 120) * gView.mScale, Color(230, 120, 255, (int)(230 * (1 - t / 0.8f))), 3);
					break;
				case EV_MONSTER:
					if (!theUnder && t < 1.0f)
						Ring(g, SX(e.mA.x), SY(e.mA.y), (40 + t * 180) * gView.mScale, e.mValue > 0 ? Color(255, 220, 120, (int)(220 * (1 - t))) : Color(200, 120, 255, (int)(220 * (1 - t))), 3);
					break;
				default:
					break;
				}
			}
		}

		// The Trench's monster spots: a marker and a timer while the monster is away.
		static void DrawMonsterSpots(Graphics* g, const ViewState& v, const ArenaSnap& a)
		{
			static const char* kName[MON_COUNT] = { "Gus camp", "Balrog camp", "Psychosquid", "The Boss" };
			uint32_t aSquidIn = v.mSide->MonsterIn(MON_SQUID), aBossIn = v.mSide->MonsterIn(MON_BOSS);
			for (int sl = 0; sl < MON_COUNT; sl++)
			{
				Vec p = MonsterHome(sl);
				uint32_t aIn = v.mSide->MonsterIn(sl);
				// The Squid and the Boss share the pit: show whichever is there or comes first.
				if (sl == MON_SQUID && (aBossIn == 0 || (aSquidIn != 0 && aBossIn < aSquidIn)))
					continue;
				if (sl == MON_BOSS && aSquidIn == 0 && aBossIn != 0)
					continue;
				if (sl == MON_BOSS && aBossIn != 0 && aSquidIn != 0 && aSquidIn <= aBossIn)
					continue;
				if (aIn == 0)
				{
					// There: a soft camp circle under it.
					Ring(g, SX(p.x), SY(p.y), (sl >= MON_SQUID ? 120.0f : 80.0f) * gView.mScale, Color(200, 140, 255, 40), 1);
					continue;
				}
				Ring(g, SX(p.x), SY(p.y), 46 * gView.mScale, Color(200, 140, 255, 80), 1);
				Centered(g, FONT_TINY, kName[sl], (int)SX(p.x), (int)SY(p.y) - 4, Color(210, 180, 255, 170));
				Centered(g, FONT_TINYBOLD, Clock(aIn + 999), (int)SX(p.x), (int)SY(p.y) + 8, Color(255, 230, 160, 200));
			}
			(void)a;
		}

		///////////////////////////////////////////////////////////////////////
		// A whole arena at gView
		///////////////////////////////////////////////////////////////////////
		void DrawWorld(Graphics* g, const ViewState& v, int theArena, bool theFull)
		{
			const Side& s = *v.mSide;
			ArenaSnap a = s.ViewArena(theArena);
			DrawBackdrop(g, theArena, v.mNow, theFull);
			DrawKelp(g, theArena, v.mNow, false);
			DrawFloor(g, theArena);
			DrawPortals(g, theArena, s.mTeam, v.mNow, theFull);
			if (theFull)
				DrawEffects(g, v, theArena, true);
			if (IsTank(theArena))
			{
				bool aFiring[2] = { false, false };
				for (const Effect& fx : s.mEffects)
					if (fx.mEvent.mType == EV_BOLT && fx.mEvent.mArena == theArena && !Elapsed(v.mNow, fx.mAt + 300))
						for (int i = 0; i < 2; i++)
							if (Dist(fx.mEvent.mA, TankMap().mTower[i]) < 80)
								aFiring[i] = true;
				for (int i = 0; i < 2; i++)
					DrawTower(g, theArena, i, a, v.mNow, aFiring[i]);
				DrawCore(g, theArena, a, v.mNow);
			}
			else if (theFull)
				DrawMonsterSpots(g, v, a);
			for (const FoodSnap& f : a.mFood)
				Sprite(g, IMAGE_FOOD, (v.mNow / 100 + f.mId) % 10, std::min<int>(a.mFoodQuality, 2), f.mPos.x, f.mPos.y, theFull ? 0.9f : 1.6f, false);
			if (a.mCollectorLevel > 0)
				Sprite(g, IMAGE_STINKY, (v.mNow / (a.mCollectorLevel >= 2 ? 45 : 80)) % 10, 0, a.mCollectorPos.x, a.mCollectorPos.y, 0.8f, a.mCollectorRight);
			for (const FishSnap& f : a.mFish)
				DrawFish(g, f, v.mNow, !theFull);
			for (const MinionSnap& m : a.mMinions)
				DrawMinion(g, m, v.mNow, theFull);
			DrawWalls(g, theArena);
			for (const CoinSnap& c : a.mCoins)
			{
				int aRow = c.mKind == COIN_SILVER ? 0 : (c.mKind == COIN_GOLD ? 1 : 3);
				float aScale = theFull ? 0.85f : 2.1f;	// the home window: big enough to click
				if (theArena == kTrench && theFull)
				{
					// Lane coins glint so you notice them.
					float p = 0.5f + 0.5f * std::sin(v.mNow / 140.0f + c.mId);
					Disc(g, SX(c.mPos.x), SY(c.mPos.y), (14 + 4 * p) * gView.mScale, Color(255, 230, 120, 40));
				}
				Sprite(g, IMAGE_MONEY, (v.mNow / 80 + c.mId) % 10, aRow, c.mPos.x, c.mPos.y, aScale, false);
			}
			std::vector<HeroSnap> aHeroes;
			s.ViewHeroes(theArena, aHeroes);
			for (const HeroSnap& h : aHeroes)
			{
				// Flash a hero white for a moment when its health drops.
				int k = h.mPlayer % kMaxPlayers;
				if (theFull)
				{
					if (sLastHp[k] >= 0 && h.mHp < sLastHp[k] - 0.5f)
						sFlashUntil[k] = v.mNow + 110;
					sLastHp[k] = h.mHp;
				}
				DrawHero(g, h, v.mNow, h.mPlayer == s.mPlayer, v.mNames[k], theFull);
			}
			DrawKelp(g, theArena, v.mNow, true);
			if (theFull)
				DrawEffects(g, v, theArena, false);
		}

		///////////////////////////////////////////////////////////////////////
		// The main view
		///////////////////////////////////////////////////////////////////////
		// Shake: big hits on my hero and big blasts nearby.
		static float sShake = 0;
		static uint32_t sShakeAt = 0, sLastSeq = 0;
		static float sLastMyHp = -1;

		static Vec ShakeOffset(const ViewState& v)
		{
			const Side& s = *v.mSide;
			const HeroState& h = s.mHero;
			float aMax = std::max(1.0f, s.MaxHp());
			if (sLastMyHp >= 0 && h.mAlive && h.mHp < sLastMyHp - aMax * 0.06f)
			{
				sShake = std::max(sShake, std::min(7.0f, (sLastMyHp - h.mHp) / aMax * 40));
				sShakeAt = v.mNow;
			}
			sLastMyHp = h.mHp;
			for (const Effect& fx : s.mEffects)
			{
				if (fx.mSeq <= sLastSeq)
					continue;
				const Event& e = fx.mEvent;
				if (e.mArena != v.mArena)
					continue;
				bool aBig = (e.mType == EV_BURST && (e.mParam == LOOK_SLAM || e.mParam == LOOK_BOSS || e.mParam == LOOK_MISSILE || e.mParam == LOOK_EVOLVE || (e.mParam == LOOK_NONE && e.mValue >= 80)))
					|| e.mType == EV_TOWER_DOWN;
				if (aBig && Dist(e.mA, h.mPos) < 700)
				{
					sShake = std::max(sShake, e.mType == EV_TOWER_DOWN ? 6.0f : 3.5f);
					sShakeAt = v.mNow;
				}
			}
			if (!s.mEffects.empty())
				sLastSeq = s.mEffects.back().mSeq;
			float t = (v.mNow - sShakeAt) / 280.0f;
			if (t >= 1 || sShake <= 0)
				return Vec();
			float k = sShake * (1 - t);
			return Vec(std::sin(v.mNow * 0.09f) * k, std::cos(v.mNow * 0.11f) * k);
		}

		void DrawTank(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			g->SetLinearBlend(true);
			Vec aShake = ShakeOffset(v);
			gView = WorldView();
			gView.mX += aShake.x;
			gView.mY += aShake.y;
			g->SetClipRect(0, kTop, kScreenW, kHudY - kTop);
			DrawWorld(g, v, v.mArena, true);

			// Where the mouse is: hovering an enemy shows who you'd attack.
			const HeroState& h = s.mHero;
			if (v.mMouseX >= 0 && InTank(v.mMouseX, v.mMouseY) && v.mAimSlot < 0)
			{
				Vec w = ToWorld(v.mMouseX, v.mMouseY);
				ArenaSnap a = s.ViewArena(v.mArena);
				std::vector<Target> aTargets;
				s.Targets(v.mArena, aTargets, true);
				for (const Target& t : aTargets)
					if (t.mTeam != s.mTeam && Dist(t.mPos, w) < t.mRadius + 14 && (t.mRef.mKind != ENT_CORE || !(a.mTowerHp[0] > 0 || a.mTowerHp[1] > 0)))
					{
						Ring(g, SX(t.mPos.x), SY(t.mPos.y), (t.mRadius + 8) * gView.mScale, t.mMonster ? Color(200, 120, 255, 220) : Color(255, 80, 60, 200), 2);
						break;
					}
			}
			if (h.mAlive && h.mArena == v.mArena)
			{
				// My hero's auto-attack target: a small marker.
				Target t;
				if (h.mAutoTarget.Valid() && s.FindTarget(h.mAutoTarget, t))
					Ring(g, SX(t.mPos.x), SY(t.mPos.y + t.mRadius), 8 * gView.mScale + 3, Color(255, 255, 255, 90), 1);
			}
			DrawAim(g, v);
			// Low health: a red edge that pulses.
			if (h.mAlive && h.mHp < s.MaxHp() * 0.3f)
			{
				int a = 60 + (int)(50 * std::sin(v.mNow / 180.0f));
				for (int k = 0; k < 6; k++)
				{
					g->SetColor(Color(200, 20, 20, a / (k + 1)));
					g->FillRect(0, kTop + k * 3, kScreenW, 3);
					g->FillRect(0, kHudY - (k + 1) * 3, kScreenW, 3);
					g->FillRect(k * 3, kTop, 3, kHudY - kTop);
					g->FillRect(kScreenW - (k + 1) * 3, kTop, 3, kHudY - kTop);
				}
			}
			gView = WorldView();
			g->ClearClipRect();
			if (v.mHomeWindow)
				DrawHomeWindow(g, v);
			g->SetLinearBlend(false);
		}

		///////////////////////////////////////////////////////////////////////
		// Hold-to-aim indicators (D36)
		///////////////////////////////////////////////////////////////////////
		void DrawAim(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			const HeroState& h = s.mHero;
			if (v.mAimSlot < 0 || !h.mAlive || h.mArena != v.mArena || v.mMouseX < 0)
				return;
			const AbilityDef& ab = h.Def().mAb[v.mAimSlot];
			Vec w = ToWorld(v.mMouseX, v.mMouseY);
			Vec d = Norm(w - h.mPos);
			if (d.x == 0 && d.y == 0)
				d = Vec(h.mRight ? 1.0f : -1.0f, 0);
			bool aReady = s.CanCast(v.mAimSlot);
			Color c = aReady ? Color(120, 230, 255, 200) : Color(255, 120, 100, 170);
			Color f(c.mRed, c.mGreen, c.mBlue, 40);
			float hx = SX(h.mPos.x), hy = SY(h.mPos.y);
			switch (ab.mAim)
			{
			case AIM_SELF:
			{
				float r = std::max(ab.mRadius, 60.0f);
				Disc(g, hx, hy, r * gView.mScale, f, 40);
				Ring(g, hx, hy, r * gView.mScale, c, 2);
				break;
			}
			case AIM_DIR:
			{
				Ring(g, hx, hy, ab.mRange * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, 60), 1);
				float aW = std::max(ab.mRadius, 14.0f);
				auto Path = [&](Vec theDir, float theLen) {
					Vec n(-theDir.y, theDir.x);
					Vec e = h.mPos + theDir * theLen;
					Point q[4] = { Point((int)SX(h.mPos.x + n.x * aW), (int)SY(h.mPos.y + n.y * aW)), Point((int)SX(e.x + n.x * aW), (int)SY(e.y + n.y * aW)),
						Point((int)SX(e.x - n.x * aW), (int)SY(e.y - n.y * aW)), Point((int)SX(h.mPos.x - n.x * aW), (int)SY(h.mPos.y - n.y * aW)) };
					g->SetColor(f);
					g->PolyFill(q, 4, true);
					g->SetColor(c);
					g->DrawLine(q[0].mX, q[0].mY, q[1].mX, q[1].mY);
					g->DrawLine(q[3].mX, q[3].mY, q[2].mX, q[2].mY);
					float aHead = std::min(aW * 1.4f, 30.0f);
					Point ah[3] = { Point((int)SX(e.x + theDir.x * aHead), (int)SY(e.y + theDir.y * aHead)), Point((int)SX(e.x + n.x * aHead), (int)SY(e.y + n.y * aHead)), Point((int)SX(e.x - n.x * aHead), (int)SY(e.y - n.y * aHead)) };
					g->PolyFill(ah, 3, true);
				};
				if (h.Look() == HERO_PRESTO && v.mAimSlot == AB_Q)
				{
					// Card Trick: the fan.
					int n = s.HasTalent(0, 0) ? 5 : 3;
					float b = std::atan2(d.y, d.x);
					for (int i = 0; i < n; i++)
					{
						float an = b + (i - (n - 1) * 0.5f) * 0.21f;
						Path(Vec(std::cos(an), std::sin(an)), ab.mRange);
					}
				}
				else
				{
					float aLen = ab.mRange;
					if (h.Look() == HERO_ITCHY && v.mAimSlot == AB_Q && s.HasTalent(0, 1))
						aLen *= 1.6f;
					if (h.Look() == HERO_CLYDE && v.mAimSlot == AB_Q && s.HasTalent(0, 1))
						aLen *= 1.4f;
					if (h.Look() == HERO_SHRAPNEL && v.mAimSlot == AB_E && s.HasTalent(2, 1))
						aLen *= 2;
					Path(d, aLen);
				}
				break;
			}
			case AIM_POINT:
			{
				Ring(g, hx, hy, ab.mRange * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, 70), 1);
				Vec aOff = w - h.mPos;
				if (Len(aOff) > ab.mRange)
					aOff = Norm(aOff) * ab.mRange;
				Vec p = h.mPos + aOff;
				float r = std::max(ab.mRadius, 24.0f);
				if (h.Look() == HERO_RHUBARB && v.mAimSlot == AB_Q)
					p = WalkerPos(h.mArena, Clamp(w.x, h.mPos.x - ab.mRange, h.mPos.x + ab.mRange), h.Def().mRadius);
				if (h.Look() == HERO_NIKO && v.mAimSlot == AB_Q)
				{
					p = WalkerPos(h.mArena, p.x, 22);
					Ring(g, SX(p.x), SY(p.y), ab.mRadius * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, 90), 1);
					r = 26;
				}
				if (h.Look() == HERO_CLYDE && v.mAimSlot == AB_E)
					r = 30;
				Disc(g, SX(p.x), SY(p.y), r * gView.mScale, f, 32);
				Ring(g, SX(p.x), SY(p.y), r * gView.mScale, c, 2);
				g->SetColor(Color(c.mRed, c.mGreen, c.mBlue, 90));
				g->DrawLine((int)hx, (int)hy, (int)SX(p.x), (int)SY(p.y));
				break;
			}
			case AIM_CONE:
			{
				float half = ab.mRadius * (s.HasTalent(0, 1) ? 2.0f : 1.0f) * 3.14159f / 180, b = std::atan2(d.y, d.x);
				std::vector<Point> p;
				p.push_back(Point((int)hx, (int)hy));
				for (int k = 0; k <= 10; k++)
				{
					float an = b - half + 2 * half * k / 10;
					p.push_back(Point((int)SX(h.mPos.x + std::cos(an) * ab.mRange), (int)SY(h.mPos.y + std::sin(an) * ab.mRange)));
				}
				g->SetColor(f);
				g->PolyFill(p.data(), (int)p.size(), true);
				g->SetColor(c);
				for (size_t k = 1; k < p.size(); k++)
					g->DrawLine(p[k - 1].mX, p[k - 1].mY, p[k].mX, p[k].mY);
				g->DrawLine(p.back().mX, p.back().mY, p[0].mX, p[0].mY);
				break;
			}
			case AIM_FRIEND:
			{
				Ring(g, hx, hy, ab.mRange * gView.mScale, Color(c.mRed, c.mGreen, c.mBlue, 70), 1);
				// What it would land on: a tower or the core near the mouse (at home), else Angie.
				Vec p = h.mPos;
				if (h.mArena == s.mTeam)
				{
					const MapDef& m = TankMap();
					float aBest = 130;
					for (int i = 0; i < 2; i++)
						if (s.mArena.mTower[i].mAlive && Dist(m.mTower[i], w) < aBest && Dist(m.mTower[i], h.mPos) < ab.mRange + kTowerR)
						{
							aBest = Dist(m.mTower[i], w);
							p = m.mTower[i];
						}
					if (Dist(m.mCore, w) < aBest && Dist(m.mCore, h.mPos) < ab.mRange + kCoreR)
						p = m.mCore;
				}
				Ring(g, SX(p.x), SY(p.y), 60 * gView.mScale, Color(255, 245, 170, 200), 2);
				break;
			}
			}
		}

		///////////////////////////////////////////////////////////////////////
		// The home window and the world map
		///////////////////////////////////////////////////////////////////////
		void DrawHomeWindow(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			bool aAlert = !v.mAlert.empty() && !Elapsed(v.mNow, v.mAlertAt + kAlertShowMs);
			bool aFlash = aAlert && (v.mNow / 250) % 2 == 0;
			g->SetColor(aFlash ? Color(200, 40, 40, 255) : Color(255, 205, 60, v.mHomeFaded ? 120 : 230));
			g->FillRect(kHomeX - 2, kHomeY - 2, kHomeW + 4, kHomeH + 4);
			gView.mScale = (float)kHomeW / kWorldW;
			gView.mX = (float)kHomeX;
			gView.mY = (float)kHomeY;
			g->SetClipRect(kHomeX, kHomeY, kHomeW, kHomeH);
			DrawWorld(g, v, s.mTeam, false);
			if (v.mHomeFaded)
			{
				g->SetColor(Color(0, 0, 0, 120));
				g->FillRect(kHomeX, kHomeY, kHomeW, kHomeH);
			}
			g->ClearClipRect();
			gView = WorldView();
			g->SetColor(Color(0, 0, 0, 150));
			g->FillRect(kHomeX, kHomeY + kHomeH - 12, 64, 12);
			Text(g, FONT_TINYBOLD, "HOME  (click)", kHomeX + 3, kHomeY + kHomeH - 3, Color(255, 225, 120));
			char b[32];
			snprintf(b, sizeof(b), "$%d", s.mArena.mMoney);
			g->SetColor(Color(0, 0, 0, 150));
			g->FillRect(kHomeX + kHomeW - 44, kHomeY + kHomeH - 12, 44, 12);
			Text(g, FONT_TINYBOLD, b, kHomeX + kHomeW - 41, kHomeY + kHomeH - 3, Color(255, 225, 90));
			if (aAlert)
				Centered(g, FONT_TINYBOLD, v.mAlert, kHomeX + kHomeW / 2, kHomeY + 10, Color(255, 230, 220));
		}

		void DrawWorldMap(Graphics* g, const ViewState& v, int x, int y, int w, int h)
		{
			// The three tanks side by side: team 0's, the Trench, team 1's (the gates line up).
			const Side& s = *v.mSide;
			bool aAlert = !v.mAlert.empty() && !Elapsed(v.mNow, v.mAlertAt + kAlertShowMs);
			int pw = (w - 4) / 3;
			for (int k = 0; k < 3; k++)
			{
				int aArena = k == 0 ? 0 : (k == 1 ? kTrench : 1);
				Rect r(x + k * (pw + 2), y, pw, h);
				bool aFlash = aAlert && aArena == s.mTeam && (v.mNow / 250) % 2 == 0;
				bool aHere = aArena == v.mArena;
				g->SetColor(aFlash ? Color(120, 20, 20, 235) : (aArena == kTrench ? Color(30, 14, 60, 235) : Color(10, 30, 60, 230)));
				g->FillRect(r);
				g->SetColor(aHere ? Color(255, 255, 255, 220) : (aArena == kTrench ? Color(170, 120, 230, 160) : TeamColor(aArena)));
				g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
				ArenaSnap a = s.ViewArena(aArena);
				auto P = [&](Vec wv) { return Point(r.mX + (int)(wv.x / kWorldW * r.mWidth), r.mY + (int)(wv.y / kWorldH * r.mHeight)); };
				for (const MinionSnap& m : a.mMinions)
				{
					Point p = P(m.mPos);
					if (m.mTeam == kNeutralTeam)
						Disc(g, (float)p.mX, (float)p.mY, m.mKind >= MIN_PSYCHO ? 2.5f : 1.8f, Color(200, 120, 255), 6);
					else
					{
						const Color& tc = TeamColor(m.mTeam);
						g->SetColor(tc);
						g->FillRect(p.mX, p.mY, 1, 1);
					}
				}
				if (IsTank(aArena))
					for (int i = 0; i < 2; i++)
					{
						Point p = P(TankMap().mTower[i]);
						g->SetColor(a.mTowerHp[i] > 0 ? TeamColor(aArena) : Color(80, 80, 80));
						g->FillRect(p.mX - 1, p.mY - 1, 3, 3);
					}
				std::vector<HeroSnap> aHeroes;
				s.ViewHeroes(aArena, aHeroes);
				for (const HeroSnap& hh : aHeroes)
					if (hh.mFlags & HF_ALIVE)
					{
						Point p = P(hh.mPos);
						Disc(g, (float)p.mX, (float)p.mY, hh.mPlayer == s.mPlayer ? 2.8f : 2.2f, TeamColor(hh.mTeam), 8);
					}
			}
		}
	}
}
