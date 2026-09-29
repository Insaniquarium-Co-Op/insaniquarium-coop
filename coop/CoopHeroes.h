// Insaniquarium Co-op - Pet Heroes: a two-tank MOBA (docs/PET_HEROES.md).
//
// The simulation lives in coop/heroes/ (plain C++, also built into the headless
// tests). This layer puts it on screen: the Pet Heroes screen (draft, countdown,
// match, result), drawing, input, practice against the bot on one computer, and a
// network match where each computer runs its own side and the Session carries the
// sides' messages (MSG_HEROES_*).

#ifndef __COOP_HEROES_H__
#define __COOP_HEROES_H__

#include <cstdint>
#include <string>

namespace Coop
{
	class ByteReader;

	bool	HeroesBusy();								// the Pet Heroes screen is up
	bool	HeroesNetworkBusy();						// ...for a match with the partner (not practice)
	void	HeroesOpenPractice();						// you against the bot, no partner needed
	bool	HeroesCanStart(std::string& theWhy);		// host: a network match can start
	void	HeroesHostStart();							// host, from the Versus screen
	void	HeroesHandleMessage(uint8_t theType, ByteReader& r);
	void	HeroesPeerLost(const std::string& theWhy);
	void	HeroesUpdate();								// once per frame

	// Test harness.
	std::string HeroesDebugState();
	bool	HeroesTestPick(int theHero);				// practice: pick and start
	bool	HeroesTestLock(int theHero);				// network: pick and lock in
	void	HeroesTestBot(bool theOn);					// the bot plays my side too
	void	HeroesTestBotHero(int theHero);				// practice: the bot's hero (-1 random)
	void	HeroesTestSpeed(int theTicksPerFrame);		// fast-forward (practice only)
	void	HeroesTestGiveUp();
}

#endif
