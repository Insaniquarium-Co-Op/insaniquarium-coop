// Pet Heroes - questions about the (static) arena layout: the floor, where swimmers
// may be, line of sight, kelp, and paths around the walls.

#ifndef __HEROES_MAP_H__
#define __HEROES_MAP_H__

#include "HeroesData.h"

namespace Heroes
{
	float	FloorY(float x);						// the floor surface at x
	bool	SwimmerFits(Vec p, float r);			// in the water, clear of walls
	Vec		ClampToWater(Vec p, float r);			// into the water (ignores walls)
	Vec		WalkerPos(float x, float r);			// standing on the floor at x
	bool	WallBetween(Vec a, Vec b);				// a wall polygon crosses a-b
	bool	FloorBetween(Vec a, Vec b);				// the floor rises between a and b
	bool	LineOfSight(Vec a, Vec b);				// neither of the above
	float	FirstBlock(Vec a, Vec b);				// fraction along a-b of the first wall/floor hit (>1: none)
	int		KelpAt(Vec p);							// index of the kelp patch containing p, or -1

	// Paths for swimmers of radius up to kPathRadius, around the walls.
	static const float kPathRadius = 36;
	static const float kPathCell = 16;
	bool	FindPath(Vec theFrom, Vec theTo, std::vector<Vec>& thePath);	// false: no path (thePath empty)
	Vec		NearestOpen(Vec p);						// the closest spot a hero can reach
	bool	OpenLine(Vec a, Vec b);					// a hero can swim straight from a to b
}

#endif
