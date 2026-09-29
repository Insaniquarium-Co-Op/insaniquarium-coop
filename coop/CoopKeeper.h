// Insaniquarium Co-op - Alien Keeper: one player keeps the fish tank, the other
// raises aliens in their own tank (the lair, in the aliens' home dimension) and
// sends them through a portal to invade; roles swap every round.
//
// Rounds reuse Tank Race's lifecycle (CoopRace). The fish keeper plays an ordinary
// Board; the alien keeper plays the LairTank screen here. The fish keeper's tank is
// streamed to the alien keeper (CoopSession) for the Tab live view. Lair actions
// travel as MSG_KEEPER_* messages and the fish keeper's computer carries them out.

#ifndef __COOP_KEEPER_H__
#define __COOP_KEEPER_H__

#include <cstdint>
#include <string>

namespace Sexy { class Board; class Graphics; class GameObject; }

namespace Coop
{
	class ByteReader;

	enum KeeperAlien { KA_SYLV, KA_BIGSYLV, KA_GUS, KA_BALROG, KA_DESTRUCTOR, KA_SQUID, KA_COUNT };

	// ---- lifecycle (CoopRace) ----
	void	KeeperBegin(bool theFishKeeper);
	void	KeeperEnd();
	void	KeeperUpdate();
	void	KeeperHandleMessage(uint8_t theType, ByteReader& r);
	void	KeeperDrawShared(Sexy::Graphics* g);	// fish keeper: streamed along with the tank
	void	KeeperDrawLocal(Sexy::Graphics* g);		// fish keeper: this screen only (the blackout)
	std::string KeeperResultLine();
	void	KeeperAlienRemoved(Sexy::GameObject* theAlien);

	// ---- game hooks (the fish keeper's tank) ----
	void	KeeperHuntPoint(int& theX, int& theY);				// aliens chase the fish nearest this point
	double	KeeperAlienSpeed(Sexy::GameObject* theAlien);		// frenzy
	bool	KeeperAlienShielded(Sexy::GameObject* theAlien);	// shield: lasers bounce off
	void	KeeperAlienAte(Sexy::GameObject* theFish);

	// ---- test harness ----
	bool	KeeperTestBuy(int theKind);
	void	KeeperTestFeed();						// a goo pellet above every alien
	void	KeeperTestGrow();						// every alien grown up at once
	void	KeeperTestCollect();					// pick up every crystal
	bool	KeeperTestSend(int theCount, int theX, int theY);	// theX < 0: random landing
	bool	KeeperTestBuff(int theBuff);			// 0 shield, 1 frenzy, on every selected alien
	void	KeeperTestSelectGrown(int theCount);
	bool	KeeperTestBlackout();
	void	KeeperTestHunt(bool theOn, int theX, int theY);
	void	KeeperTestSetMoney(int theMoney);
	void	KeeperTestBot(int theWave);				// an average lair player, for balance runs
	std::string KeeperDebugState();
}

#endif
