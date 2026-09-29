// Insaniquarium Co-op - Tank Race: each player runs their own tank on the same
// level, sends sabotage to the other, and the first to finish the egg wins.
//
// Unlike co-op, both computers simulate their own game. The connection only
// carries small messages: setup/ready/go, a 5-per-second status snapshot
// (money, egg pieces, fish positions for the rival's mini-map), attacks and
// the result. The host decides who won.

#ifndef __COOP_RACE_H__
#define __COOP_RACE_H__

#include "CoopProtocol.h"
#include <string>

namespace Sexy { class Board; class Graphics; class GameObject; }

namespace Coop
{
	enum RaceAttack
	{
		ATK_ALIEN = 0,		// an alien warps into the rival's tank
		ATK_HUNGER,			// all their guppies get hungry
		ATK_MURK,			// their water goes murky for 10 s
		ATK_THIEF,			// a mini alien swims in and eats their coins
		ATK_HIKE,			// their shop prices +30% for 20 s
		ATK_POISON,			// a row of poison potions falls across their tank
		ATK_BLOCK,			// they can't collect any money for 10 s
		ATK_COUNT
	};

	// ---- settings (host's choice, remembered) ----
	int		RaceTank();
	int		RaceLevel();
	bool	RaceCatchUp();
	void	SetRaceTank(int theTank);
	void	SetRaceLevel(int theLevel);
	void	SetRaceCatchUp(bool theOn);
	bool	VersusIsKeeper();					// the Versus menu's mode: Alien Keeper, else Tank Race
	void	SetVersusKeeper(bool theKeeper);
	// Stage pets (Versus): -1 = ask the profile, else 0/1; counts fall back to the profile's.
	int		RaceStagePetOverride(int thePet);
	int		RaceStagePetCount(int theProfileCount);
	int		RaceStagePetSlots(int theProfileSlots);
	int		RaceWins(int thePlayer);			// this session's scoreboard

	// ---- lifecycle ----
	bool	RaceBusy();							// setting up, racing or showing the result
	bool	RaceHidesToasts();					// countdown and result screens own the middle
	bool	RaceCanStart(std::string& theWhy);	// host
	void	RaceHostStart();
	void	RaceHostCancel();
	void	RaceHandleMessage(uint8_t theType, ByteReader& r);
	void	RacePeerLost(const std::string& theWhy);
	void	RaceUpdate();						// once per frame

	// ---- game hooks ----
	bool	RaceOwnsBoard(Sexy::Board* theBoard);	// a race tank: never saved
	void	RaceOverrideLevel(int& theTank, int& theLevel);
	void	RaceOnGameStarted();
	bool	RaceOnEggComplete(Sexy::Board* theBoard);
	bool	RaceOnTankLost(Sexy::Board* theBoard);
	void	RaceOnBoardDestroyed(Sexy::Board* theBoard);
	void	RaceOnAlienRemoved(Sexy::GameObject* theAlien);
	int		RaceAdjustCost(Sexy::Board* theBoard, int theCost);
	bool	RaceKeyDown(int theKey);
	bool	RaceCoinsBlocked();						// coin blocker: no collecting money right now
	void	RacePoisonClicked(Sexy::GameObject* thePotion);	// the victim clicked a poison potion away
	void	DrawRaceOverlay(Sexy::Graphics* g);
	void	DrawRaceLocalOverlay(Sexy::Graphics* g);	// after the stream frame: this screen only

	// ---- Alien Keeper rounds (CoopKeeper uses these) ----
	bool	RaceIsKeeper();						// this round is Alien Keeper
	bool	RaceIAmFishKeeper();
	int		RaceFishKeeper();					// 0 = host, 1 = guest
	Sexy::Board* RaceTankBoard();				// my tank this round, or null
	bool	RaceRunning();
	uint32_t RaceRunningMs();					// since GO
	int		RaceRivalAliens();					// from the fish keeper's snapshots
	int		RaceRivalFish();
	int		RaceRivalMoney();
	int		RaceRivalEggs();
	int		RaceRoundTank();					// the tank (1-4) this round plays
	void	RaceDrawRivalDots(Sexy::Graphics* g, int x, int y, int w, int h);	// the rival's tank as a map
	void	RaceKeeperLairDefeated();			// alien keeper: no aliens left and no money for one
	void	RaceKeeperSurvived();				// fish keeper: the round clock ran out
	void	RaceKeeperForfeit();				// alien keeper gave up
	int		RaceKeeperNextFishKeeper();			// host: who keeps the fish next round

	// ---- test harness ----
	bool	RaceTestAttack(int theAttack);
	void	RaceTestReceive(int theAttack, int thePrice);	// as if the rival sent it
	std::string RaceDebugState();
}

#endif
