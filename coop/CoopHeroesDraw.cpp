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
		static const Color kTeamColor[2] = { Color(255, 205, 60), Color(80, 220, 255) };
		static bool Elapsed(uint32_t theNow, uint32_t theAt) { return (int32_t)(theNow - theAt) >= 0; }

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

		static void Sprite(Graphics* g, Image* theImage, int theCol, int theRow, float wx, float wy, float theScale, bool theMirror,
			const Color& theTint = Color(255, 255, 255), bool theColorize = false)
		{
			if (theImage == nullptr)
				return;
			Rect aSrc = Cel(theImage, theCol, theRow);
			Transform t;
			t.Scale(theMirror ? -theScale * kScale : theScale * kScale, theScale * kScale);
			if (theColorize || theTint.mAlpha < 255)
			{
				g->SetColorizeImages(true);
				g->SetColor(theTint);
			}
			g->DrawImageTransformF(theImage, t, aSrc, SX(wx), SY(wy));
			g->SetColorizeImages(false);
		}

		// The same in screen coordinates (HUD, draft).
		static void ScreenSprite(Graphics* g, Image* theImage, int theCol, int theRow, float sx, float sy, float theScale, bool theMirror,
			const Color& theTint = Color(255, 255, 255), bool theColorize = false)
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

		static void Disc(Graphics* g, float cx, float cy, float r, const Color& c, int theSides = 20)
		{
			if (r < 0.5f)
				return;
			std::vector<Point> p;
			for (int k = 0; k < theSides; k++)
				p.push_back(Point((int)std::lround(cx + r * std::cos(k * 6.2832f / theSides)), (int)std::lround(cy + r * std::sin(k * 6.2832f / theSides))));
			g->SetColor(c);
			g->PolyFill(p.data(), (int)p.size(), true);
		}

		static void Ring(Graphics* g, float cx, float cy, float r, const Color& c, int theThick = 1)
		{
			g->SetColor(c);
			for (int t = 0; t < theThick; t++)
			{
				float rr = r + t;
				for (int k = 0; k < 32; k++)
				{
					float a0 = k * 6.2832f / 32, a1 = (k + 1) * 6.2832f / 32;
					g->DrawLine((int)(cx + rr * std::cos(a0)), (int)(cy + rr * std::sin(a0)), (int)(cx + rr * std::cos(a1)), (int)(cy + rr * std::sin(a1)));
				}
			}
		}

		static void Bar(Graphics* g, int x, int y, int w, int h, float theFrac, const Color& theFill, const Color& theBack = Color(20, 20, 30, 200))
		{
			g->SetColor(theBack);
			g->FillRect(x - 1, y - 1, w + 2, h + 2);
			int f = (int)std::lround(w * Clamp(theFrac, 0, 1));
			g->SetColor(theFill);
			g->FillRect(x, y, f, h);
		}

		static void Text(Graphics* g, Font* f, const std::string& s, int x, int y, const Color& c)
		{
			if (f == nullptr)
				return;
			g->SetFont(f);
			g->SetColor(c);
			g->DrawString(s, x, y);
		}

		static void Centered(Graphics* g, Font* f, const std::string& s, int cx, int y, const Color& c)
		{
			if (f == nullptr)
				return;
			Text(g, f, s, cx - f->StringWidth(s) / 2, y, c);
		}

		static int Wrapped(Graphics* g, Font* f, const std::string& s, int x, int y, int w, const Color& c)
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

		static std::string Clock(uint32_t theMs)
		{
			char b[16];
			snprintf(b, sizeof(b), "%u:%02u", theMs / 60000, (theMs / 1000) % 60);
			return b;
		}

		const char* HeroName(int theHero) { return HeroDefOf(theHero).mName; }

		// Speedy (D30): Stinky's art in a neon, purple-heavy palette, made once per image.
		// Browns turn electric purple, the yellow-green body hot magenta; whites, greys and
		// the dark outline stay (a little purple), so he still reads as a snail.
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
						// Greys: a touch of violet.
						hue = 275;
						sat = std::min(1.0f, sat + 0.18f);
					}
					else
					{
						// Browns and oranges (0-50) to purple, yellows and greens (50-150) to magenta;
						// the shell's dark spiral lines to electric cyan.
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
			default: return Neon(IMAGE_STINKY);
			}
		}

		static Image* HeroPortrait(int theHero)
		{
			switch (theHero)
			{
			case HERO_ITCHY: return IMAGE_SCL_ITCHY;
			case HERO_CLYDE: return IMAGE_SCL_CLYDE;
			case HERO_RHUBARB: return IMAGE_SCL_RHUBARB;
			case HERO_ANGIE: return IMAGE_SCL_ANGIE;
			default: return Neon(IMAGE_SCL_STINKY);
			}
		}

		static Image* Backdrop(int theTeam) { return theTeam == 0 ? IMAGE_AQUARIUM1 : IMAGE_AQUARIUM4; }

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
			default: break;
			}
			if (anId >= 0)
				anApp->PlaySample(anId);
		}

		///////////////////////////////////////////////////////////////////////
		// The tank's scenery
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

		static void DrawBackdrop(Graphics* g, int theTeam, uint32_t theNow)
		{
			Image* b = Backdrop(theTeam);
			if (b != nullptr)
				g->DrawImage(b, Rect(0, kTop, kScreenW, kHudY - kTop), Rect(0, 0, b->mWidth, b->mHeight));
			// Deep water: the painting becomes distant scenery behind the play.
			g->SetColor(Color(0, 18, 44, 160));
			g->FillRect(0, kTop, kScreenW, kHudY - kTop);
			// Light rays.
			for (int i = 0; i < 5; i++)
			{
				float x = 90.0f + i * 125 + 20 * std::sin(theNow / 3000.0f + i);
				Point p[4] = { Point((int)x, kTop), Point((int)x + 34, kTop), Point((int)x + 90, kHudY - 40), Point((int)x + 20, kHudY - 40) };
				g->SetColor(Color(200, 230, 255, 14));
				g->PolyFill(p, 4, true);
			}
		}

		static void DrawFloor(Graphics* g, int theTeam)
		{
			const std::vector<Vec>& f = TheMap().mFloor;
			// The painting's own sand, tiled along the floor in 256-unit columns.
			MemoryImage* b = Cut(Backdrop(theTeam), 100, 404, 256, 64);
			if (b == nullptr)
				return;
			const float kTile = 256;
			std::vector<TriVertex> v;
			for (float x = 0; x < kWorldW; x += 32)
			{
				float x1 = std::min(kWorldW, x + 32);
				float u0 = std::fmod(x, kTile) / kTile, u1 = u0 + (x1 - x) / kTile;
				float y0 = FloorY(x), y1 = FloorY(x1);
				TriVertex t0(SX(x), SY(y0), u0, 0, 0xFFFFFFFF), t1(SX(x1), SY(y1), u1, 0, 0xFFFFFFFF);
				TriVertex b0(SX(x), SY(kWorldH), u0, 1, 0xFF8A8A8A), b1(SX(x1), SY(kWorldH), u1, 1, 0xFF8A8A8A);
				v.push_back(t0); v.push_back(t1); v.push_back(b1);
				v.push_back(t0); v.push_back(b1); v.push_back(b0);
			}
			g->DrawTrianglesTex(b, (const TriVertex(*)[3])v.data(), (int)v.size() / 3);
			// The rim of the floor.
			g->SetColor(Color(255, 240, 200, 90));
			for (size_t i = 1; i < f.size(); i++)
				g->DrawLine((int)SX(f[i - 1].x), (int)SY(f[i - 1].y), (int)SX(f[i].x), (int)SY(f[i].y));
		}

		// Walls are solid slate blocks (D35): a bevel (edges facing up are lit, edges facing
		// down are shaded), a flat face, and a thick dark outline, so they read as things
		// you bump into rather than part of the painted scenery.
		static void DrawWalls(Graphics* g)
		{
			const Color kLit(228, 238, 250), kSide(168, 182, 202), kShade(72, 84, 106), kFace(150, 166, 188), kEdge(12, 15, 22);
			const float kBevel = 11, kOutline = 3.2f;
			for (const WallDef& w : TheMap().mWalls)
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
					float aUp = -anOut.y / std::max(0.01f, Len(anOut));		// 1: faces straight up
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
					Disc(g, SX(a.x), SY(a.y), SX(kOutline), kEdge, 8);
				}
			}
		}

		// Swaying kelp; theFront draws a see-through layer over whatever hides inside.
		static void DrawKelp(Graphics* g, uint32_t theNow, bool theFront)
		{
			const std::vector<KelpDef>& aKelp = TheMap().mKelp;
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
					float aBottom = std::min(p.mY1, FloorY(x)) + 4;
					int aSegs = 10;
					Color c = theFront ? Color(40, 160, 70, 150) : Color(30 + (s * 20) % 40, 120 + (s * 17) % 50, 50, 235);
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
							Disc(g, SX(x + sw1 + (i % 4 == 1 ? 10.0f : -10.0f)), SY(y1), 3.5f, Color(60, 190, 90, 220), 8);
					}
				}
			}
		}

		static void DrawPortal(Graphics* g, uint32_t theNow)
		{
			const MapDef& m = TheMap();
			float r = m.mPortalR;
			Disc(g, SX(m.mPortal.x), SY(m.mPortal.y), SX(r * 1.25f), Color(120, 60, 200, 60));
			if (IMAGE_WARPHOLE != nullptr)
				Sprite(g, IMAGE_WARPHOLE, (theNow / 70) % 17, 0, m.mPortal.x, m.mPortal.y, 0.62f, false);
			Ring(g, SX(m.mPortal.x), SY(m.mPortal.y), SX(r), Color(210, 160, 255, 90 + (int)(60 * std::sin(theNow / 300.0f))), 2);
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
				Ring(g, SX(p.x), SY(p.y - 4), SX(m.mPadR), Color(200, 150, 255, 150));
			}
			float x = m.mPortal.x;
			Point q[4] = { Point((int)SX(x - 16), (int)SY(m.mPortal.y + r)), Point((int)SX(x + 16), (int)SY(m.mPortal.y + r)),
				Point((int)SX(x + 34), (int)SY(FloorY(x))), Point((int)SX(x - 34), (int)SY(FloorY(x))) };
			g->SetColor(Color(190, 140, 255, 16));
			g->PolyFill(q, 4, true);
		}

		///////////////////////////////////////////////////////////////////////
		// Structures
		///////////////////////////////////////////////////////////////////////
		static void DrawTower(Graphics* g, int theArena, int theIndex, const ArenaSnap& s, uint32_t theNow, bool theFiring)
		{
			const MapDef& m = TheMap();
			Vec p = m.mTower[theIndex];
			float aBase = FloorY(p.x);
			bool aAlive = s.mTowerHp[theIndex] > 0;
			// The pedestal: a stone column.
			std::vector<Vec> aCol = { Vec(p.x - 34, aBase + 4), Vec(p.x - 26, p.y - 20), Vec(p.x + 26, p.y - 20), Vec(p.x + 34, aBase + 4) };
			if (!aAlive)
				aCol = { Vec(p.x - 40, aBase + 4), Vec(p.x - 30, aBase - 30), Vec(p.x - 6, aBase - 44), Vec(p.x + 20, aBase - 26), Vec(p.x + 40, aBase + 4) };
			TexturedPoly(g, StoneTex(), aCol, 0, 0, 1, 1, 0xFF707080);
			if (!aAlive)
				return;
			Color c = kTeamColor[theArena];
			// A glow in the owner's color, then Niko's clam on top: it opens to fire pearls.
			Disc(g, SX(p.x), SY(p.y - 34), SX(44), Color(c.mRed, c.mGreen, c.mBlue, 50));
			int aRow = theFiring ? 1 : 0;
			int aCol2 = theFiring ? 9 : (int)((theNow / 150 + theIndex * 3) % 10);
			bool aBlind = (s.mTowerBlind >> theIndex) & 1;
			Sprite(g, IMAGE_NIKO, aCol2, aRow, p.x, p.y - 44, 1.45f, false, aBlind ? Color(140, 200, 120) : Color(255, 255, 255), aBlind);
			if (s.mTowerShield[theIndex] > 0)
				Ring(g, SX(p.x), SY(p.y - 30), SX(70), Color(255, 245, 170, 170), 2);
			// Health.
			float aFrac = s.mTowerHp[theIndex] / std::max(1.0f, s.mTowerMax);
			Bar(g, (int)SX(p.x - 44), (int)SY(p.y - 104), (int)SX(88), 3, aFrac, aFrac > 0.5f ? Color(90, 230, 110) : (aFrac > 0.25f ? Color(240, 210, 60) : Color(240, 80, 60)));
		}

		static void DrawCore(Graphics* g, int theArena, const ArenaSnap& s, uint32_t theNow)
		{
			const MapDef& m = TheMap();
			Vec p = m.mCore;
			Color c = kTeamColor[theArena];
			bool aOpen = s.mTowerHp[0] <= 0 && s.mTowerHp[1] <= 0;
			Disc(g, SX(p.x), SY(p.y + 10), SX(96), Color(c.mRed, c.mGreen, c.mBlue, 44));
			if (s.mCoreHp > 0)
				Sprite(g, IMAGE_MONEY, (theNow / 90) % 10, 4, p.x, p.y - 6, 2.5f, false);
			else
				Sprite(g, IMAGE_MONEY, 0, 4, p.x, p.y, 2.2f, false, Color(90, 80, 80), true);
			if (!aOpen && s.mCoreHp > 0)
			{
				// Protected while a tower stands: a faint bubble.
				Ring(g, SX(p.x), SY(p.y - 10), SX(96), Color(200, 230, 255, 70 + (int)(30 * std::sin(theNow / 400.0f))), 1);
				Disc(g, SX(p.x), SY(p.y - 10), SX(92), Color(200, 230, 255, 18));
			}
			if (s.mCoreShield > 0)
				Ring(g, SX(p.x), SY(p.y - 10), SX(104), Color(255, 245, 170, 180), 2);
			float aFrac = s.mCoreHp / kCoreHealth;
			Bar(g, (int)SX(p.x - 80), (int)SY(p.y - 100), (int)SX(160), 4, aFrac, aOpen ? Color(240, 90, 60) : Color(255, 210, 80));
		}

		///////////////////////////////////////////////////////////////////////
		// Creatures
		///////////////////////////////////////////////////////////////////////
		static void DrawFish(Graphics* g, const FishSnap& f, uint32_t theNow)
		{
			int aFrame = (int)((theNow / 90 + f.mId * 3) % 10);
			bool aMirror = (f.mFlags & FF_RIGHT) != 0;
			bool aHungry = (f.mFlags & FF_HUNGRY) != 0;
			if (f.mFlags & FF_DYING)
			{
				// The death sheet has guppies (by size) and the carnivore; a breeder dies as
				// itself, greyed (as in Insaniquarium).
				if (f.mKind == FISH_BREEDER)
				{
					Sprite(g, IMAGE_HUNGRYBREEDER, aFrame, 3, f.mPos.x, f.mPos.y, 1.0f, aMirror, Color(170, 170, 170, 160), true);
					return;
				}
				Sprite(g, IMAGE_SMALLDIE, aFrame, f.mKind == FISH_CARNIVORE ? 4 : std::min<int>(f.mSize, 2), f.mPos.x, f.mPos.y, 1.0f, aMirror, Color(255, 255, 255, 160));
				return;
			}
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

		static Image* MinionImage(int theKind)
		{
			switch (theKind)
			{
			case MIN_MINI: return IMAGE_MINISYLV;
			case MIN_SYLV: return IMAGE_SYLV;
			case MIN_GUS: return IMAGE_GUS;
			case MIN_BALROG: return IMAGE_BALROG;
			default: return IMAGE_DESTRUCTOR;
			}
		}

		static void DrawMinion(Graphics* g, const MinionSnap& m, uint32_t theNow)
		{
			Image* anImg = MinionImage(m.mKind);
			int aFrame = (int)((theNow / 70 + m.mId) % 10);
			float aScale = m.mKind == MIN_MINI ? 1.0f : 0.8f;
			bool aMirror = (m.mFlags & MF_RIGHT) != 0;
			int aRow = (m.mKind == MIN_GUS && (m.mFlags & MF_ATTACKING)) ? 2 : 0;
			Color aTint = (m.mFlags & MF_CHARMED) ? Color(255, 150, 220) : Color(255, 255, 255);
			// Whose side it's on: a faint ring in that player's color.
			float rr = MinionDefOf(m.mKind).mRadius * 0.8f;
			Color tc = kTeamColor[m.mTeam % 2];
			Ring(g, SX(m.mPos.x), SY(m.mPos.y), SX(rr), Color(tc.mRed, tc.mGreen, tc.mBlue, 110), 1);
			Sprite(g, anImg, aFrame, aRow, m.mPos.x, m.mPos.y, aScale, aMirror, aTint, (m.mFlags & MF_CHARMED) != 0);
			float r = MinionDefOf(m.mKind).mRadius;
			if (m.mHpFrac < 0.999f)
				Bar(g, (int)SX(m.mPos.x - r), (int)SY(m.mPos.y - r - 16), (int)SX(r * 2), 2, m.mHpFrac, Color(240, 90, 70));
			if (m.mFlags & MF_STUNNED)
				Sprite(g, IMAGE_ZZZ, 0, 0, m.mPos.x + r * 0.5f, m.mPos.y - r - 20, 1.0f, false);
		}

		static void DrawHero(Graphics* g, const HeroSnap& h, uint32_t theNow, bool theMine, const std::string& theName)
		{
			const HeroDef& d = HeroDefOf(h.mHero);
			Color c = kTeamColor[h.mTeam % 2];
			if (!(h.mFlags & HF_ALIVE))
			{
				Sprite(g, HeroImage(h.mHero), 0, 0, h.mPos.x, h.mPos.y, 1.2f, false, Color(120, 120, 140, 110), true);
				return;
			}
			int aAlpha = (h.mFlags & HF_HIDDEN) ? 110 : ((h.mFlags & HF_UNTARGETABLE) ? 90 : 255);
			bool aRight = (h.mFlags & HF_RIGHT) != 0;
			// A ring in the owner's color.
			float r = d.mRadius;
			Ring(g, SX(h.mPos.x), SY(h.mPos.y + (d.mWalker ? r * 0.6f : 0)), SX(r + 6), Color(c.mRed, c.mGreen, c.mBlue, aAlpha * 7 / 10), 2);
			Image* anImg = HeroImage(h.mHero);
			int aFrame = (int)((theNow / 75) % 10);
			int aRow = 0;
			bool aMirror = aRight;
			switch (h.mHero)
			{
			case HERO_ITCHY: aRow = (h.mFlags & (HF_ATTACKING | HF_STORM)) ? 2 : 0; break;
			case HERO_RHUBARB: aRow = (h.mFlags & HF_ATTACKING) ? 1 : 0; aMirror = false; break;
			case HERO_SPEEDY: aRow = (h.mFlags & HF_IMMUNE) ? 2 : 0; if (h.mFlags & HF_IMMUNE) aFrame = 9; break;
			default: break;
			}
			float aScale = 1.35f;
			if (h.mFlags & HF_STORM)
			{
				// Swordstorm: spin.
				Transform t;
				t.RotateRad(theNow / 60.0f);
				t.Scale(aScale * kScale, aScale * kScale);
				g->DrawImageTransformF(anImg, t, anImg->GetCelRect(aFrame, 0), SX(h.mPos.x), SY(h.mPos.y));
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), SX(110), Color(220, 240, 255, 120), 1);
			}
			else
			{
				if (h.mHero == HERO_SPEEDY)
				{
					// Speedy's neon glow.
					g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
					Sprite(g, anImg, aFrame, aRow, h.mPos.x, h.mPos.y, aScale * 1.12f, aMirror, Color(170, 60, 255, aAlpha * 65 / 100), true);
					g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
				}
				Sprite(g, anImg, aFrame, aRow, h.mPos.x, h.mPos.y, aScale, aMirror, Color(255, 255, 255, aAlpha));
			}
			if (h.mFlags & HF_IMMUNE)
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), SX(r + 12), Color(255, 240, 150, 200), 2);
			if (h.mShield > 0)
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), SX(r + 16), Color(255, 250, 200, 160), 1);
			if (h.mFlags & HF_STUNNED)
				Sprite(g, IMAGE_ZZZ, 0, 0, h.mPos.x + 10, h.mPos.y - r - 30, 1.2f, false);
			if (h.mFlags & HF_GOLDRUSH)
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), SX(r + 20 + 6 * std::sin(theNow / 90.0f)), Color(255, 220, 60, 160), 2);
			// Name, level and health.
			int bx = (int)SX(h.mPos.x - 34), by = (int)SY(h.mPos.y - r - 30);
			Bar(g, bx, by, (int)SX(68), 3, h.mHp / std::max(1.0f, h.mMaxHp), theMine ? Color(110, 240, 110) : Color(250, 90, 80));
			if (h.mShield > 0)
				Bar(g, bx, by - 3, (int)(SX(68) * std::min(1.0f, h.mShield / std::max(1.0f, h.mMaxHp))), 1, 1, Color(255, 250, 200), Color(0, 0, 0, 0));
			Disc(g, bx - 5.0f, by + 1.5f, 5, Color(20, 20, 40, 220), 12);
			Centered(g, FONT_TINYBOLD, std::to_string(h.mLevel), bx - 5, by + 5, Color(255, 240, 180));
			Centered(g, FONT_TINY, theName, (int)SX(h.mPos.x), by - 3, Color(c.mRed, c.mGreen, c.mBlue, aAlpha));
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
			case LOOK_DESTRUCTOR: return Color(255, 120, 60);
			case LOOK_CHARM: return Color(255, 140, 220);
			case LOOK_SLAM: case LOOK_LEAP: return Color(230, 200, 150);
			case LOOK_STORM: case LOOK_LUNGE: return Color(220, 250, 255);
			case LOOK_TELEPORT: return Color(200, 160, 255);
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

		static void DrawEffects(Graphics* g, const ViewState& v, bool theUnder)
		{
			const Side& s = *v.mSide;
			for (const Effect& fx : s.mEffects)
			{
				const Event& e = fx.mEvent;
				if (e.mArena != v.mArena)
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
					if (e.mParam == LOOK_SLIME)
					{
						Disc(g, SX(e.mA.x), SY(e.mA.y), SX(e.mValue), Color(110, 210, 60, a / 2), 14);
						break;
					}
					if (e.mParam == LOOK_STINK)
					{
						for (int k = 0; k < 7; k++)
						{
							float ang = k * 0.9f + v.mNow / 900.0f;
							Disc(g, SX(e.mA.x + std::cos(ang) * e.mValue * 0.5f), SY(e.mA.y + std::sin(ang) * e.mValue * 0.35f), SX(e.mValue * 0.45f), Color(130, 190, 60, a / 3), 14);
						}
						break;
					}
					Disc(g, SX(e.mA.x), SY(e.mA.y), SX(e.mValue), Color(c.mRed, c.mGreen, c.mBlue, a / 4));
					Ring(g, SX(e.mA.x), SY(e.mA.y), SX(e.mValue), Color(c.mRed, c.mGreen, c.mBlue, a), 1);
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
				case EV_TEXT:
				{
					if (theUnder || t > 1.3f)
						break;
					static const Color kText[] = { Color(255, 90, 80), Color(120, 255, 130), Color(255, 225, 80), Color(190, 150, 255), Color(230, 240, 255), Color(255, 120, 100) };
					Color tc = kText[std::min<int>(e.mParam, 5)];
					tc.mAlpha = (int)(255 * Clamp(1.3f - t, 0, 1));
					Centered(g, e.mParam == TC_DAMAGE ? FONT_TINYBOLD : FONT_TINY, e.mText, (int)SX(e.mA.x), (int)SY(e.mA.y - t * 50), tc);
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
					float aScale = e.mParam == LOOK_ZAP ? 0.7f : 0.42f;
					g->SetDrawMode(Graphics::DRAWMODE_ADDITIVE);
					Sprite(g, IMAGE_ENERGYBALL, (v.mNow / 60) % 6, 0, p.x, p.y, aScale, false, c, true);
					g->SetDrawMode(Graphics::DRAWMODE_NORMAL);
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
					Sprite(g, IMAGE_ENERGYBALL, (v.mNow / 60) % 6, 0, p.x, p.y, 0.45f, false, kTeamColor[e.mArena % 2], true);
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
					if (theUnder || t > 0.5f)
						break;
					if (e.mParam == LOOK_NONE && IMAGE_EXPLOSION != nullptr)
					{
						int aCel = std::min(9, (int)(t / 0.05f));
						Sprite(g, IMAGE_EXPLOSION, aCel, 0, e.mA.x, e.mA.y, std::max(0.5f, e.mValue / 40.0f), false);
						break;
					}
					float r = e.mValue * (0.3f + t / 0.5f * 0.9f);
					int a = (int)(220 * (1 - t / 0.5f));
					Ring(g, SX(e.mA.x), SY(e.mA.y), SX(r), Color(c.mRed, c.mGreen, c.mBlue, a), 2);
					Disc(g, SX(e.mA.x), SY(e.mA.y), SX(r * 0.9f), Color(c.mRed, c.mGreen, c.mBlue, a / 5));
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
						Ring(g, SX(e.mA.x), SY(e.mA.y), SX(40 + t * 60), Color(255, 230, 120, (int)(200 * (1 - t / 1.4f))), 2);
						Centered(g, FONT_JUNGLEFEVER10OUTLINE, "Level " + std::to_string((int)e.mValue) + "!", (int)SX(e.mA.x), (int)SY(e.mA.y - 70 - t * 30), Color(255, 230, 120, (int)(255 * (1 - t / 1.4f))));
					}
					break;
				case EV_CROSS: case EV_REVIVE:
					if (!theUnder && t < 0.6f)
					{
						Color rc = e.mType == EV_CROSS ? Color(210, 160, 255) : Color(255, 245, 180);
						rc.mAlpha = (int)(220 * (1 - t / 0.6f));
						Ring(g, SX(e.mA.x), SY(e.mA.y), SX(20 + t * 90), rc, 2);
					}
					break;
				case EV_FISH_DIED:
					if (!theUnder && t < 0.8f)
						for (int k = 0; k < 4; k++)
							Disc(g, SX(e.mA.x + (k - 1.5f) * 10), SY(e.mA.y - t * 70 - k * 8), 2.5f, Color(220, 240, 255, (int)(200 * (1 - t / 0.8f))), 8);
					break;
				case EV_BUMP:		// a little sand puff where my hero ran into a wall
					if (!theUnder && t < 0.45f)
						for (int k = 0; k < 7; k++)
						{
							float anAngle = k * 0.8976f + e.mId * 0.7f, r = 8 + t * 60;
							Disc(g, SX(e.mA.x + std::cos(anAngle) * r), SY(e.mA.y + std::sin(anAngle) * r * 0.7f - t * 16), SX(8.0f - t * 9),
								Color(236, 218, 172, (int)(235 * (1 - t / 0.45f))), 10);
						}
					break;
				case EV_WAVE:
					if (!theUnder && t < 0.8f)
						Ring(g, SX(e.mA.x), SY(e.mA.y), SX(60 + t * 120), Color(230, 120, 255, (int)(230 * (1 - t / 0.8f))), 3);
					break;
				default:
					break;
				}
			}
		}

		///////////////////////////////////////////////////////////////////////
		// The tank
		///////////////////////////////////////////////////////////////////////
		void DrawTank(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			g->SetLinearBlend(true);
			ArenaSnap a = s.ViewArena(v.mArena);
			DrawBackdrop(g, v.mArena, v.mNow);
			DrawKelp(g, v.mNow, false);
			DrawFloor(g, v.mArena);
			DrawPortal(g, v.mNow);
			DrawEffects(g, v, true);
			// A tower fires when it just sent a bolt.
			bool aFiring[2] = { false, false };
			for (const Effect& fx : s.mEffects)
				if (fx.mEvent.mType == EV_BOLT && fx.mEvent.mArena == v.mArena && !Elapsed(v.mNow, fx.mAt + 300))
					for (int i = 0; i < 2; i++)
						if (Dist(fx.mEvent.mA, TheMap().mTower[i]) < 80)
							aFiring[i] = true;
			for (int i = 0; i < 2; i++)
				DrawTower(g, v.mArena, i, a, v.mNow, aFiring[i]);
			DrawCore(g, v.mArena, a, v.mNow);
			for (const FoodSnap& f : a.mFood)
				Sprite(g, IMAGE_FOOD, (v.mNow / 100 + f.mId) % 10, std::min<int>(a.mFoodQuality, 2), f.mPos.x, f.mPos.y, 0.9f, false);
			if (a.mCollectorLevel > 0)		// Stinky the pet, in his own colors (the hero is Speedy)
				Sprite(g, IMAGE_STINKY, (v.mNow / (a.mCollectorLevel >= 2 ? 45 : 80)) % 10, 0, a.mCollectorPos.x, a.mCollectorPos.y, 0.8f, a.mCollectorRight);
			for (const FishSnap& f : a.mFish)
				DrawFish(g, f, v.mNow);
			for (const MinionSnap& m : a.mMinions)
				DrawMinion(g, m, v.mNow);
			// Food, fish and minions pass behind the walls; coins (clicked) and heroes stay on top (D35).
			DrawWalls(g);
			for (const CoinSnap& c : a.mCoins)
			{
				int aRow = c.mKind == COIN_SILVER ? 0 : (c.mKind == COIN_GOLD ? 1 : 3);
				Sprite(g, IMAGE_MONEY, (v.mNow / 80 + c.mId) % 10, aRow, c.mPos.x, c.mPos.y, 0.85f, false);
			}
			std::vector<HeroSnap> aHeroes;
			s.ViewHeroes(v.mArena, aHeroes);
			for (const HeroSnap& h : aHeroes)
				DrawHero(g, h, v.mNow, h.mPlayer == s.mPlayer, v.mNames[h.mPlayer % kMaxPlayers]);
			DrawKelp(g, v.mNow, true);
			DrawEffects(g, v, false);

			// Aiming an ability: its reach around the hero.
			const HeroState& h = s.mHero;
			if (v.mAimSlot >= 0 && h.mAlive && h.mArena == v.mArena)
			{
				const AbilityDef& ab = h.Def().mAb[v.mAimSlot];
				float aReach = ab.mAim == AIM_SELF ? std::max(ab.mRadius, 60.0f) : ab.mRange;
				Ring(g, SX(h.mPos.x), SY(h.mPos.y), SX(aReach), Color(255, 255, 255, 110), 1);
			}
			// Where the mouse is: hovering an enemy shows who you'd attack.
			if (v.mMouseX >= 0 && InTank(v.mMouseX, v.mMouseY))
			{
				Vec w = ToWorld(v.mMouseX, v.mMouseY);
				std::vector<Target> aTargets;
				s.Targets(v.mArena, aTargets, true);
				for (const Target& t : aTargets)
					if (t.mTeam != s.mTeam && Dist(t.mPos, w) < t.mRadius + 14 && (t.mRef.mKind != ENT_CORE || !(a.mTowerHp[0] > 0 || a.mTowerHp[1] > 0)))
					{
						Ring(g, SX(t.mPos.x), SY(t.mPos.y), SX(t.mRadius + 8), Color(255, 80, 60, 200), 2);
						break;
					}
			}
			// Warp sickness and respawn countdowns for my hero.
			if (!h.mAlive)
			{
				g->SetColor(Color(0, 0, 0, 90));
				g->FillRect(0, kTop, kScreenW, kHudY - kTop);
				char b[48];
				snprintf(b, sizeof(b), "Back in %d", (int)std::ceil((h.mRespawnAt - v.mNow) / 1000.0f));
				Centered(g, FONT_JUNGLEFEVER17OUTLINE, b, kScreenW / 2, 220, Color(255, 255, 255));
			}
			g->SetLinearBlend(false);
		}

		///////////////////////////////////////////////////////////////////////
		// Top strip and HUD
		///////////////////////////////////////////////////////////////////////
		static void StructurePips(Graphics* g, int x, int y, const ArenaSnap* s, const Color& c, bool theMirror)
		{
			if (s == nullptr)
				return;
			for (int i = 0; i < 2; i++)
			{
				int bx = theMirror ? x - 34 * (i + 1) : x + 34 * i;
				Bar(g, bx, y, 30, 3, s->mTowerHp[i] / std::max(1.0f, s->mTowerMax), s->mTowerHp[i] > 0 ? c : Color(90, 90, 90));
			}
			int cx = theMirror ? x - 68 - 58 : x + 68;
			Bar(g, cx, y, 54, 3, s->mCoreHp / kCoreHealth, Color(255, 200, 80));
		}

		void DrawTopStrip(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			g->SetColor(Color(8, 14, 30, 235));
			g->FillRect(0, 0, kScreenW, kTop);
			ArenaSnap aMine = s.mArena.Snapshot();
			const ArenaSnap* aTheirs = s.mOther.LatestArena();
			Text(g, FONT_TINY, "YOU", 4, 9, kTeamColor[s.mTeam % 2]);
			StructurePips(g, 26, 4, &aMine, kTeamColor[s.mTeam % 2], false);
			Text(g, FONT_TINY, "THEM", kScreenW - 30, 9, kTeamColor[(s.mTeam + 1) % 2]);
			StructurePips(g, kScreenW - 34, 4, aTheirs, kTeamColor[(s.mTeam + 1) % 2], true);
			uint32_t ms = s.MatchMs();
			bool aSudden = s.mSuddenDeath;
			std::string aClock = Clock(ms) + (aSudden ? "  SUDDEN DEATH" : "");
			const HeroSnap* o = s.OtherHero();
			if (o != nullptr)
				aClock = std::to_string(s.mHero.mKills) + " - " + std::to_string(o->mKills) + "   " + aClock;
			Centered(g, FONT_TINYBOLD, aClock, kScreenW / 2, 9, aSudden ? Color(255, 110, 90) : Color(230, 235, 255));
			if (v.mArena != s.mTeam)
				Centered(g, FONT_TINY, "RIVAL'S TANK (Tab: home)", kScreenW / 2 + 118, 9, Color(255, 170, 140));
			else if (s.mHero.mArena != s.mTeam)
				Centered(g, FONT_TINY, "HOME (Tab)", kScreenW / 2 + 118, 9, Color(150, 255, 170));
		}

		// Quick-buy slots 1-4 and their shop entries.
		int QuickShop(int theSlot)
		{
			static const int kQuick[kQuickSlots] = { SHOP_GUPPY, SHOP_FOOD_COUNT, SHOP_BREEDER, SHOP_CARNIVORE };
			return kQuick[std::clamp(theSlot, 0, kQuickSlots - 1)];
		}

		static Rect AbilityRect(int i) { return Rect(196 + i * 37, kHudY + 5, 34, 34); }
		static Rect ItemRect(int i) { return Rect(348 + (i % 3) * 19, kHudY + 6 + (i / 3) * 19, 17, 17); }
		static Rect QuickRect(int i) { return Rect(410 + i * 30, kHudY + 4, 28, 36); }
		static Rect ShopButtonRect() { return Rect(530, kHudY + 4, 34, 36); }
		static Rect MapRect() { return Rect(568, kHudY + 2, 70, 40); }

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

		static void ItemIcon(Graphics* g, int theItem, float cx, float cy, float theSize)
		{
			float sc = theSize / 72.0f;
			switch (theItem)
			{
			case ITEM_SHARP_FIN: ScreenSprite(g, IMAGE_ITCHY, 0, 0, cx, cy, theSize / 64, false); break;
			case ITEM_THICK_SHELL: ScreenSprite(g, IMAGE_SHELLS, 0, 0, cx, cy, theSize / 30, false); break;
			case ITEM_SPEED_KELP: ScreenSprite(g, IMAGE_SHELLS, 0, 2, cx, cy, theSize / 30, false); break;
			case ITEM_PEARL_CHARM: ScreenSprite(g, IMAGE_PEARL, 0, 0, cx, cy, sc, false); break;
			case ITEM_TOWER_BUSTER: ScreenSprite(g, IMAGE_EXPLOSION, 5, 0, cx, cy, theSize / 70, false); break;
			case ITEM_LEECH_TOOTH: ScreenSprite(g, IMAGE_SHELLS, 0, 3, cx, cy, theSize / 30, false, Color(255, 120, 120), true); break;
			case ITEM_CORAL_ARMOR: ScreenSprite(g, IMAGE_SHELLS, 0, 1, cx, cy, theSize / 30, false, Color(255, 150, 170), true); break;
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
			case SHOP_FOOD_QUALITY: ScreenSprite(g, IMAGE_FOOD, f, theFoodRow, cx, cy, theSize / 34, false); break;	// the next level's food
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

		static void Bolt(Graphics* g, float cx, float cy, float theSize, const Color& c)
		{
			Point p[6] = { Point((int)(cx + theSize * 0.15f), (int)(cy - theSize * 0.5f)), Point((int)(cx - theSize * 0.3f), (int)(cy + theSize * 0.05f)),
				Point((int)(cx - theSize * 0.02f), (int)(cy + theSize * 0.05f)), Point((int)(cx - theSize * 0.15f), (int)(cy + theSize * 0.5f)),
				Point((int)(cx + theSize * 0.3f), (int)(cy - theSize * 0.08f)), Point((int)(cx + theSize * 0.02f), (int)(cy - theSize * 0.08f)) };
			g->SetColor(c);
			g->PolyFill(p, 6, false);
		}

		// A picture for each ability, from the game's own art.
		static void AbilityIcon(Graphics* g, int theHero, int theSlot, float cx, float cy, float theSize, uint32_t theNow)
		{
			int f = (int)((theNow / 90) % 10);
			float k = theSize / 32.0f;
			switch (theHero * AB_COUNT + theSlot)
			{
			case HERO_ITCHY * AB_COUNT + AB_Q: ScreenSprite(g, IMAGE_ITCHY, f, 0, cx, cy, 0.42f * k, true); Bolt(g, cx - 8 * k, cy, 10 * k, Color(255, 255, 255, 160)); break;
			case HERO_ITCHY * AB_COUNT + AB_W: ScreenSprite(g, IMAGE_ITCHY, 0, 0, cx, cy, 0.38f * k, true); ScreenSprite(g, IMAGE_SPARKLE, f % 10, 0, cx + 7 * k, cy - 5 * k, 1.1f * k, false); break;
			case HERO_ITCHY * AB_COUNT + AB_E: ScreenSprite(g, IMAGE_BUBBLES, f % 5, 0, cx, cy, 1.0f * k, false); ScreenSprite(g, IMAGE_ITCHY, f, 0, cx, cy, 0.3f * k, true, Color(255, 255, 255, 120)); break;
			case HERO_ITCHY * AB_COUNT + AB_R: Ring(g, cx, cy, 11 * k, Color(220, 240, 255), 2); ScreenSprite(g, IMAGE_ITCHY, 5, 1, cx, cy, 0.34f * k, false); break;
			case HERO_CLYDE * AB_COUNT + AB_Q: ScreenSprite(g, IMAGE_ENERGYBALL, f % 6, 0, cx, cy, 0.34f * k, false, Color(120, 220, 255), true); break;
			case HERO_CLYDE * AB_COUNT + AB_W: Disc(g, cx, cy, 11 * k, Color(120, 220, 255, 80)); Ring(g, cx, cy, 11 * k, Color(150, 230, 255), 1); Bolt(g, cx, cy, 14 * k, Color(200, 240, 255)); break;
			case HERO_CLYDE * AB_COUNT + AB_E: ScreenSprite(g, IMAGE_WARPHOLE, (theNow / 70) % 17, 0, cx, cy, 0.13f * k, false); ScreenSprite(g, IMAGE_CLYDE, f, 0, cx + 4 * k, cy, 0.25f * k, false, Color(255, 255, 255, 170)); break;
			case HERO_CLYDE * AB_COUNT + AB_R: Bolt(g, cx - 6 * k, cy, 16 * k, Color(255, 240, 120)); Bolt(g, cx + 6 * k, cy + 2 * k, 14 * k, Color(170, 230, 255)); break;
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
			default: ScreenSprite(g, IMAGE_MONEY, f, 1, cx, cy, 0.42f * k, false); break;
			}
		}

		static void MiniMap(Graphics* g, const Rect& r, const Side& s, int theArena, uint32_t theNow, bool theAlert = false)
		{
			bool aFlash = theAlert && (theNow / 250) % 2 == 0;
			g->SetColor(aFlash ? Color(120, 20, 20, 235) : Color(10, 30, 60, 230));
			g->FillRect(r);
			g->SetColor(theAlert ? Color(255, 80, 70, 230) : Color(120, 170, 220, 160));
			g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
			if (theAlert)
				g->DrawRect(r.mX + 1, r.mY + 1, r.mWidth - 3, r.mHeight - 3);
			ArenaSnap a = s.ViewArena(theArena);
			auto P = [&](Vec w) { return Point(r.mX + (int)(w.x / kWorldW * r.mWidth), r.mY + (int)(w.y / kWorldH * r.mHeight)); };
			for (const FishSnap& f : a.mFish)
			{
				Point p = P(f.mPos);
				g->SetColor(Color(255, 200, 80));
				g->FillRect(p.mX, p.mY, 1, 1);
			}
			for (const MinionSnap& m : a.mMinions)
			{
				Point p = P(m.mPos);
				g->SetColor(Color(255, 70, 70));
				g->FillRect(p.mX - 1, p.mY - 1, 2, 2);
			}
			for (int i = 0; i < 2; i++)
			{
				Point p = P(TheMap().mTower[i]);
				g->SetColor(a.mTowerHp[i] > 0 ? kTeamColor[theArena % 2] : Color(80, 80, 80));
				g->FillRect(p.mX - 2, p.mY - 2, 4, 4);
			}
			std::vector<HeroSnap> aHeroes;
			s.ViewHeroes(theArena, aHeroes);
			for (const HeroSnap& h : aHeroes)
				if (h.mFlags & HF_ALIVE)
				{
					Point p = P(h.mPos);
					Disc(g, (float)p.mX, (float)p.mY, 2.5f, kTeamColor[h.mTeam % 2], 8);
				}
			Text(g, FONT_TINY, theArena == s.mTeam ? "home" : "rival", r.mX + 2, r.mY + r.mHeight - 2, Color(200, 220, 255, 170));
		}

		void DrawHud(Graphics* g, const ViewState& v)
		{
			const Side& s = *v.mSide;
			const HeroState& h = s.mHero;
			const HeroDef& d = h.Def();
			g->SetColor(Color(12, 20, 42, 245));
			g->FillRect(0, kHudY, kScreenW, kScreenH - kHudY);
			g->SetColor(Color(90, 130, 190, 200));
			g->FillRect(0, kHudY, kScreenW, 1);

			// Portrait, level, XP.
			ScreenSprite(g, HeroPortrait(h.mHero), (v.mNow / 100) % 10, 0, 22, kHudY + 22, 0.66f, false);
			Disc(g, 38, kHudY + 36, 7, Color(20, 20, 50, 240), 12);
			Centered(g, FONT_TINYBOLD, std::to_string(h.mLevel), 38, kHudY + 40, Color(255, 235, 150));
			// Health, XP, money.
			float aMax = s.MaxHp();
			Bar(g, 48, kHudY + 6, 142, 9, h.mHp / std::max(1.0f, aMax), Color(90, 220, 100));
			char b[64];
			snprintf(b, sizeof(b), "%d / %d", (int)std::max(0.0f, h.mHp), (int)aMax);
			Centered(g, FONT_CONTINUUMBOLD12 ? FONT_CONTINUUMBOLD12 : FONT_TINY, b, 119, kHudY + 15, Color(255, 255, 255));
			float aXpFrac = h.mLevel >= kMaxLevel ? 1.0f : h.mXp / XpForLevel(h.mLevel);
			Bar(g, 48, kHudY + 19, 142, 3, aXpFrac, Color(170, 130, 255));
			ScreenSprite(g, IMAGE_MONEY, (v.mNow / 90) % 10, 1, 56, kHudY + 32, 0.26f, false);
			snprintf(b, sizeof(b), "%d", s.mArena.mMoney);
			Text(g, FONT_CONTINUUMBOLD12OUTLINE ? FONT_CONTINUUMBOLD12OUTLINE : FONT_TINYBOLD, b, 66, kHudY + 38, Color(255, 225, 90));
			snprintf(b, sizeof(b), "%d/%d", h.mKills, h.mDeaths);
			Text(g, FONT_TINY, b, 160, kHudY + 36, Color(220, 220, 240));
			uint32_t aSick = s.WarpSicknessLeft();
			if (aSick > 0)
			{
				snprintf(b, sizeof(b), "warp %ds", (int)std::ceil(aSick / 1000.0f));
				Text(g, FONT_TINY, b, 110, kHudY + 30, Color(200, 160, 255));
			}
			if (d.mWalker && (int32_t)(v.mNow - h.mHopReadyAt) < 0)
			{
				snprintf(b, sizeof(b), "hop %ds", (int)std::ceil((h.mHopReadyAt - v.mNow) / 1000.0f));
				Text(g, FONT_TINY, b, 110, kHudY + 40, Color(160, 220, 255));
			}

			// Abilities.
			for (int i = 0; i < AB_COUNT; i++)
			{
				Rect r = AbilityRect(i);
				bool aLocked = h.mRank[i] <= 0;
				g->SetColor(aLocked ? Color(30, 30, 40) : Color(34, 60, 100));
				g->FillRect(r);
				AbilityIcon(g, h.mHero, i, r.mX + 17.0f, r.mY + 17.0f, 32, v.mNow);
				uint32_t aLeft = s.CooldownLeft(i);
				if (aLeft > 0 && !aLocked)
				{
					int aRank = i == AB_R ? std::max(1, h.mRank[AB_R]) : h.mRank[i];
					float aTotal = d.mAb[i].mCooldownS * RankCooldown(aRank) * s.CooldownMult() * 1000;
					int aH = (int)(r.mHeight * std::min(1.0f, aLeft / std::max(1.0f, aTotal)));
					g->SetColor(Color(0, 0, 0, 170));
					g->FillRect(r.mX, r.mY + r.mHeight - aH, r.mWidth, aH);
					Centered(g, FONT_TINYBOLD, std::to_string((int)std::ceil(aLeft / 1000.0f)), r.mX + 17, r.mY + 22, Color(255, 255, 255));
				}
				if (aLocked)
					Centered(g, FONT_TINY, "Lv5", r.mX + 17, r.mY + 22, Color(160, 160, 170));
				g->SetColor(v.mAimSlot == i ? Color(255, 255, 255) : Color(120, 160, 220));
				g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
				Text(g, FONT_TINYBOLD, kAbilityKeys[i], r.mX + 2, r.mY + 9, Color(255, 240, 160));
				// Rank pips.
				int aMaxRank = i == AB_R ? 2 : kMaxRank;
				for (int k = 0; k < aMaxRank; k++)
				{
					g->SetColor(k < h.mRank[i] ? Color(255, 220, 90) : Color(60, 60, 80));
					g->FillRect(r.mX + 3 + k * 7, r.mY + r.mHeight - 4, 5, 2);
				}
				if (h.mPoints > 0 && i < AB_R && h.mRank[i] < kMaxRank)
					Text(g, FONT_TINYBOLD, "+", r.mX + r.mWidth - 8, r.mY + 9, Color(120, 255, 120));
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
			// Quick-buy and the shop button.
			for (int i = 0; i < kQuickSlots; i++)
			{
				Rect r = QuickRect(i);
				int aShop = QuickShop(i);
				bool aCan = s.CanBuy(aShop);
				g->SetColor(aCan ? Color(40, 70, 50) : Color(40, 36, 44));
				g->FillRect(r);
				ShopIcon(g, aShop, r.mX + 14.0f, r.mY + 14.0f, 22, v.mNow, std::min(s.mArena.mFoodQuality + 1, 2));
				Text(g, FONT_TINY, std::to_string(i + 1), r.mX + 2, r.mY + 8, Color(255, 240, 160));
				Centered(g, FONT_TINY, "$" + std::to_string(s.Price(aShop)), r.mX + 14, r.mY + 34, aCan ? Color(255, 225, 90) : Color(150, 140, 150));
			}
			Rect sb = ShopButtonRect();
			g->SetColor(v.mShopTab >= 0 ? Color(90, 70, 30) : Color(60, 50, 24));
			g->FillRect(sb);
			ScreenSprite(g, IMAGE_MONEY, (v.mNow / 90) % 10, 4, sb.mX + 17.0f, sb.mY + 15.0f, 0.36f, false);
			Centered(g, FONT_TINYBOLD, "B", sb.mX + 17, sb.mY + 34, Color(255, 240, 160));
			// Mini-map: the other tank (or home while away).
			int aMapArena = h.mArena == s.mTeam && v.mArena == s.mTeam ? 1 - s.mTeam : s.mTeam;
			if (v.mArena != s.mTeam)
				aMapArena = s.mTeam;
			bool aAlert = !v.mAlert.empty() && v.mArena != s.mTeam && !Elapsed(v.mNow, v.mAlertAt + kAlertShowMs);
			MiniMap(g, MapRect(), s, aMapArena, v.mNow, aAlert && aMapArena == s.mTeam);
			// Home under attack while you're looking at the rival's tank: a red banner.
			if (aAlert)
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
			// Hovering an ability: what it does.
			for (int i = 0; i < AB_COUNT; i++)
				if (AbilityRect(i).Contains(v.mMouseX, v.mMouseY))
				{
					const AbilityDef& ab = d.mAb[i];
					std::string aTip = std::string(kAbilityKeys[i]) + " " + ab.mName + ": " + ab.mDesc;
					Font* f = FONT_TINY;
					int w = std::min(420, (f != nullptr ? f->StringWidth(aTip) : 200) + 12);
					g->SetColor(Color(0, 0, 20, 220));
					g->FillRect(std::min(kScreenW - w - 4, 150), kHudY - 40, w, 34);
					Wrapped(g, f, aTip, std::min(kScreenW - w - 4, 150) + 6, kHudY - 28, w - 12, Color(235, 240, 255));
				}
		}

		///////////////////////////////////////////////////////////////////////
		// The shop
		///////////////////////////////////////////////////////////////////////
		static const Rect kShopRect(70, 40, 500, 330);
		static const char* kTabNames[TAB_COUNT] = { "Fish", "Upgrades", "Hero", "Towers", "Minions" };

		static std::vector<int> TabEntries(int theTab)
		{
			std::vector<int> v;
			for (int i = 0; i < SHOP_COUNT; i++)
				if (ShopDefOf(i).mTab == theTab)
					v.push_back(i);
			return v;
		}

		static Rect TabRect(int i) { return Rect(kShopRect.mX + 12 + i * 96, kShopRect.mY + 30, 92, 22); }
		static Rect EntryRect(int i) { return Rect(kShopRect.mX + 12 + (i % 2) * 240, kShopRect.mY + 60 + (i / 2) * 50, 234, 46); }

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
			g->SetColor(Color(6, 14, 34, 238));
			g->FillRect(R);
			g->SetColor(Color(255, 205, 80, 220));
			g->DrawRect(R.mX, R.mY, R.mWidth - 1, R.mHeight - 1);
			Text(g, FONT_JUNGLEFEVER12OUTLINE, "SHOP", R.mX + 12, R.mY + 22, Color(255, 215, 90));
			char b[96];
			snprintf(b, sizeof(b), "$%d", s.mArena.mMoney);
			Text(g, FONT_JUNGLEFEVER12OUTLINE, b, R.mX + 90, R.mY + 22, Color(255, 235, 150));
			Text(g, FONT_TINY, "B or Esc closes. The game keeps going!", R.mX + R.mWidth - 190, R.mY + 18, Color(180, 190, 220));
			for (int i = 0; i < TAB_COUNT; i++)
			{
				Rect r = TabRect(i);
				bool aOn = i == v.mShopTab;
				g->SetColor(aOn ? Color(80, 60, 20) : Color(26, 36, 60));
				g->FillRect(r);
				Centered(g, FONT_JUNGLEFEVER10OUTLINE, kTabNames[i], r.mX + r.mWidth / 2, r.mY + 16, aOn ? Color(255, 230, 120) : Color(190, 200, 230));
			}
			std::vector<int> e = TabEntries(v.mShopTab);
			int aHover = -1;
			for (size_t i = 0; i < e.size(); i++)
			{
				Rect r = EntryRect((int)i);
				int aShop = e[i];
				std::string aWhy;
				bool aCan = s.CanBuy(aShop, &aWhy);
				bool aOver = r.Contains(v.mMouseX, v.mMouseY);
				if (aOver)
					aHover = aShop;
				g->SetColor(aOver ? Color(50, 70, 110) : Color(22, 32, 56));
				g->FillRect(r);
				ShopIcon(g, aShop, r.mX + 23.0f, r.mY + 23.0f, 36, v.mNow, std::min(s.mArena.mFoodQuality + 1, 2));
				const ShopDef& sd = ShopDefOf(aShop);
				Text(g, FONT_JUNGLEFEVER10OUTLINE, sd.mName, r.mX + 48, r.mY + 16, aCan ? Color(255, 255, 255) : Color(170, 170, 185));
				int aPrice = s.Price(aShop);
				int aOwned = s.Owned(aShop);
				std::string aSub = aPrice > 0 ? "$" + std::to_string(aPrice) : "maxed";
				if (aOwned > 0)
					aSub += "   (have " + std::to_string(aOwned) + ")";
				Text(g, FONT_TINY, aSub, r.mX + 48, r.mY + 30, aCan ? Color(255, 225, 90) : Color(190, 140, 130));
				if (!aCan && !aWhy.empty() && aWhy != "Not enough money.")
					Text(g, FONT_TINY, aWhy, r.mX + 48, r.mY + 41, Color(180, 150, 150));
			}
			// What the hovered entry does.
			if (aHover >= 0)
			{
				g->SetColor(Color(0, 0, 0, 160));
				g->FillRect(R.mX + 12, R.mY + R.mHeight - 30, R.mWidth - 24, 22);
				Text(g, FONT_TINY, ShopDefOf(aHover).mDesc, R.mX + 18, R.mY + R.mHeight - 15, Color(230, 240, 255));
			}
		}

		void DrawFeed(Graphics* g, const ViewState& v)
		{
			// Kills and towers falling, top left under the strip.
			const Side& s = *v.mSide;
			int y = kTop + 14;
			for (const Effect& fx : s.mEffects)
			{
				const Event& e = fx.mEvent;
				float t = (v.mNow - fx.mAt) / 1000.0f;
				if (t > 4)
					continue;
				int a = (int)(255 * Clamp(4 - t, 0, 1));
				std::string aLine;
				Color c(255, 255, 255, a);
				if (e.mType == EV_KILL)
				{
					std::string aKiller = e.mPlayer >= 0 ? v.mNames[e.mPlayer % kMaxPlayers] : "A tower";
					aLine = aKiller + " took down " + v.mNames[e.mId % kMaxPlayers] + "!";
					c = e.mPlayer == s.mPlayer ? Color(255, 225, 90, a) : Color(255, 130, 110, a);
				}
				else if (e.mType == EV_TOWER_DOWN)
				{
					aLine = e.mArena == s.mTeam ? "Your tower fell!" : "Their tower fell!";
					c = e.mArena == s.mTeam ? Color(255, 110, 90, a) : Color(130, 255, 140, a);
				}
				else if (e.mType == EV_WAVE && e.mArena == s.mTeam)
				{
					aLine = "Minions coming through your portal!";
					c = Color(230, 160, 255, a);
				}
				else if (e.mType == EV_CROSS && e.mPlayer != s.mPlayer && e.mArena == s.mTeam)
				{
					aLine = v.mNames[e.mPlayer % kMaxPlayers] + " is raiding your tank!";
					c = Color(255, 120, 100, a);
				}
				if (aLine.empty())
					continue;
				Text(g, FONT_JUNGLEFEVER10OUTLINE, aLine, 8, y, c);
				y += 14;
			}
		}

		///////////////////////////////////////////////////////////////////////
		// Draft
		///////////////////////////////////////////////////////////////////////
		static Rect CardRect(int i) { return Rect(14 + i * 124, 64, 116, 300); }
		static Rect DraftButtonRect(int theButton)
		{
			switch (theButton)
			{
			case DB_OPPONENT: return Rect(40, 380, 180, 32);
			case DB_SKILL: return Rect(230, 380, 150, 32);
			case DB_START: return Rect(420, 376, 180, 40);
			default: return Rect(40, 430, 110, 28);
			}
		}

		int DraftHit(int x, int y, bool thePractice)
		{
			for (int i = 0; i < HERO_COUNT; i++)
				if (CardRect(i).Contains(x, y))
					return i;
			static const int kButtons[] = { DB_OPPONENT, DB_SKILL, DB_START, DB_BACK };
			for (int b : kButtons)
			{
				if (!thePractice && (b == DB_OPPONENT || b == DB_SKILL))
					continue;
				if (DraftButtonRect(b).Contains(x, y))
					return b;
			}
			return DB_NONE;
		}

		static void StatBar(Graphics* g, int x, int y, const char* theLabel, float theFrac, const Color& c)
		{
			Text(g, FONT_TINY, theLabel, x, y + 5, Color(200, 210, 230));
			Bar(g, x + 42, y, 60, 4, theFrac, c);
		}

		static void Button(Graphics* g, const Rect& r, const std::string& theLabel, bool theOn, bool theHover)
		{
			g->SetColor(theOn ? (theHover ? Color(110, 150, 60) : Color(80, 120, 40)) : Color(50, 50, 60));
			g->FillRect(r);
			g->SetColor(Color(255, 230, 140, 200));
			g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
			Centered(g, FONT_JUNGLEFEVER12OUTLINE, theLabel, r.mX + r.mWidth / 2, r.mY + r.mHeight / 2 + 6, theOn ? Color(255, 255, 255) : Color(150, 150, 160));
		}

		void DrawDraft(Graphics* g, uint32_t theNow, int theHover, int theMine, int theTheirs, bool thePractice, int theBotHero, int theBotSkill, const std::string& theStatus)
		{
			g->SetLinearBlend(true);
			Image* b = IMAGE_AQUARIUM3;
			if (b != nullptr)
				g->DrawImage(b, Rect(0, 0, kScreenW, kScreenH), Rect(0, 0, b->mWidth, b->mHeight));
			g->SetColor(Color(0, 10, 30, 170));
			g->FillRect(0, 0, kScreenW, kScreenH);
			Centered(g, FONT_JUNGLEFEVER17OUTLINE, "PET HEROES", kScreenW / 2, 30, Color(255, 215, 80));
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, thePractice ? "Practice against the bot. Pick your hero:" : "Pick your hero (your rival can't see it until you both lock in):", kScreenW / 2, 52, Color(220, 230, 255));
			for (int i = 0; i < HERO_COUNT; i++)
			{
				const HeroDef& d = HeroDefOf(i);
				Rect r = CardRect(i);
				bool aMine = i == theMine, aHover = i == theHover;
				g->SetColor(aMine ? Color(70, 60, 20, 235) : (aHover ? Color(30, 50, 90, 235) : Color(16, 26, 50, 225)));
				g->FillRect(r);
				g->SetColor(aMine ? Color(255, 215, 80) : Color(90, 120, 170));
				g->DrawRect(r.mX, r.mY, r.mWidth - 1, r.mHeight - 1);
				if (aMine)
					g->DrawRect(r.mX + 1, r.mY + 1, r.mWidth - 3, r.mHeight - 3);
				int aRow = 0;
				ScreenSprite(g, HeroImage(i), (theNow / 80) % 10, aRow, r.mX + r.mWidth / 2.0f, r.mY + 52.0f, aHover || aMine ? 1.25f : 1.1f, false);
				Centered(g, FONT_JUNGLEFEVER12OUTLINE, d.mName, r.mX + r.mWidth / 2, r.mY + 106, Color(255, 255, 255));
				Centered(g, FONT_JUNGLEFEVER10OUTLINE, d.mRole, r.mX + r.mWidth / 2, r.mY + 121, Color(255, 205, 90));
				int y = r.mY + 132;
				StatBar(g, r.mX + 6, y, "Health", d.mHealth / 800, Color(110, 230, 110));
				StatBar(g, r.mX + 6, y + 9, "Attack", d.mDamage / d.mAttackS / 40, Color(250, 110, 90));
				StatBar(g, r.mX + 6, y + 18, "Range", d.mRange / 260, Color(120, 190, 255));
				StatBar(g, r.mX + 6, y + 27, "Speed", d.mSpeed / 250, Color(255, 225, 90));
				Text(g, FONT_TINY, d.mWalker ? "Walks the floor" : "Swims", r.mX + 6, y + 45, Color(200, 190, 255));
				int ty = y + 57;
				ty = Wrapped(g, FONT_TINY, std::string(d.mPassiveName) + ": " + d.mPassive, r.mX + 6, ty, r.mWidth - 12, Color(200, 230, 255));
				for (int k = 0; k < AB_COUNT && ty < r.mY + r.mHeight - 6; k++)
				{
					Text(g, FONT_TINY, std::string(kAbilityKeys[k]) + " " + d.mAb[k].mName, r.mX + 6, ty, k == AB_R ? Color(255, 190, 120) : Color(235, 235, 245));
					ty += 10;
				}
				if (aHover && !aMine)
				{
					g->SetColor(Color(0, 0, 0, 200));
					g->FillRect(r.mX + 4, r.mY + r.mHeight - 44, r.mWidth - 8, 40);
					Wrapped(g, FONT_TINY, d.mBlurb, r.mX + 8, r.mY + r.mHeight - 34, r.mWidth - 16, Color(255, 240, 200));
				}
			}
			if (thePractice)
			{
				std::string aOpp = std::string("Bot: ") + (theBotHero < 0 ? "random" : HeroDefOf(theBotHero).mName);
				static const char* kSkill[3] = { "Easy", "Normal", "Hard" };
				Button(g, DraftButtonRect(DB_OPPONENT), aOpp, true, false);
				Button(g, DraftButtonRect(DB_SKILL), std::string("Bot: ") + kSkill[std::clamp(theBotSkill, 0, 2)], true, false);
			}
			Button(g, DraftButtonRect(DB_START), thePractice ? "Start!" : "Lock in!", theMine >= 0, false);
			Button(g, DraftButtonRect(DB_BACK), "Back", true, false);
			if (!theStatus.empty())
				Centered(g, FONT_JUNGLEFEVER10OUTLINE, theStatus, kScreenW / 2, 470, Color(255, 225, 150));
			(void)theTheirs;
			g->SetLinearBlend(false);
		}

		void DrawCountdown(Graphics* g, int theSeconds)
		{
			g->SetColor(Color(0, 0, 0, 90));
			g->FillRect(0, kTop, kScreenW, kHudY - kTop);
			Centered(g, FONT_JUNGLEFEVER17OUTLINE, theSeconds > 0 ? std::to_string(theSeconds) : "GO!", kScreenW / 2, 220, Color(255, 225, 90));
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, "WASD move  -  right-click attack  -  Q E R F abilities", kScreenW / 2, 250, Color(230, 240, 255));
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, "B shop  -  Tab home  -  hold H: help", kScreenW / 2, 266, Color(230, 240, 255));
		}

		void DrawHelp(Graphics* g, int theHero, bool theWaiting)
		{
			Rect R(40, 30, 560, 400);
			g->SetColor(Color(4, 12, 32, 240));
			g->FillRect(R);
			g->SetColor(Color(255, 205, 80, 220));
			g->DrawRect(R.mX, R.mY, R.mWidth - 1, R.mHeight - 1);
			Centered(g, FONT_JUNGLEFEVER15OUTLINE, "HOW TO PLAY PET HEROES", kScreenW / 2, R.mY + 26, Color(255, 215, 80));
			Font* f = FONT_JUNGLEFEVER10OUTLINE;
			Color k(255, 230, 140), t(235, 240, 255), d(190, 200, 225);
			int y = R.mY + 52;
			auto Line = [&](const std::string& theKey, const std::string& theText) {
				Text(g, f, theKey, R.mX + 18, y, k);
				int aBottom = Wrapped(g, FONT_TINY, theText, R.mX + 150, y - 8, R.mWidth - 168, t);
				y = std::max(y + 20, aBottom + 12);
			};
			Line("W A S D", "Move. Walkers (on the floor): A and D walk, W or Space hops over minions' bites, S near the portal's beam or a floor pad crosses.");
			Line("Right-click", "Attack what's under the cursor, or move there (hold to keep moving).");
			Line("Q E R F", "Your abilities, aimed at the mouse (F unlocks at level 5). Ctrl+Q/E/R: pick the next upgrade.");
			Line("Left-click", "In your own tank: collect coins, drop food ($5), zap invaders with your laser.");
			Line("1 2 3 4 / B", "Quick-buy a guppy, more food, a breeder, a carnivore. B opens the full shop: items, towers, minions.");
			Line("Tab (hold)", "Look at your own tank while your hero is away.");
			Line("The portal", "Top middle: cross to the rival's tank and back (walkers: S near its beam). The floor pads in the corners go there too.");
			Line("Win", "Minion waves attack the rival's towers every 30 s. Break both towers, then their treasure chest core.");
			Line("Tips", "Feed your fish: they pay for everything. You're stronger in your own tank. Kelp hides you. Towers hurt: push with your minions.");
			const HeroDef& h = HeroDefOf(theHero);
			y += 6;
			Text(g, FONT_JUNGLEFEVER12OUTLINE, std::string(h.mName) + " (" + h.mRole + ")", R.mX + 18, y, Color(120, 230, 255));
			y += 16;
			Wrapped(g, FONT_TINY, std::string(h.mPassiveName) + ": " + h.mPassive, R.mX + 18, y, R.mWidth - 36, d);
			y += 12;
			for (int i = 0; i < AB_COUNT; i++)
			{
				Wrapped(g, FONT_TINY, std::string(kAbilityKeys[i]) + " " + h.mAb[i].mName + ": " + h.mAb[i].mDesc, R.mX + 18, y, R.mWidth - 36, i == AB_R ? Color(255, 200, 130) : d);
				y += 11;
			}
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, theWaiting ? "Press any key or click to start!" : "Hold H to see this again.", kScreenW / 2, R.mY + R.mHeight - 10, Color(255, 225, 150));
		}

		static Rect ResultButtonRect() { return Rect(kScreenW / 2 - 120, 400, 240, 36); }
		int ResultHit(int x, int y) { return ResultButtonRect().Contains(x, y) ? 1 : 0; }

		void DrawResult(Graphics* g, const ViewState& v, bool theWon, const std::string& theReason, uint32_t theMatchMs,
			const HeroSnap& theOther, int theOtherFishLost, int theOtherEarned)
		{
			const Side& s = *v.mSide;
			g->SetColor(Color(0, 0, 0, 170));
			g->FillRect(0, 0, kScreenW, kScreenH);
			Rect R(90, 70, 460, 380);
			g->SetColor(Color(10, 20, 44, 245));
			g->FillRect(R);
			g->SetColor(theWon ? Color(255, 215, 80) : Color(230, 100, 90));
			g->DrawRect(R.mX, R.mY, R.mWidth - 1, R.mHeight - 1);
			Centered(g, FONT_JUNGLEFEVER17OUTLINE, theWon ? "VICTORY!" : "DEFEAT", kScreenW / 2, R.mY + 34, theWon ? Color(255, 215, 80) : Color(240, 110, 100));
			Centered(g, FONT_JUNGLEFEVER10OUTLINE, theReason, kScreenW / 2, R.mY + 56, Color(220, 230, 255));
			Centered(g, FONT_TINY, "Match time " + Clock(theMatchMs), kScreenW / 2, R.mY + 72, Color(190, 200, 230));
			// Two columns: you and them.
			const HeroState& h = s.mHero;
			struct Col { std::string mName; int mHero, mLevel, mKills, mDeaths, mTowers, mFishLost, mEarned; };
			Col aCols[2] = {
				{ v.mNames[s.mPlayer % kMaxPlayers], h.mHero, h.mLevel, h.mKills, h.mDeaths, h.mTowers, s.mArena.mFishLost, s.mArena.mMoneyEarned },
				{ v.mNames[s.mOtherPlayer % kMaxPlayers], theOther.mHero, theOther.mLevel, theOther.mKills, theOther.mDeaths, theOther.mTowers, theOtherFishLost, theOtherEarned },
			};
			for (int c = 0; c < 2; c++)
			{
				int cx = R.mX + 120 + c * 220;
				const Col& k = aCols[c];
				ScreenSprite(g, HeroImage(k.mHero), (v.mNow / 80) % 10, 0, (float)cx, R.mY + 118.0f, 1.2f, c == 0);
				Centered(g, FONT_JUNGLEFEVER12OUTLINE, k.mName, cx, R.mY + 170, kTeamColor[(c == 0 ? s.mTeam : 1 - s.mTeam) % 2]);
				Centered(g, FONT_JUNGLEFEVER10OUTLINE, std::string(HeroDefOf(k.mHero).mName) + "  level " + std::to_string(k.mLevel), cx, R.mY + 188, Color(230, 230, 240));
				int y = R.mY + 212;
				auto Row = [&](const char* theLabel, const std::string& theValue) {
					Text(g, FONT_JUNGLEFEVER10OUTLINE, theLabel, cx - 90, y, Color(190, 200, 230));
					Text(g, FONT_JUNGLEFEVER10OUTLINE, theValue, cx + 50, y, Color(255, 255, 255));
					y += 20;
				};
				Row("Kills / deaths", std::to_string(k.mKills) + " / " + std::to_string(k.mDeaths));
				if (k.mTowers >= 0)
					Row("Towers taken", std::to_string(k.mTowers));
				Row("Fish lost", std::to_string(k.mFishLost));
				Row("Money earned", "$" + std::to_string(k.mEarned));
			}
			Button(g, ResultButtonRect(), "Back to the menu (Enter)", true, ResultButtonRect().Contains(v.mMouseX, v.mMouseY));
		}
	}
}
