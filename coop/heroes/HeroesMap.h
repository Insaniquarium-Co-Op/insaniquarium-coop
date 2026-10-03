// Pet Heroes - questions about the (static) layout of a tank or the Trench: the floor,
// where swimmers may be, line of sight, kelp, and paths around the walls. Every query
// names the arena (0/1: a team's tank, kTrench: the Trench).

#ifndef __HEROES_MAP_H__
#define __HEROES_MAP_H__

#include "HeroesData.h"

namespace Heroes
{
	float	FloorY(int theArena, float x);						// the floor surface at x
	bool	SwimmerFits(int theArena, Vec p, float r);			// in the water, clear of walls
	Vec		ClampToWater(int theArena, Vec p, float r);			// into the water (ignores walls)
	Vec		WalkerPos(int theArena, float x, float r);			// standing on the floor at x
	bool	WallBetween(int theArena, Vec a, Vec b);			// a wall polygon crosses a-b
	bool	FloorBetween(int theArena, Vec a, Vec b);			// the floor rises between a and b
	bool	LineOfSight(int theArena, Vec a, Vec b);			// neither of the above
	float	FirstBlock(int theArena, Vec a, Vec b);				// fraction along a-b of the first wall/floor hit (>1: none)
	int		KelpAt(int theArena, Vec p);						// index of the kelp patch containing p, or -1

	// Paths for swimmers of radius up to kPathRadius, around the walls.
	static const float kPathRadius = 36;
	static const float kPathCell = 16;
	bool	FindPath(int theArena, Vec theFrom, Vec theTo, std::vector<Vec>& thePath);	// false: no path (thePath empty)
	Vec		NearestOpen(int theArena, Vec p);					// the closest spot a hero can reach
	bool	OpenLine(int theArena, Vec a, Vec b);				// a hero can swim straight from a to b
}

#endif
