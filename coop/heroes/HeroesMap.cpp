#include "HeroesMap.h"
#include <queue>

namespace Heroes
{
	float FloorY(float x)
	{
		const std::vector<Vec>& f = TheMap().mFloor;
		if (x <= f.front().x)
			return f.front().y;
		for (size_t i = 1; i < f.size(); i++)
			if (x <= f[i].x)
			{
				float t = (x - f[i - 1].x) / std::max(1e-3f, f[i].x - f[i - 1].x);
				return f[i - 1].y + (f[i].y - f[i - 1].y) * t;
			}
		return f.back().y;
	}

	Vec ClampToWater(Vec p, float r)
	{
		p.x = Clamp(p.x, r, kWorldW - r);
		p.y = Clamp(p.y, kSurfaceY + r, FloorY(p.x) - r);
		return p;
	}

	Vec WalkerPos(float x, float r)
	{
		x = Clamp(x, r, kWorldW - r);
		return Vec(x, FloorY(x) - r);
	}

	bool SwimmerFits(Vec p, float r)
	{
		if (p.x < r || p.x > kWorldW - r || p.y < kSurfaceY + r || p.y > FloorY(p.x) - r)
			return false;
		for (const WallDef& w : TheMap().mWalls)
			if (DistPointPoly(w.mPoly, p) < r)
				return false;
		return true;
	}

	float FirstBlock(Vec a, Vec b)
	{
		float aBest = 2;
		for (const WallDef& w : TheMap().mWalls)
		{
			float t;
			if (SegPoly(a, b, w.mPoly, &t) && t < aBest)
				aBest = t;
		}
		// The floor: a segment that dips below it. Points resting on the floor (walkers,
		// towers, the core) sit a little above it, so a small margin keeps them visible.
		const std::vector<Vec>& f = TheMap().mFloor;
		for (size_t i = 1; i < f.size(); i++)
		{
			float t;
			Vec c = f[i - 1] + Vec(0, 6), d = f[i] + Vec(0, 6);
			if (SegSeg(a, b, c, d, &t) && t < aBest)
				aBest = t;
		}
		return aBest;
	}

	bool WallBetween(Vec a, Vec b)
	{
		for (const WallDef& w : TheMap().mWalls)
			if (SegPoly(a, b, w.mPoly, nullptr))
				return true;
		return false;
	}

	bool FloorBetween(Vec a, Vec b)
	{
		const std::vector<Vec>& f = TheMap().mFloor;
		for (size_t i = 1; i < f.size(); i++)
			if (SegSeg(a, b, f[i - 1] + Vec(0, 6), f[i] + Vec(0, 6), nullptr))
				return true;
		return false;
	}

	bool LineOfSight(Vec a, Vec b) { return !WallBetween(a, b) && !FloorBetween(a, b); }

	int KelpAt(Vec p)
	{
		const std::vector<KelpDef>& k = TheMap().mKelp;
		for (size_t i = 0; i < k.size(); i++)
			if (p.x >= k[i].mX0 && p.x <= k[i].mX1 && p.y >= k[i].mY0 && p.y <= k[i].mY1)
				return (int)i;
		return -1;
	}

	///////////////////////////////////////////////////////////////////////////
	// Path grid
	///////////////////////////////////////////////////////////////////////////
	static const int kGW = (int)(kWorldW / kPathCell), kGH = (int)(kWorldH / kPathCell);

	struct Grid
	{
		std::vector<uint8_t> mOpen;
		Grid()
		{
			mOpen.resize(kGW * kGH);
			for (int y = 0; y < kGH; y++)
				for (int x = 0; x < kGW; x++)
					mOpen[y * kGW + x] = SwimmerFits(Center(x, y), kPathRadius) ? 1 : 0;
		}
		static Vec Center(int x, int y) { return Vec((x + 0.5f) * kPathCell, (y + 0.5f) * kPathCell); }
		bool Open(int x, int y) const { return x >= 0 && y >= 0 && x < kGW && y < kGH && mOpen[y * kGW + x] != 0; }
		static void Cell(Vec p, int& x, int& y)
		{
			x = std::clamp((int)(p.x / kPathCell), 0, kGW - 1);
			y = std::clamp((int)(p.y / kPathCell), 0, kGH - 1);
		}
	};

	static const Grid& TheGrid()
	{
		static const Grid kGrid;
		return kGrid;
	}

	Vec NearestOpen(Vec p)
	{
		const Grid& g = TheGrid();
		p = ClampToWater(p, kPathRadius);
		if (SwimmerFits(p, kPathRadius))
			return p;
		int sx, sy;
		Grid::Cell(p, sx, sy);
		// Breadth-first search outward for the nearest open cell.
		std::vector<uint8_t> aSeen(kGW * kGH, 0);
		std::queue<int> q;
		q.push(sy * kGW + sx);
		aSeen[sy * kGW + sx] = 1;
		while (!q.empty())
		{
			int c = q.front();
			q.pop();
			int x = c % kGW, y = c / kGW;
			if (g.Open(x, y))
				return Grid::Center(x, y);
			static const int kD[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
			for (const auto& d : kD)
			{
				int nx = x + d[0], ny = y + d[1];
				if (nx >= 0 && ny >= 0 && nx < kGW && ny < kGH && !aSeen[ny * kGW + nx])
				{
					aSeen[ny * kGW + nx] = 1;
					q.push(ny * kGW + nx);
				}
			}
		}
		return p;
	}

	bool OpenLine(Vec a, Vec b)
	{
		const Grid& g = TheGrid();
		float l = Dist(a, b);
		int n = std::max(1, (int)(l / (kPathCell * 0.5f)));
		for (int i = 0; i <= n; i++)
		{
			Vec p = Lerp(a, b, (float)i / n);
			int x, y;
			Grid::Cell(p, x, y);
			if (!g.Open(x, y))
				return false;
		}
		return true;
	}

	bool FindPath(Vec theFrom, Vec theTo, std::vector<Vec>& thePath)
	{
		thePath.clear();
		const Grid& g = TheGrid();
		theTo = NearestOpen(theTo);
		Vec aStart = NearestOpen(theFrom);
		if (OpenLine(aStart, theTo))
		{
			thePath.push_back(theTo);
			return true;
		}
		int sx, sy, tx, ty;
		Grid::Cell(aStart, sx, sy);
		Grid::Cell(theTo, tx, ty);
		const int N = kGW * kGH;
		std::vector<float> aCost(N, 1e30f);
		std::vector<int> aFrom(N, -1);
		std::vector<uint8_t> aDone(N, 0);
		typedef std::pair<float, int> Item;
		std::priority_queue<Item, std::vector<Item>, std::greater<Item>> q;
		int s = sy * kGW + sx, t = ty * kGW + tx;
		aCost[s] = 0;
		q.push({ 0, s });
		auto H = [&](int c) { return std::hypot((float)(c % kGW - tx), (float)(c / kGW - ty)); };
		while (!q.empty())
		{
			int c = q.top().second;
			q.pop();
			if (aDone[c])
				continue;
			aDone[c] = 1;
			if (c == t)
				break;
			int x = c % kGW, y = c / kGW;
			for (int dy = -1; dy <= 1; dy++)
				for (int dx = -1; dx <= 1; dx++)
				{
					if ((dx == 0 && dy == 0) || !g.Open(x + dx, y + dy))
						continue;
					if (dx != 0 && dy != 0 && (!g.Open(x + dx, y) || !g.Open(x, y + dy)))
						continue;	// no corner cutting
					int n = (y + dy) * kGW + (x + dx);
					float aStep = (dx != 0 && dy != 0) ? 1.4142f : 1.0f;
					if (aCost[c] + aStep < aCost[n])
					{
						aCost[n] = aCost[c] + aStep;
						aFrom[n] = c;
						q.push({ aCost[n] + H(n), n });
					}
				}
		}
		if (!aDone[t])
			return false;
		std::vector<Vec> aCells;
		for (int c = t; c != -1 && c != s; c = aFrom[c])
			aCells.push_back(Grid::Center(c % kGW, c / kGW));
		std::reverse(aCells.begin(), aCells.end());
		if (!aCells.empty())
			aCells.back() = theTo;
		else
			aCells.push_back(theTo);
		// String-pull: skip every point that can be seen past.
		Vec aAt = aStart;
		size_t i = 0;
		while (i < aCells.size())
		{
			size_t j = aCells.size() - 1;
			while (j > i && !OpenLine(aAt, aCells[j]))
				j--;
			thePath.push_back(aCells[j]);
			aAt = aCells[j];
			i = j + 1;
		}
		return true;
	}
}
