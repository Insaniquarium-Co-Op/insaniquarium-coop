// Pet Heroes - small geometry helpers and a random number generator.
// Plain C++: the simulation builds without the framework (tests/heroes).

#ifndef __HEROES_MATH_H__
#define __HEROES_MATH_H__

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Heroes
{
	struct Vec
	{
		float x = 0, y = 0;
		Vec() {}
		Vec(float theX, float theY) : x(theX), y(theY) {}
		Vec operator+(const Vec& o) const { return Vec(x + o.x, y + o.y); }
		Vec operator-(const Vec& o) const { return Vec(x - o.x, y - o.y); }
		Vec operator*(float s) const { return Vec(x * s, y * s); }
		Vec& operator+=(const Vec& o) { x += o.x; y += o.y; return *this; }
		Vec& operator-=(const Vec& o) { x -= o.x; y -= o.y; return *this; }
		bool operator==(const Vec& o) const { return x == o.x && y == o.y; }
	};

	inline float Dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y; }
	inline float Cross(Vec a, Vec b) { return a.x * b.y - a.y * b.x; }
	inline float Len(Vec a) { return std::sqrt(a.x * a.x + a.y * a.y); }
	inline float Dist(Vec a, Vec b) { return Len(a - b); }
	inline float Dist2(Vec a, Vec b) { Vec d = a - b; return d.x * d.x + d.y * d.y; }
	inline Vec Norm(Vec a) { float l = Len(a); return l > 1e-6f ? a * (1.0f / l) : Vec(0, 0); }
	inline Vec Lerp(Vec a, Vec b, float t) { return a + (b - a) * t; }
	inline float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

	// Moves a toward b by at most theStep; returns true when it arrives.
	inline bool StepToward(Vec& a, Vec b, float theStep)
	{
		Vec d = b - a;
		float l = Len(d);
		if (l <= theStep || l < 1e-4f)
		{
			a = b;
			return true;
		}
		a += d * (theStep / l);
		return false;
	}

	inline float DistPointSeg(Vec p, Vec a, Vec b)
	{
		Vec ab = b - a;
		float l2 = Dot(ab, ab);
		float t = l2 > 1e-6f ? Clamp(Dot(p - a, ab) / l2, 0, 1) : 0;
		return Dist(p, a + ab * t);
	}

	// Segment a-b against segment c-d; theT is the fraction along a-b.
	inline bool SegSeg(Vec a, Vec b, Vec c, Vec d, float* theT)
	{
		Vec r = b - a, s = d - c;
		float den = Cross(r, s);
		if (std::fabs(den) < 1e-6f)
			return false;
		float t = Cross(c - a, s) / den;
		float u = Cross(c - a, r) / den;
		if (t < 0 || t > 1 || u < 0 || u > 1)
			return false;
		if (theT != nullptr)
			*theT = t;
		return true;
	}

	// First point where segment a-b enters a circle (theT = fraction along a-b).
	inline bool SegCircle(Vec a, Vec b, Vec c, float r, float* theT)
	{
		Vec d = b - a, f = a - c;
		float A = Dot(d, d), B = 2 * Dot(f, d), C = Dot(f, f) - r * r;
		if (C <= 0)
		{
			if (theT != nullptr)
				*theT = 0;
			return true;
		}
		if (A < 1e-6f)
			return false;
		float disc = B * B - 4 * A * C;
		if (disc < 0)
			return false;
		float t = (-B - std::sqrt(disc)) / (2 * A);
		if (t < 0 || t > 1)
			return false;
		if (theT != nullptr)
			*theT = t;
		return true;
	}

	inline bool PointInPoly(const std::vector<Vec>& thePoly, Vec p)
	{
		bool in = false;
		for (size_t i = 0, j = thePoly.size() - 1; i < thePoly.size(); j = i++)
		{
			const Vec& a = thePoly[i];
			const Vec& b = thePoly[j];
			if (((a.y > p.y) != (b.y > p.y)) && (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x))
				in = !in;
		}
		return in;
	}

	// Segment a-b against a polygon's edges (or starting inside it).
	inline bool SegPoly(Vec a, Vec b, const std::vector<Vec>& thePoly, float* theT)
	{
		if (thePoly.size() < 3)
			return false;
		if (PointInPoly(thePoly, a))
		{
			if (theT != nullptr)
				*theT = 0;
			return true;
		}
		float aBest = 2;
		for (size_t i = 0, j = thePoly.size() - 1; i < thePoly.size(); j = i++)
		{
			float t;
			if (SegSeg(a, b, thePoly[j], thePoly[i], &t) && t < aBest)
				aBest = t;
		}
		if (aBest > 1)
			return false;
		if (theT != nullptr)
			*theT = aBest;
		return true;
	}

	inline float DistPointPoly(const std::vector<Vec>& thePoly, Vec p)
	{
		if (PointInPoly(thePoly, p))
			return 0;
		float aBest = 1e9f;
		for (size_t i = 0, j = thePoly.size() - 1; i < thePoly.size(); j = i++)
			aBest = std::min(aBest, DistPointSeg(p, thePoly[j], thePoly[i]));
		return aBest;
	}

	// xorshift64*: fast, good enough, and the same everywhere.
	struct Rng
	{
		uint64_t mState = 0x9E3779B97F4A7C15ull;
		explicit Rng(uint64_t theSeed = 1) { Seed(theSeed); }
		void Seed(uint64_t theSeed) { mState = theSeed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull; if (mState == 0) mState = 1; }
		uint32_t Next()
		{
			mState ^= mState >> 12;
			mState ^= mState << 25;
			mState ^= mState >> 27;
			return (uint32_t)((mState * 0x2545F4914F6CDD1Dull) >> 32);
		}
		int Int(int theN) { return theN <= 0 ? 0 : (int)(Next() % (uint32_t)theN); }
		float Float() { return (Next() >> 8) * (1.0f / 16777216.0f); }
		float Range(float lo, float hi) { return lo + (hi - lo) * Float(); }
		bool Chance(float p) { return Float() < p; }
	};
}

#endif
