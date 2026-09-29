// Insaniquarium Co-op - gameplay rules that change when two people share a tank.

#ifndef __COOP_GAME_H__
#define __COOP_GAME_H__

namespace Sexy { class Board; class GameObject; class Graphics; }

namespace Coop
{
	// All of these return the original behaviour unless a guest is connected.
	double	AlienHealthMult();
	int		ScaleAlienDelay(int theTicks);
	int		HungerExtraPeriod();		// 0 = none, N = one extra hunger tick every N updates
	void	ApplyLevelPrices(Sexy::Board* theBoard);
	void	MaybeSpawnBonusAlien(Sexy::Board* theBoard);

	void	OnLevelStarted(Sexy::Board* theBoard);
	void	OnLevelWon(Sexy::Board* theBoard);
	void	OnTimeUp(Sexy::Board* theBoard);			// Time Trial clock ran out
	void	OnTankLost(Sexy::Board* theBoard);			// every fish died
	void	OnBoardDestroyed(Sexy::Board* theBoard);
	void	OnCoinCollected(int theValue);

	///////////////////////////////////////////////////////////////////////////
	// Coin Rivals: same tank, separate wallets, first to finish the egg wins.
	//
	// The board keeps a single mMoney and egg counter. While rivals is running
	// they hold the wallet of the "actor" (0 = host, 1 = guest), and a
	// RivalsScope swaps the other player's wallet in while that player acts
	// (their clicks, their held mouse button, a coin they picked up arriving).
	///////////////////////////////////////////////////////////////////////////
	bool	RivalsActive();
	int		RivalsActor();
	int		RivalsWallet(int thePlayer);
	int		RivalsEggPieces(int thePlayer);		// 0..3

	class RivalsScope
	{
	public:
		explicit RivalsScope(int thePlayer);
		~RivalsScope();
	private:
		int		mPrev = 0;
		bool	mOn = false;
	};

	int		RivalsSpawnOwner();						// owner for an object entering the tank
	bool	RivalsReceive(Sexy::Board* theBoard, Sexy::GameObject* theFrom, int theValue);	// true = credited
	bool	RivalsCountsInFlight(Sexy::GameObject* theObj);	// for the board's "money on its way" sum
	void	RivalsFlushInFlight(Sexy::Board* theBoard);
	bool	RivalsCoinLocked(Sexy::GameObject* theCoin, int thePlayer);
	void	RivalsOnAlienShot(Sexy::GameObject* theAlien, int thePlayer);
	void	RivalsBuyFailed();
	void	RivalsGuestJoined();
	void	RivalsGuestLeft();
	void	DrawRivalsOverlay(Sexy::Graphics* g);
}

#endif
