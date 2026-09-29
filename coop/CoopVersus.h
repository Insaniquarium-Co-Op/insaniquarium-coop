// Insaniquarium Co-op - the multiplayer panel on the main menu (Co-op / Coin Rivals
// for Adventure and Time Trial, and the Versus button) and the Versus screen (Tank
// Race or Alien Keeper on Tank 1-4). Both belong to the host; player 2 watches.

#ifndef __COOP_VERSUS_H__
#define __COOP_VERSUS_H__

namespace Sexy { class WidgetManager; class Widget; class WinFishApp; }

namespace Coop
{
	// Called by the game selector as it comes and goes.
	void	AttachMenuPanel(Sexy::WidgetManager* theManager);
	void	DetachMenuPanel(Sexy::WidgetManager* theManager);
	void	MenuPanelToFront(Sexy::WidgetManager* theManager, Sexy::Widget* theSelector);

	void	OpenVersusDialog(Sexy::WinFishApp* theApp);
}

#endif
